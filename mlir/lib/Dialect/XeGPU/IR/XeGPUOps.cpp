//===- XeGPUOps.cpp - MLIR XeGPU ops implementation -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Arith/Utils/Utils.h"
#include "mlir/Dialect/Utils/StaticValueUtils.h"
#include "mlir/Dialect/XeGPU/IR/XeGPU.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/TypeUtilities.h"

#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "xegpu"

namespace mlir {
namespace xegpu {

static void transpose(llvm::ArrayRef<int64_t> trans,
                      SmallVector<int64_t> &shape) {
  SmallVector<int64_t> old = shape;
  for (size_t i = 0; i < trans.size(); i++)
    shape[i] = old[trans[i]];
}

template <typename T>
static std::string makeString(T array, bool breakline = false) {
  std::string buf;
  buf.clear();
  llvm::raw_string_ostream os(buf);
  os << "[";
  for (size_t i = 1; i < array.size(); i++) {
    os << array[i - 1] << ", ";
    if (breakline)
      os << "\n\t\t";
  }
  os << array.back() << "]";
  return buf;
}

static SmallVector<int64_t> getShapeOf(Type type) {
  SmallVector<int64_t> shape;
  if (auto ty = llvm::dyn_cast<ShapedType>(type))
    shape = SmallVector<int64_t>(ty.getShape());
  else
    shape.push_back(1);
  return shape;
}

static int64_t getRankOf(Value val) {
  auto type = val.getType();
  if (auto ty = llvm::dyn_cast<ShapedType>(type))
    return ty.getRank();
  return 0;
}

static bool isReadHintOrNone(const CachePolicyAttr &attr) {
  if (!attr)
    return true;
  auto kind = attr.getValue();
  return kind == CachePolicy::CACHED || kind == CachePolicy::UNCACHED ||
         kind == CachePolicy::STREAMING || kind == CachePolicy::READ_INVALIDATE;
}

static bool isWriteHintOrNone(const CachePolicyAttr &attr) {
  if (!attr)
    return true;
  auto kind = attr.getValue();
  return kind == CachePolicy::CACHED || kind == CachePolicy::UNCACHED ||
         kind == CachePolicy::WRITE_BACK || kind == CachePolicy::WRITE_THROUGH;
}

// Validations for nd instruction arguments is successful if any of these are
// true:
// - tensor descriptor and the output vector shapes exactly match.
// - tensor descriptor has a sg_map attribute and the distributed vector shape
//   matches the tensor descriptor shape when scaled using sg_map factors on
//   each dimension.
static bool isArgShapesValid(ArrayRef<int64_t> descShape,
                             ArrayRef<int64_t> valShape, SGMapAttr sgMap) {
  // Equal shapes with no distribution - no further verification needed.
  if (descShape == valShape && !sgMap)
    return true;

  // Unknown distribution - cannot perform operation on partial shape.
  if (!sgMap)
    return false;

  // Invalid rank or mixed rank usage.
  size_t descRank = descShape.size();
  if (descRank > 2 || valShape.size() != descRank)
    return false;

  // For 1D, SG map is guaranteed to be unit size in the outer dimension.
  // Only take the distribution over the innermost dimension for validation.
  ArrayRef<uint32_t> wiLayout = sgMap.getWiLayout();
  SmallVector<uint32_t> mapLayout(wiLayout.begin(), wiLayout.end());
  if (descRank == 1)
    mapLayout = {wiLayout.back()};

  for (const auto &[factor, dim, expected] :
       llvm::zip_equal(mapLayout, valShape, descShape)) {
    if (factor * dim != expected)
      return false;
  }

  return true;
}

//===----------------------------------------------------------------------===//
// XeGPU_CreateNdDescOp
//===----------------------------------------------------------------------===//
void CreateNdDescOp::build(OpBuilder &builder, OperationState &state,
                           Type tdesc, TypedValue<MemRefType> source,
                           llvm::ArrayRef<OpFoldResult> offsets) {
  [[maybe_unused]] auto ty = source.getType();
  assert(ty.hasStaticShape() && offsets.size() == (size_t)ty.getRank());

  llvm::SmallVector<int64_t> staticOffsets;
  llvm::SmallVector<Value> dynamicOffsets;
  dispatchIndexOpFoldResults(offsets, dynamicOffsets, staticOffsets);

  build(builder, state, tdesc, source, dynamicOffsets /* dynamic offsets */,
        ValueRange({}) /* empty dynamic shape */,
        ValueRange({}) /* empty dynamic strides */,
        staticOffsets /* const offsets */, {} /* empty const shape*/,
        {} /* empty const strides*/);
}

void CreateNdDescOp::build(OpBuilder &builder, OperationState &state,
                           Type tdesc, TypedValue<MemRefType> source,
                           llvm::ArrayRef<OpFoldResult> offsets,
                           llvm::ArrayRef<OpFoldResult> shape,
                           llvm::ArrayRef<OpFoldResult> strides) {
  assert(shape.size() && offsets.size() && strides.size() &&
         shape.size() == strides.size() && shape.size() == offsets.size());

  llvm::SmallVector<int64_t> staticOffsets;
  llvm::SmallVector<int64_t> staticShape;
  llvm::SmallVector<int64_t> staticStrides;
  llvm::SmallVector<Value> dynamicOffsets;
  llvm::SmallVector<Value> dynamicShape;
  llvm::SmallVector<Value> dynamicStrides;

  dispatchIndexOpFoldResults(offsets, dynamicOffsets, staticOffsets);
  dispatchIndexOpFoldResults(shape, dynamicShape, staticShape);
  dispatchIndexOpFoldResults(strides, dynamicStrides, staticStrides);

  auto staticOffsetsAttr = builder.getDenseI64ArrayAttr(staticOffsets);
  auto staticShapeAttr = builder.getDenseI64ArrayAttr(staticShape);
  auto staticStridesAttr = builder.getDenseI64ArrayAttr(staticStrides);

  build(builder, state, tdesc, source, dynamicOffsets, dynamicShape,
        dynamicStrides, staticOffsetsAttr, staticShapeAttr, staticStridesAttr);
}

void CreateNdDescOp::build(OpBuilder &builder, OperationState &state,
                           Type tdesc, TypedValue<IntegerType> source,
                           llvm::ArrayRef<OpFoldResult> offsets,
                           llvm::ArrayRef<OpFoldResult> shape,
                           llvm::ArrayRef<OpFoldResult> strides) {
  assert(shape.size() && offsets.size() && strides.size() &&
         shape.size() == strides.size() && shape.size() == offsets.size());

  llvm::SmallVector<int64_t> staticOffsets;
  llvm::SmallVector<int64_t> staticShape;
  llvm::SmallVector<int64_t> staticStrides;
  llvm::SmallVector<Value> dynamicOffsets;
  llvm::SmallVector<Value> dynamicShape;
  llvm::SmallVector<Value> dynamicStrides;

  dispatchIndexOpFoldResults(offsets, dynamicOffsets, staticOffsets);
  dispatchIndexOpFoldResults(shape, dynamicShape, staticShape);
  dispatchIndexOpFoldResults(strides, dynamicStrides, staticStrides);

  auto staticOffsetsAttr = builder.getDenseI64ArrayAttr(staticOffsets);
  auto staticShapeAttr = builder.getDenseI64ArrayAttr(staticShape);
  auto staticStridesAttr = builder.getDenseI64ArrayAttr(staticStrides);

  build(builder, state, tdesc, source, dynamicOffsets, dynamicShape,
        dynamicStrides, staticOffsetsAttr, staticShapeAttr, staticStridesAttr);
}

LogicalResult CreateNdDescOp::verify() {
  auto rank = (int64_t)getMixedOffsets().size();
  bool invalidRank = false;
  bool invalidElemTy = false;

  // Memory space of created TensorDesc should match with the source.
  // Both source and TensorDesc are considered for global memory by default,
  // if the memory scope attr is not specified. If source is an integer,
  // it is considered as ptr to global memory.
  auto srcMemorySpace = getSourceMemorySpace();
  auto tdescMemorySpace = static_cast<unsigned>(getType().getMemorySpace());
  if (srcMemorySpace != tdescMemorySpace)
    return emitOpError("Memory space mismatch.")
           << " Source: " << srcMemorySpace
           << ", TensorDesc: " << tdescMemorySpace;

  // check source type matches the rank if it is a memref.
  // It also should have the same ElementType as TensorDesc.
  auto memrefTy = dyn_cast<MemRefType>(getSourceType());
  if (memrefTy) {
    invalidRank |= (memrefTy.getRank() != rank);
    invalidElemTy |= memrefTy.getElementType() != getElementType();
  }

  // mismatches among shape, strides, and offsets are
  // already handeled by OffsetSizeAndStrideOpInterface.
  // So they are not check here.
  if (invalidRank)
    return emitOpError(
        "Expecting the rank of shape, strides, offsets, and source (if source "
        "is a memref) should match with each other.");

  // check result TensorDesc rank
  invalidRank = (getType().getRank() > 2 || getType().getRank() > rank);

  if (invalidRank)
    return emitOpError(
        "Expecting the TensorDesc rank is up to 2 and not greater than the "
        "ranks of shape, strides, offsets or the memref source.");

  if (invalidElemTy)
    return emitOpError("TensorDesc should have the same element "
                       "type with the source if it is a memref.\n");

  if (getType().isScattered())
    return emitOpError("Expects a non-scattered TensorDesc.\n");

  return success();
}

//===----------------------------------------------------------------------===//
// XeGPU_PrefetchNdOp
//===----------------------------------------------------------------------===//
LogicalResult PrefetchNdOp::verify() {
  auto tdescTy = getTensorDescType();
  if (tdescTy.isScattered())
    return emitOpError("Expects a non-scattered TensorDesc.\n");

  if (!isReadHintOrNone(getL1HintAttr()))
    return emitOpError("invalid l1_hint: ") << getL1HintAttr();

  if (!isReadHintOrNone(getL2HintAttr()))
    return emitOpError("invalid l2_hint: ") << getL2HintAttr();

  if (!isReadHintOrNone(getL3HintAttr()))
    return emitOpError("invalid l3_hint: ") << getL3HintAttr();

  return success();
}

//===----------------------------------------------------------------------===//
// XeGPU_LoadNdOp
//===----------------------------------------------------------------------===//
LogicalResult LoadNdOp::verify() {
  auto tdescTy = getTensorDescType();
  auto valueTy = getType();

  if (tdescTy.getRank() > 2)
    return emitOpError("Expecting a 1D/2D TensorDesc.\n");

  if (tdescTy.isScattered())
    return emitOpError("Expects a non-scattered TensorDesc.\n");

  if (!valueTy)
    return emitOpError("Invalid result, it should be a VectorType.\n");

  if (!isReadHintOrNone(getL1HintAttr()))
    return emitOpError("invalid l1_hint: ") << getL1HintAttr();

  if (!isReadHintOrNone(getL2HintAttr()))
    return emitOpError("invalid l2_hint: ") << getL2HintAttr();

  if (!isReadHintOrNone(getL3HintAttr()))
    return emitOpError("invalid l3_hint: ") << getL3HintAttr();

  auto array_len = tdescTy.getArrayLength();
  auto tdescShape = getShapeOf(tdescTy);
  auto valueShape = getShapeOf(valueTy);

  if (getTranspose()) {
    auto trans = getTranspose().value();

    // Make sure the transpose value is valid.
    bool valid = std::all_of(trans.begin(), trans.end(), [&](int t) {
      return t >= 0 && t < tdescTy.getRank();
    });

    if (valid)
      transpose(trans, tdescShape);
    else
      mlir::emitWarning(getLoc()) << "Invalid transpose attr. It is ignored.";
  }

  if (getPacked()) {
    if (tdescTy.getRank() == 2) {
      const int axis = 0;
      auto vnni_factor = valueShape.back();
      tdescShape[axis] /= vnni_factor;
      tdescShape.push_back(vnni_factor);
    } else {
      mlir::emitWarning(getLoc())
          << "Invalid Packed Attr. It is ignored (available for 2D "
             "TensorDesc only).";
    }
  }

  if (array_len > 1) {
    auto it = tdescShape.begin();
    tdescShape.insert(it, array_len);
  }
  auto sgMap = tdescTy.getSGMapAttr();

  if (!isArgShapesValid(tdescShape, valueShape, sgMap))
    return emitOpError() << "Result shape doesn't match TensorDesc shape."
                         << "The expected shape is " << makeString(tdescShape)
                         << ". But the given shape is "
                         << makeString(valueShape) << ".\n";
  return success();
}

//===----------------------------------------------------------------------===//
// XeGPU_StoreNdOp
//===----------------------------------------------------------------------===//
LogicalResult StoreNdOp::verify() {
  auto dstTy = getTensorDescType(); // Tile
  auto valTy = getValueType();      // Vector

  if (dstTy.getRank() > 2)
    return emitOpError("Expecting a 1D/2D TensorDesc.\n");

  if (dstTy.isScattered())
    return emitOpError("Expects a non-scattered TensorDesc.\n");

  if (!valTy)
    return emitOpError("Expecting a VectorType result.\n");

  if (!isWriteHintOrNone(getL1HintAttr()))
    return emitOpError("invalid l1_hint: ") << getL1HintAttr();

  if (!isWriteHintOrNone(getL2HintAttr()))
    return emitOpError("invalid l2_hint: ") << getL2HintAttr();

  if (!isWriteHintOrNone(getL3HintAttr()))
    return emitOpError("invalid l3_hint: ") << getL3HintAttr();

  auto tdescShape = getShapeOf(dstTy);
  auto valueShape = getShapeOf(valTy);
  auto sgMap = dstTy.getSGMapAttr();

  if (!isArgShapesValid(tdescShape, valueShape, sgMap))
    return emitOpError() << "Result shape doesn't match TensorDesc shape."
                         << "The expected shape is " << makeString(tdescShape)
                         << ". But the given shape is "
                         << makeString(valueShape) << ".\n";
  return success();
}

//===----------------------------------------------------------------------===//
// XeGPU_UpdateNDOffsetOp
//===----------------------------------------------------------------------===//
LogicalResult UpdateNdOffsetOp::verify() {
  auto ty = getTensorDescType();
  if (ty.isScattered())
    return emitOpError("Expects a non-scattered TensorDesc.\n");

  // number of offsets specified must match the rank of the tensor descriptor
  if (ty.getRank() != (int64_t)getNumOffsets()) {
    return emitOpError("Invalid number of offsets.");
  }
  return success();
}

//===----------------------------------------------------------------------===//
// XeGPU_CreateDescOp
//===----------------------------------------------------------------------===//

void CreateDescOp::build(OpBuilder &builder, OperationState &state,
                         TensorDescType TensorDesc, Value source,
                         llvm::ArrayRef<OpFoldResult> offsets) {
  auto loc = source.getLoc();
  int64_t size = static_cast<int64_t>(offsets.size());
  auto type = VectorType::get(size, builder.getIndexType());
  auto values = getValueOrCreateConstantIndexOp(builder, loc, offsets);
  auto offset = builder.create<vector::FromElementsOp>(loc, type, values);
  build(builder, state, TensorDesc, source, offset);
}

void CreateDescOp::build(OpBuilder &builder, OperationState &state,
                         TensorDescType TensorDesc, Value source,
                         llvm::ArrayRef<int64_t> offsets) {
  auto ofrs = getAsIndexOpFoldResult(builder.getContext(), offsets);
  build(builder, state, TensorDesc, source, ofrs);
}

LogicalResult CreateDescOp::verify() {
  auto tdescTy = getTensorDescType();

  if (getRankOf(getSource()) > 1)
    return emitOpError(
        "Expecting the source is a 1D memref or pointer (uint64_t).");

  if (!tdescTy.isScattered())
    return emitOpError("Expects a scattered TensorDesc.\n");

  // Memory space of created TensorDesc should match with the source.
  // Both source and TensorDesc are considered for global memory by default,
  // if the memory scope attr is not specified. If source is an integer,
  // it is considered as ptr to global memory.
  auto srcMemorySpace = getSourceMemorySpace();
  auto tdescMemorySpace = static_cast<unsigned>(tdescTy.getMemorySpace());
  if (srcMemorySpace != tdescMemorySpace)
    return emitOpError("Memory space mismatch.")
           << " Source: " << srcMemorySpace
           << ", TensorDesc: " << tdescMemorySpace;

  // check total size
  auto chunkSize = tdescTy.getChunkSize();
  auto elemBits = tdescTy.getElementType().getIntOrFloatBitWidth();
  auto bitsPerLane = elemBits * chunkSize;
  if (chunkSize > 1 && bitsPerLane % 32) {
    // For 8-bit and 16-bit data, the hardware only supports chunk size of 1.
    // For 32-bit data, the hardware can support larger larger chunk size. So
    // we can bitcast 8-bit/16-bit data to 32-bit data for better performance.
    // But this requires the total size is 32 bit aligned to make the
    // optimization work.
    return emitOpError(
        "access size (chunk_size * sizeof(elemTy)) should be 32-bit aligned.");
  }

  auto lscConstraints = 512 * 8; // each access is upto 512 bytes.
  if (elemBits * tdescTy.getNumElements() > lscConstraints)
    return emitOpError("total access size (simd_lanes * chunk_size * "
                       "sizeof(elemTy)) is upto 512 bytes.");

  SmallVector<int64_t> shape({(int64_t)getNumOffsets()});
  if (chunkSize != 1)
    shape.push_back(chunkSize);

  auto tdescShape = getShapeOf(tdescTy);
  if (shape != tdescShape)
    return emitOpError("Incorrect TensorDesc shape. ")
           << "Expected is " << makeString(shape) << "\n";

  return success();
}

//===----------------------------------------------------------------------===//
// XeGPU_PrefetchOp
//===----------------------------------------------------------------------===//
LogicalResult PrefetchOp::verify() {
  auto tdescTy = getTensorDescType();
  if (!tdescTy.isScattered())
    return emitOpError("Expects a scattered TensorDesc.\n");

  if (!isReadHintOrNone(getL1HintAttr()))
    return emitOpError("invalid l1_hint: ") << getL1HintAttr();

  if (!isReadHintOrNone(getL2HintAttr()))
    return emitOpError("invalid l2_hint: ") << getL2HintAttr();

  if (!isReadHintOrNone(getL3HintAttr()))
    return emitOpError("invalid l3_hint: ") << getL3HintAttr();

  return success();
}

//===----------------------------------------------------------------------===//
// XeGPU_LoadGatherOp
//===----------------------------------------------------------------------===//
LogicalResult LoadGatherOp::verify() {
  auto tdescTy = getTensorDescType();
  auto maskTy = getMaskType();
  auto valueTy = getValueType();

  if (!tdescTy.isScattered())
    return emitOpError("Expects a scattered TensorDesc.\n");

  if (!isReadHintOrNone(getL1HintAttr()))
    return emitOpError("invalid l1_hint: ") << getL1HintAttr();

  if (!isReadHintOrNone(getL2HintAttr()))
    return emitOpError("invalid l2_hint: ") << getL2HintAttr();

  if (!isReadHintOrNone(getL3HintAttr()))
    return emitOpError("invalid l3_hint: ") << getL3HintAttr();

  auto tdescElemTy = tdescTy.getElementType();
  auto valueElemTy = getElementType();
  if (tdescElemTy != valueElemTy)
    return emitOpError(
        "Value should have the same element type as TensorDesc.");

  auto maskShape = getShapeOf(maskTy);
  auto valueShape = getShapeOf(valueTy);
  auto tdescShape = getShapeOf(tdescTy);

  if (tdescShape[0] != maskShape[0])
    return emitOpError("dim-0 of the Mask and TensorDesc should be the same.");

  if (tdescTy.getRank() == 2) {
    if (!getTransposeAttr())
      return emitOpError("load of rank-2 tensor has to be transposed.");
    transpose({1, 0}, tdescShape);
  }

  if (auto sgMap = tdescTy.getSGMapAttr()) {
    auto valueVecTy = cast<VectorType>(valueTy);
    const int32_t wiData =
        sgMap.getWiData()[0] > 1 ? sgMap.getWiData()[0] : sgMap.getWiData()[1];
    // All represent the same concept: a number of row elements to store.
    if (valueVecTy.getNumElements() != wiData ||
        valueVecTy.getNumElements() != tdescTy.getChunkSize()) {
      return emitOpError("Chunk size, vector size and wi_data must match.");
    }
    // Work-item's slice (i.e., vector shape to load) is [1] or [1, chunk_size].
    tdescShape[tdescTy.getRank() - 1] = 1;
  }

  if (valueShape != tdescShape)
    return emitOpError("Unexpected result shape")
           << "(Expected shape: " << makeString(tdescShape)
           << ", Given shape: " << makeString(valueShape) << ").\n";

  return success();
}

//===----------------------------------------------------------------------===//
// XeGPU_StoreScatterOp
//===----------------------------------------------------------------------===//
LogicalResult StoreScatterOp::verify() {
  auto tdescTy = getTensorDescType();
  if (!tdescTy.isScattered())
    return emitOpError("Expects a scattered TensorDesc.\n");

  if (!isWriteHintOrNone(getL1HintAttr()))
    return emitOpError("invalid l1_hint: ") << getL1HintAttr();

  if (!isWriteHintOrNone(getL2HintAttr()))
    return emitOpError("invalid l2_hint: ") << getL2HintAttr();

  if (!isWriteHintOrNone(getL3HintAttr()))
    return emitOpError("invalid l3_hint: ") << getL3HintAttr();

  auto maskTy = getMaskType();
  auto valueTy = getValueType();
  auto maskShape = getShapeOf(maskTy);
  auto tdescShape = getShapeOf(tdescTy);
  auto valueShape = getShapeOf(valueTy);
  if (tdescShape[0] != maskShape[0])
    return emitOpError("dim-0 of the Mask and TensorDesc should be the same.");

  if (tdescTy.getRank() == 2) {
    if (!getTransposeAttr())
      return emitOpError("Store of a rank-2 tensor has to be transposed.");
    transpose({1, 0}, tdescShape);
  }

  if (auto sgMap = tdescTy.getSGMapAttr()) {
    auto valueVecTy = cast<VectorType>(valueTy);
    const int32_t wiData =
        sgMap.getWiData()[0] > 1 ? sgMap.getWiData()[0] : sgMap.getWiData()[1];
    // All represent the same concept: a number of row elements to store.
    if (valueVecTy.getNumElements() != wiData ||
        valueVecTy.getNumElements() != tdescTy.getChunkSize()) {
      return emitOpError("Chunk size, vector size and wi_data must match.");
    }
    // Work-item's slice (i.e., vector to store) is [1] or [1, chunk_size].
    tdescShape[tdescTy.getRank() - 1] = 1;
  }

  if (valueShape != tdescShape)
    return emitOpError("Unexpected value shape")
           << "(Expected shape: " << makeString(tdescShape)
           << ", Given shape: " << makeString(valueShape) << ").\n";

  return success();
}

//===----------------------------------------------------------------------===//
// XeGPU_UpdateOffsetOp
//===----------------------------------------------------------------------===//
void UpdateOffsetOp::build(OpBuilder &builder, OperationState &state,
                           mlir::Value tensorDesc,
                           llvm::ArrayRef<OpFoldResult> offsets) {
  auto tdescTy = mlir::dyn_cast<TensorDescType>(tensorDesc.getType());
  assert(tdescTy && "Expecting the source is a TensorDescType value.");
  auto loc = tensorDesc.getLoc();
  int64_t size = static_cast<int64_t>(offsets.size());
  auto type = VectorType::get({size}, builder.getIndexType());
  auto values = getValueOrCreateConstantIndexOp(builder, loc, offsets);
  auto offset = builder.create<vector::FromElementsOp>(loc, type, values);
  build(builder, state, tdescTy, tensorDesc, offset);
}

void UpdateOffsetOp::build(OpBuilder &builder, OperationState &state,
                           Value tensorDesc, llvm::ArrayRef<int64_t> offsets) {
  auto ofrs = getAsIndexOpFoldResult(builder.getContext(), offsets);
  build(builder, state, tensorDesc, ofrs);
}

//===----------------------------------------------------------------------===//
// XeGPU_DpasOp
//===----------------------------------------------------------------------===//
LogicalResult DpasOp::verify() {
  int64_t lhsRank = getLhsType().getRank();
  int64_t rhsRank = getRhsType().getRank();

  if (lhsRank != 2 || (rhsRank != 2 && rhsRank != 3))
    return emitOpError("expecting lhs to be a 2D vector, and rhs to be either "
                       "2D or 3D (packed) vector.");

  auto lhsShape = getLhsType().getShape();
  auto rhsShape = getRhsType().getShape();
  auto bK = rhsRank == 3 ? rhsShape[0] * rhsShape[2] : rhsShape[0];
  if (bK != lhsShape[1])
    return emitOpError("K-dimension mismatch.");

  return success();
}

} // namespace xegpu
} // namespace mlir

#include <mlir/Dialect/XeGPU/IR/XeGPUEnums.cpp.inc>
#define GET_OP_CLASSES
#include <mlir/Dialect/XeGPU/IR/XeGPU.cpp.inc>

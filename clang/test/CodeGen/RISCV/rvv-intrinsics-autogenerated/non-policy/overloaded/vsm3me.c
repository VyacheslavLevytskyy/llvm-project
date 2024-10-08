// NOTE: Assertions have been autogenerated by utils/update_cc_test_checks.py UTC_ARGS: --version 2
// REQUIRES: riscv-registered-target
// RUN: %clang_cc1 -triple riscv64 -target-feature +v -target-feature +zvl512b \
// RUN:   -target-feature +zvbb \
// RUN:   -target-feature +zvbc \
// RUN:   -target-feature +zvkb \
// RUN:   -target-feature +zvkg \
// RUN:   -target-feature +zvkned \
// RUN:   -target-feature +zvknhb \
// RUN:   -target-feature +zvksed \
// RUN:   -target-feature +zvksh \
// RUN:   -disable-O0-optnone \
// RUN:   -emit-llvm %s -o - | opt -S -passes=mem2reg | \
// RUN:   FileCheck --check-prefix=CHECK-RV64 %s

#include <riscv_vector.h>

// CHECK-RV64-LABEL: define dso_local <vscale x 1 x i32> @test_vsm3me_vv_u32mf2
// CHECK-RV64-SAME: (<vscale x 1 x i32> [[VS2:%.*]], <vscale x 1 x i32> [[VS1:%.*]], i64 noundef [[VL:%.*]]) #[[ATTR0:[0-9]+]] {
// CHECK-RV64-NEXT:  entry:
// CHECK-RV64-NEXT:    [[TMP0:%.*]] = call <vscale x 1 x i32> @llvm.riscv.vsm3me.nxv1i32.nxv1i32.i64(<vscale x 1 x i32> poison, <vscale x 1 x i32> [[VS2]], <vscale x 1 x i32> [[VS1]], i64 [[VL]])
// CHECK-RV64-NEXT:    ret <vscale x 1 x i32> [[TMP0]]
//
vuint32mf2_t test_vsm3me_vv_u32mf2(vuint32mf2_t vs2, vuint32mf2_t vs1, size_t vl) {
  return __riscv_vsm3me(vs2, vs1, vl);
}

// CHECK-RV64-LABEL: define dso_local <vscale x 2 x i32> @test_vsm3me_vv_u32m1
// CHECK-RV64-SAME: (<vscale x 2 x i32> [[VS2:%.*]], <vscale x 2 x i32> [[VS1:%.*]], i64 noundef [[VL:%.*]]) #[[ATTR0]] {
// CHECK-RV64-NEXT:  entry:
// CHECK-RV64-NEXT:    [[TMP0:%.*]] = call <vscale x 2 x i32> @llvm.riscv.vsm3me.nxv2i32.nxv2i32.i64(<vscale x 2 x i32> poison, <vscale x 2 x i32> [[VS2]], <vscale x 2 x i32> [[VS1]], i64 [[VL]])
// CHECK-RV64-NEXT:    ret <vscale x 2 x i32> [[TMP0]]
//
vuint32m1_t test_vsm3me_vv_u32m1(vuint32m1_t vs2, vuint32m1_t vs1, size_t vl) {
  return __riscv_vsm3me(vs2, vs1, vl);
}

// CHECK-RV64-LABEL: define dso_local <vscale x 4 x i32> @test_vsm3me_vv_u32m2
// CHECK-RV64-SAME: (<vscale x 4 x i32> [[VS2:%.*]], <vscale x 4 x i32> [[VS1:%.*]], i64 noundef [[VL:%.*]]) #[[ATTR0]] {
// CHECK-RV64-NEXT:  entry:
// CHECK-RV64-NEXT:    [[TMP0:%.*]] = call <vscale x 4 x i32> @llvm.riscv.vsm3me.nxv4i32.nxv4i32.i64(<vscale x 4 x i32> poison, <vscale x 4 x i32> [[VS2]], <vscale x 4 x i32> [[VS1]], i64 [[VL]])
// CHECK-RV64-NEXT:    ret <vscale x 4 x i32> [[TMP0]]
//
vuint32m2_t test_vsm3me_vv_u32m2(vuint32m2_t vs2, vuint32m2_t vs1, size_t vl) {
  return __riscv_vsm3me(vs2, vs1, vl);
}

// CHECK-RV64-LABEL: define dso_local <vscale x 8 x i32> @test_vsm3me_vv_u32m4
// CHECK-RV64-SAME: (<vscale x 8 x i32> [[VS2:%.*]], <vscale x 8 x i32> [[VS1:%.*]], i64 noundef [[VL:%.*]]) #[[ATTR0]] {
// CHECK-RV64-NEXT:  entry:
// CHECK-RV64-NEXT:    [[TMP0:%.*]] = call <vscale x 8 x i32> @llvm.riscv.vsm3me.nxv8i32.nxv8i32.i64(<vscale x 8 x i32> poison, <vscale x 8 x i32> [[VS2]], <vscale x 8 x i32> [[VS1]], i64 [[VL]])
// CHECK-RV64-NEXT:    ret <vscale x 8 x i32> [[TMP0]]
//
vuint32m4_t test_vsm3me_vv_u32m4(vuint32m4_t vs2, vuint32m4_t vs1, size_t vl) {
  return __riscv_vsm3me(vs2, vs1, vl);
}

// CHECK-RV64-LABEL: define dso_local <vscale x 16 x i32> @test_vsm3me_vv_u32m8
// CHECK-RV64-SAME: (<vscale x 16 x i32> [[VS2:%.*]], <vscale x 16 x i32> [[VS1:%.*]], i64 noundef [[VL:%.*]]) #[[ATTR0]] {
// CHECK-RV64-NEXT:  entry:
// CHECK-RV64-NEXT:    [[TMP0:%.*]] = call <vscale x 16 x i32> @llvm.riscv.vsm3me.nxv16i32.nxv16i32.i64(<vscale x 16 x i32> poison, <vscale x 16 x i32> [[VS2]], <vscale x 16 x i32> [[VS1]], i64 [[VL]])
// CHECK-RV64-NEXT:    ret <vscale x 16 x i32> [[TMP0]]
//
vuint32m8_t test_vsm3me_vv_u32m8(vuint32m8_t vs2, vuint32m8_t vs1, size_t vl) {
  return __riscv_vsm3me(vs2, vs1, vl);
}


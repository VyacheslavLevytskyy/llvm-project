# An example of a function which has been split into two parts. Roughly
# corresponds to this C code.
# int baz() { return 47; }
# int bar() { return foo(0); }
# int foo(int flag) { return flag ? bar() : baz(); }
# int main() { return foo(1); }
# The function bar has been placed "in the middle" of foo. The functions are not
# using the frame pointer register and the are deliberately adjusting the stack
# pointer to test that we're using the correct unwind row.

        .text

        .type   baz,@function
baz:
        .cfi_startproc
        movl    $47, %eax
        retq
        .cfi_endproc
.Lbaz_end:
        .size   baz, .Lbaz_end-baz

foo.__part.3:
        .cfi_startproc
        .cfi_def_cfa_offset 32
        .cfi_offset %rbx, -16
        addq    $24, %rsp
        .cfi_def_cfa %rsp, 8
        retq
.Lfoo.__part.3_end:
        .size   foo.__part.3, .Lfoo.__part.3_end-foo.__part.3
        .cfi_endproc

# NB: Deliberately inserting padding to separate the two parts of the function
# as we're currently only parsing a single FDE entry from a (coalesced) address
# range.
        nop

foo.__part.1:
        .cfi_startproc
        .cfi_def_cfa_offset 32
        .cfi_offset %rbx, -16
        subq    $16, %rsp
        .cfi_def_cfa_offset 48
        callq   bar
        addq    $16, %rsp
        .cfi_def_cfa_offset 32
        jmp     foo.__part.3
.Lfoo.__part.1_end:
        .size   foo.__part.1, .Lfoo.__part.1_end-foo.__part.1
        .cfi_endproc

bar:
        .cfi_startproc
        subq    $88, %rsp
        .cfi_def_cfa_offset 96
        xorl    %edi, %edi
        callq   foo
        addq    $88, %rsp
        .cfi_def_cfa %rsp, 8
        retq
        .cfi_endproc
.Lbar_end:
        .size   bar, .Lbar_end-bar

        .type   foo,@function
foo:
        .cfi_startproc
        pushq   %rbx
        .cfi_def_cfa_offset 16
        .cfi_offset %rbx, -16
        movl    %edi, %ebx
        cmpl    $0, %ebx
        je      foo.__part.2
        subq    $16, %rsp
        .cfi_def_cfa_offset 32
        jmp     foo.__part.1
        .cfi_endproc
.Lfoo_end:
        .size   foo, .Lfoo_end-foo

# NB: Deliberately inserting padding to separate the two parts of the function
# as we're currently only parsing a single FDE entry from a (coalesced) address
# range.
        nop

foo.__part.2:
        .cfi_startproc
        .cfi_def_cfa_offset 16
        .cfi_offset %rbx, -16
        subq    $16, %rsp
        .cfi_def_cfa_offset 32
        callq   baz
        jmp     foo.__part.3
.Lfoo.__part.2_end:
        .size   foo.__part.2, .Lfoo.__part.2_end-foo.__part.2
        .cfi_endproc

        .globl  main
        .type   main,@function
main:
        .cfi_startproc
        movl    $1, %edi
        callq   foo
        retq
        .cfi_endproc
.Lmain_end:
        .size   main, .Lmain_end-main

        .section        .debug_abbrev,"",@progbits
        .byte   1                               # Abbreviation Code
        .byte   17                              # DW_TAG_compile_unit
        .byte   1                               # DW_CHILDREN_yes
        .byte   37                              # DW_AT_producer
        .byte   8                               # DW_FORM_string
        .byte   19                              # DW_AT_language
        .byte   5                               # DW_FORM_data2
        .byte   17                              # DW_AT_low_pc
        .byte   1                               # DW_FORM_addr
        .byte   85                              # DW_AT_ranges
        .byte   35                              # DW_FORM_rnglistx
        .byte   116                             # DW_AT_rnglists_base
        .byte   23                              # DW_FORM_sec_offset
        .byte   0                               # EOM(1)
        .byte   0                               # EOM(2)
        .byte   2                               # Abbreviation Code
        .byte   46                              # DW_TAG_subprogram
        .byte   0                               # DW_CHILDREN_no
        .byte   17                              # DW_AT_low_pc
        .byte   1                               # DW_FORM_addr
        .byte   18                              # DW_AT_high_pc
        .byte   1                               # DW_FORM_addr
        .byte   3                               # DW_AT_name
        .byte   8                               # DW_FORM_string
        .byte   0                               # EOM(1)
        .byte   0                               # EOM(2)
        .byte   3                               # Abbreviation Code
        .byte   46                              # DW_TAG_subprogram
        .byte   1                               # DW_CHILDREN_yes
        .byte   85                              # DW_AT_ranges
        .byte   35                              # DW_FORM_rnglistx
        .byte   64                              # DW_AT_frame_base
        .byte   24                              # DW_FORM_exprloc
        .byte   3                               # DW_AT_name
        .byte   8                               # DW_FORM_string
        .byte   0                               # EOM(1)
        .byte   0                               # EOM(2)
        .byte   4                               # Abbreviation Code
        .byte   5                               # DW_TAG_formal_parameter
        .byte   0                               # DW_CHILDREN_no
        .byte   2                               # DW_AT_location
        .byte   24                              # DW_FORM_exprloc
        .byte   3                               # DW_AT_name
        .byte   8                               # DW_FORM_string
        .byte   73                              # DW_AT_type
        .byte   19                              # DW_FORM_ref4
        .byte   0                               # EOM(1)
        .byte   0                               # EOM(2)
        .byte   5                               # Abbreviation Code
        .byte   36                              # DW_TAG_base_type
        .byte   0                               # DW_CHILDREN_no
        .byte   3                               # DW_AT_name
        .byte   8                               # DW_FORM_string
        .byte   62                              # DW_AT_encoding
        .byte   11                              # DW_FORM_data1
        .byte   11                              # DW_AT_byte_size
        .byte   11                              # DW_FORM_data1
        .byte   0                               # EOM(1)
        .byte   0                               # EOM(2)
        .byte   0                               # EOM(3)

        .section        .debug_info,"",@progbits
.Lcu_begin0:
        .long   .Ldebug_info_end0-.Ldebug_info_start0 # Length of Unit
.Ldebug_info_start0:
        .short  5                               # DWARF version number
        .byte   1                               # DWARF Unit Type
        .byte   8                               # Address Size (in bytes)
        .long   .debug_abbrev                   # Offset Into Abbrev. Section
        .byte   1                               # Abbrev [1] DW_TAG_compile_unit
        .asciz  "Hand-written DWARF"            # DW_AT_producer
        .short  29                              # DW_AT_language
        .quad   0                               # DW_AT_low_pc
        .byte   1                               # DW_AT_ranges
        .long   .Lrnglists_table_base0          # DW_AT_rnglists_base
        .byte   2                               # Abbrev [2] DW_TAG_subprogram
        .quad   baz                             # DW_AT_low_pc
        .quad   .Lbaz_end                       # DW_AT_high_pc
        .asciz  "baz"                           # DW_AT_name
        .byte   2                               # Abbrev [2] DW_TAG_subprogram
        .quad   bar                             # DW_AT_low_pc
        .quad   .Lbar_end                       # DW_AT_high_pc
        .asciz  "bar"                           # DW_AT_name
        .byte   3                               # Abbrev [3] DW_TAG_subprogram
        .byte   0                               # DW_AT_ranges
        .byte   1                               # DW_AT_frame_base
        .byte   86
        .asciz  "foo"                           # DW_AT_name
        .byte   4                               # Abbrev [4] DW_TAG_formal_parameter
        .byte   1                               # DW_AT_location
        .byte   0x53                            # DW_OP_reg3
        .asciz  "flag"                          # DW_AT_name
        .long   .Lint-.Lcu_begin0               # DW_AT_type
        .byte   0                               # End Of Children Mark
        .byte   2                               # Abbrev [2] DW_TAG_subprogram
        .quad   main                            # DW_AT_low_pc
        .quad   .Lmain_end                      # DW_AT_high_pc
        .asciz  "main"                          # DW_AT_name
.Lint:
        .byte   5                               # Abbrev [5] DW_TAG_base_type
        .asciz  "int"                           # DW_AT_name
        .byte   5                               # DW_AT_encoding
        .byte   4                               # DW_AT_byte_size
        .byte   0                               # End Of Children Mark
.Ldebug_info_end0:

        .section        .debug_rnglists,"",@progbits
        .long   .Ldebug_list_header_end0-.Ldebug_list_header_start0 # Length
.Ldebug_list_header_start0:
        .short  5                               # Version
        .byte   8                               # Address size
        .byte   0                               # Segment selector size
        .long   2                               # Offset entry count
.Lrnglists_table_base0:
        .long   .Ldebug_ranges0-.Lrnglists_table_base0
        .long   .Ldebug_ranges1-.Lrnglists_table_base0
.Ldebug_ranges0:
        .byte   6                               # DW_RLE_start_end
        .quad   foo
        .quad   .Lfoo_end
        .byte   6                               # DW_RLE_start_end
        .quad   foo.__part.1
        .quad   .Lfoo.__part.1_end
        .byte   6                               # DW_RLE_start_end
        .quad   foo.__part.2
        .quad   .Lfoo.__part.2_end
        .byte   6                               # DW_RLE_start_end
        .quad   foo.__part.3
        .quad   .Lfoo.__part.3_end
        .byte   0                               # DW_RLE_end_of_list
.Ldebug_ranges1:
        .byte   6                               # DW_RLE_start_end
        .quad   baz
        .quad   .Lbaz_end
        .byte   6                               # DW_RLE_start_end
        .quad   bar
        .quad   .Lbar_end
        .byte   6                               # DW_RLE_start_end
        .quad   foo.__part.1
        .quad   .Lfoo.__part.1_end
        .byte   6                               # DW_RLE_start_end
        .quad   foo.__part.2
        .quad   .Lfoo.__part.2_end
        .byte   6                               # DW_RLE_start_end
        .quad   foo.__part.3
        .quad   .Lfoo.__part.3_end
        .byte   6                               # DW_RLE_start_end
        .quad   foo
        .quad   .Lfoo_end
        .byte   6                               # DW_RLE_start_end
        .quad   main
        .quad   .Lmain_end
        .byte   0                               # DW_RLE_end_of_list
.Ldebug_list_header_end0:

        .section        ".note.GNU-stack","",@progbits

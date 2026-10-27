    .text
    .align 2
    .globl lab3_extra_register_context
lab3_extra_register_context:
    # Preserve the caller's ABI before installing sentinel register values.
    addi sp, sp, -144
    sd ra, 0(sp)
    sd s0, 8(sp)
    sd s1, 16(sp)
    sd s2, 24(sp)
    sd s3, 32(sp)
    sd s4, 40(sp)
    sd s5, 48(sp)
    sd s6, 56(sp)
    sd s7, 64(sp)
    sd s8, 72(sp)
    sd s9, 80(sp)
    sd s10, 88(sp)
    sd s11, 96(sp)
    sd gp, 104(sp)
    sd tp, 112(sp)
    sd sp, 120(sp)

    li ra, 0x101
    li t0, 0x105
    li t1, 0x106
    li t2, 0x107
    li s0, 0x108
    li s1, 0x109
    li a0, 0x10a
    li a1, 0x10b
    li a2, 0x10c
    li a3, 0x10d
    li a4, 0x10e
    li a5, 0x10f
    li a6, 0x110
    li a7, 0x111
    li s2, 0x112
    li s3, 0x113
    li s4, 0x114
    li s5, 0x115
    li s6, 0x116
    li s7, 0x117
    li s8, 0x118
    li s9, 0x119
    li s10, 0x11a
    li s11, 0x11b
    li t3, 0x11c
    li t4, 0x11d
    li t5, 0x11e
    li t6, 0x11f
    .option push
    .option norvc
    ebreak
    .option pop
    sd a0, 128(sp)
    li a0, 0x101
    bne ra, a0, .Lextra_fail
    li a0, 0x105
    bne t0, a0, .Lextra_fail
    li a0, 0x106
    bne t1, a0, .Lextra_fail
    li a0, 0x107
    bne t2, a0, .Lextra_fail
    li a0, 0x108
    bne s0, a0, .Lextra_fail
    li a0, 0x109
    bne s1, a0, .Lextra_fail
    li a0, 0x10b
    bne a1, a0, .Lextra_fail
    li a0, 0x10c
    bne a2, a0, .Lextra_fail
    li a0, 0x10d
    bne a3, a0, .Lextra_fail
    li a0, 0x10e
    bne a4, a0, .Lextra_fail
    li a0, 0x10f
    bne a5, a0, .Lextra_fail
    li a0, 0x110
    bne a6, a0, .Lextra_fail
    li a0, 0x111
    bne a7, a0, .Lextra_fail
    li a0, 0x112
    bne s2, a0, .Lextra_fail
    li a0, 0x113
    bne s3, a0, .Lextra_fail
    li a0, 0x114
    bne s4, a0, .Lextra_fail
    li a0, 0x115
    bne s5, a0, .Lextra_fail
    li a0, 0x116
    bne s6, a0, .Lextra_fail
    li a0, 0x117
    bne s7, a0, .Lextra_fail
    li a0, 0x118
    bne s8, a0, .Lextra_fail
    li a0, 0x119
    bne s9, a0, .Lextra_fail
    li a0, 0x11a
    bne s10, a0, .Lextra_fail
    li a0, 0x11b
    bne s11, a0, .Lextra_fail
    li a0, 0x11c
    bne t3, a0, .Lextra_fail
    li a0, 0x11d
    bne t4, a0, .Lextra_fail
    li a0, 0x11e
    bne t5, a0, .Lextra_fail
    li a0, 0x11f
    bne t6, a0, .Lextra_fail
    ld t0, 128(sp)
    li a0, 0x10a
    bne t0, a0, .Lextra_fail
    ld t0, 104(sp)
    bne gp, t0, .Lextra_fail
    ld t0, 112(sp)
    bne tp, t0, .Lextra_fail
    ld t0, 120(sp)
    bne sp, t0, .Lextra_fail
    li a0, 0
    j .Lextra_return
.Lextra_fail:
    li a0, 1
.Lextra_return:
    ld ra, 0(sp)
    ld s0, 8(sp)
    ld s1, 16(sp)
    ld s2, 24(sp)
    ld s3, 32(sp)
    ld s4, 40(sp)
    ld s5, 48(sp)
    ld s6, 56(sp)
    ld s7, 64(sp)
    ld s8, 72(sp)
    ld s9, 80(sp)
    ld s10, 88(sp)
    ld s11, 96(sp)
    addi sp, sp, 144
    ret

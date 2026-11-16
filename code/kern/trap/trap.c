#include <assert.h>
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <kdebug.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>
#include <stdio.h>
#include <trap.h>
#include <vmm.h>

#define TICK_NUM 100

static void print_ticks()
{
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S
 */
void idt_init(void)
{
    extern void __alltraps(void);
    /* Set sscratch register to 0, indicating to exception vector that we are
     * presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
    /* Allow kernel to access user memory */
    set_csr(sstatus, SSTATUS_SUM);
}

void print_trapframe(struct trapframe *tf)
{
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
    cprintf("  cause    0x%08x\n", tf->cause);
}

void print_regs(struct pushregs *gpr)
{
    cprintf("  zero     0x%08x\n", gpr->zero);
    cprintf("  ra       0x%08x\n", gpr->ra);
    cprintf("  sp       0x%08x\n", gpr->sp);
    cprintf("  gp       0x%08x\n", gpr->gp);
    cprintf("  tp       0x%08x\n", gpr->tp);
    cprintf("  t0       0x%08x\n", gpr->t0);
    cprintf("  t1       0x%08x\n", gpr->t1);
    cprintf("  t2       0x%08x\n", gpr->t2);
    cprintf("  s0       0x%08x\n", gpr->s0);
    cprintf("  s1       0x%08x\n", gpr->s1);
    cprintf("  a0       0x%08x\n", gpr->a0);
    cprintf("  a1       0x%08x\n", gpr->a1);
    cprintf("  a2       0x%08x\n", gpr->a2);
    cprintf("  a3       0x%08x\n", gpr->a3);
    cprintf("  a4       0x%08x\n", gpr->a4);
    cprintf("  a5       0x%08x\n", gpr->a5);
    cprintf("  a6       0x%08x\n", gpr->a6);
    cprintf("  a7       0x%08x\n", gpr->a7);
    cprintf("  s2       0x%08x\n", gpr->s2);
    cprintf("  s3       0x%08x\n", gpr->s3);
    cprintf("  s4       0x%08x\n", gpr->s4);
    cprintf("  s5       0x%08x\n", gpr->s5);
    cprintf("  s6       0x%08x\n", gpr->s6);
    cprintf("  s7       0x%08x\n", gpr->s7);
    cprintf("  s8       0x%08x\n", gpr->s8);
    cprintf("  s9       0x%08x\n", gpr->s9);
    cprintf("  s10      0x%08x\n", gpr->s10);
    cprintf("  s11      0x%08x\n", gpr->s11);
    cprintf("  t3       0x%08x\n", gpr->t3);
    cprintf("  t4       0x%08x\n", gpr->t4);
    cprintf("  t5       0x%08x\n", gpr->t5);
    cprintf("  t6       0x%08x\n", gpr->t6);
}



//lab3 function2 2413807
#include <sbi.h>

/* 指导书未给定关机阈值；本次取 10 次打印，即累计 1000 次时钟中断后关机。 */
#define SHUTDOWN_AFTER 10

/* 仅处理内核可读地址上的 16/32 位异常指令；不可读地址可能再次 Trap，此处不是通用故障恢复器。 */
static size_t exception_instruction_length(uintptr_t epc) {
    uint16_t first = *(const volatile uint16_t *)epc; // 只读首个半字，识别压缩指令时无需跨越后续地址。
    if ((first & 3) != 3) { // 低两位非 11：16 位编码，包括 c.ebreak 及本实验的 16 位非法编码。
        return 2;
    }
    if ((first & 0x1f) != 0x1f) { // 低两位为 11，且低五位非 11111：32 位编码。
        return 4;
    }
    panic("unsupported exception instruction length at 0x%016lx", epc); // 更长编码不猜测长度，停止并报告。
}

void interrupt_handler(struct trapframe *tf) {
    uintptr_t cause = tf->cause & ~((uintptr_t)1 << (__riscv_xlen - 1)); // 去掉 scause 最高位的中断标志。
    switch (cause) { // 课程枚举中的其他类型仅打印名称；本次实际完成的是 S 态时钟中断处理。
        case IRQ_U_SOFT:
            cprintf("User software interrupt\n");
            break;
        case IRQ_S_SOFT:
            cprintf("Supervisor software interrupt\n");
            break;
        case IRQ_H_SOFT:
            cprintf("Hypervisor software interrupt\n");
            break;
        case IRQ_M_SOFT:
            cprintf("Machine software interrupt\n");
            break;
        case IRQ_U_TIMER:
            cprintf("User timer interrupt\n");
            break;
        case IRQ_S_TIMER:
            /* SBI 定时器设置的是一次绝对截止时间；每次中断都要重设，才能形成周期性通知。 */
            clock_set_next_event();
            ticks++; // 每个 S 态时钟中断加 1，屏幕输出频率另由 TICK_NUM=100 控制。
            if (ticks % TICK_NUM == 0) { // 第 100、200……次中断各打印一行，而非每次中断都打印。
                static size_t printed; // 静态存储期，记录打印次数；不会在每次处理时重新清零。
                print_ticks();
                if (++printed >= SHUTDOWN_AFTER) {
                    sbi_shutdown(); // 请求 M 态固件关机；验证时应观察 QEMU 实际退出，不能只看输出。
                }
            }
            break;
        case IRQ_H_TIMER:
            cprintf("Hypervisor timer interrupt\n");
            break;
        case IRQ_M_TIMER:
            cprintf("Machine timer interrupt\n");
            break;
        case IRQ_U_EXT:
            cprintf("User external interrupt\n");
            break;
        case IRQ_S_EXT:
            cprintf("Supervisor external interrupt\n");
            break;
        case IRQ_H_EXT:
            cprintf("Hypervisor external interrupt\n");
            break;
        case IRQ_M_EXT:
            cprintf("Machine external interrupt\n");
            break;
        default:
            cprintf("Unknown interrupt: cause %lu\n", cause);
            print_trapframe(tf);
            break;
    }
}

void exception_handler(struct trapframe *tf) {
    switch (tf->cause) { // 同步异常的 sepc 指向出错指令；处理成功后必须更新，避免返回后重复触发。
        case CAUSE_ILLEGAL_INSTRUCTION:
            cprintf("Illegal instruction at 0x%016lx\n", tf->epc);
            tf->epc += exception_instruction_length(tf->epc); // 按实际编码前进 2/4 字节，不能统一加 4。
            break;
        case CAUSE_BREAKPOINT:
            cprintf("Breakpoint at 0x%016lx\n", tf->epc);
            tf->epc += exception_instruction_length(tf->epc); // 同时支持 c.ebreak（16 位）与 ebreak（32 位）。
            break;
        default:
            print_trapframe(tf); // 其他异常只保留诊断；未修改 epc，不能声称已经恢复执行。
            break;
    }
}



/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap()
 * returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the sret instruction to return from the exception.
 * */
void trap(struct trapframe *tf)
{
    // dispatch based on what type of trap occurred
    if ((intptr_t)tf->cause < 0)
    {
        // interrupts
        interrupt_handler(tf);
    }
    else
    {
        // exceptions
        exception_handler(tf);
    }
}

#include <defs.h>
#include <unistd.h>
#include <syscall.h>

static inline int
syscall(int64_t num, uintptr_t x1, uintptr_t x2, uintptr_t x3,
        uintptr_t x4, uintptr_t x5)
{
    register uintptr_t a0 asm("a0") = num;
    register uintptr_t a1 asm("a1") = x1;
    register uintptr_t a2 asm("a2") = x2;
    register uintptr_t a3 asm("a3") = x3;
    register uintptr_t a4 asm("a4") = x4;
    register uintptr_t a5 asm("a5") = x5;
    asm volatile("ecall" : "+r"(a0), "+r"(a1), "+r"(a2),
                 "+r"(a3), "+r"(a4), "+r"(a5) : : "memory");
    return (int)a0;
}
int sys_exit(int64_t code) { return syscall(SYS_exit, code, 0, 0, 0, 0); }
int sys_fork(void) { return syscall(SYS_fork, 0, 0, 0, 0, 0); }
int sys_wait(int64_t pid, int *store) { return syscall(SYS_wait, pid, (uintptr_t)store, 0, 0, 0); }
int sys_yield(void) { return syscall(SYS_yield, 0, 0, 0, 0, 0); }
int sys_kill(int64_t pid) { return syscall(SYS_kill, pid, 0, 0, 0, 0); }
int sys_getpid(void) { return syscall(SYS_getpid, 0, 0, 0, 0, 0); }
int sys_putc(int64_t c) { return syscall(SYS_putc, c, 0, 0, 0, 0); }
int sys_pgdir(void) { return syscall(SYS_pgdir, 0, 0, 0, 0, 0); }

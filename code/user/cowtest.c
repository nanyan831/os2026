#include <ulib.h>
#include <stdio.h>
#include <error.h>

static volatile unsigned char area[32 * 4096] __attribute__((aligned(4096)));
static volatile int code __attribute__((aligned(4096))) = 0x12345678;

void __attribute__((used, noinline)) write_text(void) { asm volatile("nop"); }

int main(void)
{
    for (int i = 0; i < 32; i++) area[i * 4096] = i + 1;
    int pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        for (int i = 0; i < 32; i++) assert(area[i * 4096] == i + 1);
        area[4096] = 99;
        yield();
        assert(area[0] == 1 && area[4096] == 99);
        exit(73);
    }
    area[0] = 88; // Parent write before child runs also needs COW + TLB flush.
    assert(waitpid(pid, (int *)&code) == 0 && code == 73);
    assert(area[0] == 88 && area[4096] == 2);
    cprintf("[COW user] parent/child writes and untouched pages PASS\n");

    code = 0x12345678;
    int observer = fork();
    assert(observer >= 0);
    if (observer == 0) {
        for (int i = 0; i < 20; i++) { yield(); assert(code == 0x12345678); }
        exit(0);
    }
    int worker = fork();
    assert(worker >= 0);
    if (worker == 0) exit(42);
    assert(waitpid(worker, (int *)&code) == 0 && code == 42);
    assert(waitpid(observer, NULL) == 0);
    cprintf("[COW user] waitpid kernel copyout preserves observer PASS\n");

    pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        int grand = fork(); assert(grand >= 0);
        if (grand == 0) { area[2 * 4096] = 77; exit(9); }
        int status; assert(waitpid(grand, &status) == 0 && status == 9);
        assert(area[2 * 4096] == 3); exit(0);
    }
    int status;
    assert(waitpid(pid, &status) == 0 && status == 0 && area[2 * 4096] == 3);
    cprintf("[COW user] nested fork PASS\n");

    pid = fork(); assert(pid >= 0);
    if (pid == 0) { *(volatile char *)(uintptr_t)write_text = 0; exit(1); }
    assert(waitpid(pid, &status) == 0 && status == -E_KILLED);
    cprintf("[COW user] read-only text write rejected PASS\n");
    cprintf("cowtest pass.\n");
    return 0;
}

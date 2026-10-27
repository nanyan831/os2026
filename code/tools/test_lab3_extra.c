/* Independent regression harness; injected only into a temporary tree. */
#include <assert.h>
#include <clock.h>
#include <intr.h>
#include <pmm.h>
#include <riscv.h>
#include <sbi.h>
#include <stdio.h>
#include <trap.h>
#include <../sync/sync.h>

extern uintptr_t lab3_extra_register_context(void);

static void check_tail_insertion(void) {
    cprintf("[Extra] PMM tail insertion: BEGIN\n");
    size_t initial = nr_free_pages();
    assert(initial > 8);
    struct Page *all = alloc_pages(initial);
    assert(all != NULL && nr_free_pages() == 0);
    free_page(all + 1);
    free_page(all + 5);
    assert(nr_free_pages() == 2);
    assert(alloc_page() == all + 1);
    assert(alloc_page() == all + 5);
    assert(nr_free_pages() == 0);
    free_pages(all, initial);
    assert(nr_free_pages() == initial);
    /* Adjacent tail insertion must also preserve the predecessor for merging. */
    all = alloc_pages(initial);
    assert(all != NULL);
    free_page(all + 1);
    free_page(all + 2);
    assert(alloc_pages(2) == all + 1);
    assert(nr_free_pages() == 0);
    free_pages(all, initial);
    assert(nr_free_pages() == initial);
    cprintf("[Extra] PMM tail insertion: PASS\n");
}

static void check_interrupt_state(void) {
    size_t initial = nr_free_pages();
    for (int enabled = 0; enabled <= 1; enabled++) {
        if (enabled) intr_enable(); else intr_disable();
        uintptr_t expected = enabled ? SSTATUS_SIE : 0;
        assert((read_csr(sstatus) & SSTATUS_SIE) == expected);
        struct Page *p = alloc_page();
        assert(p != NULL);
        assert((read_csr(sstatus) & SSTATUS_SIE) == expected);
        assert(nr_free_pages() == initial - 1);
        assert((read_csr(sstatus) & SSTATUS_SIE) == expected);
        free_page(p);
        assert((read_csr(sstatus) & SSTATUS_SIE) == expected);
        assert(nr_free_pages() == initial);
    }
    intr_disable();
    bool outer, inner;
    intr_enable();
    local_intr_save(outer);
    local_intr_save(inner);
    assert(outer && !inner);
    local_intr_restore(inner);
    assert(!(read_csr(sstatus) & SSTATUS_SIE));
    local_intr_restore(outer);
    assert(read_csr(sstatus) & SSTATUS_SIE);
    intr_disable();
    cprintf("[Extra] SIE preservation and nested save/restore: PASS\n");
}

void lab3_extra_check(void) {
    assert(sizeof(struct trapframe) == 36 * sizeof(uintptr_t));
    assert(offsetof(struct trapframe, status) == 32 * sizeof(uintptr_t));
    assert(offsetof(struct trapframe, epc) == 33 * sizeof(uintptr_t));
    check_tail_insertion();
    assert(lab3_extra_register_context() == 0);
    cprintf("[Extra] all writable GPRs, original SP, GP/TP: PASS\n");

    asm volatile(".globl lab3_break16_site\nlab3_break16_site:\n"
                 ".2byte 0x9002\n" ::: "memory");
    cprintf("[Extra] breakpoint16 return: PASS\n");
    asm volatile(".globl lab3_break32_site\nlab3_break32_site:\n"
                 ".word 0x00100073\n" ::: "memory");
    cprintf("[Extra] breakpoint32 return: PASS\n");
    asm volatile(".globl lab3_illegal16_site\nlab3_illegal16_site:\n"
                 ".2byte 0x0000\n" ::: "memory");
    cprintf("[Extra] illegal16 return: PASS\n");
    asm volatile(".globl lab3_illegal32_site\nlab3_illegal32_site:\n"
                 ".word 0x0000007b\n" ::: "memory");
    cprintf("[Extra] illegal32 return: PASS\n");
    check_interrupt_state();
    assert(ticks == 0);
    cprintf("[Extra] suite complete: PASS\n");
    sbi_shutdown();
    panic("SBI shutdown returned in extra harness");
}

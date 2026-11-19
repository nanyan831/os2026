#include <vmm.h>
#include <pmm.h>
#include <default_pmm.h>
#include <assert.h>
#include <string.h>
#include <error.h>
#include <stdio.h>
#include <kmalloc.h>

#ifdef COW_SELFTEST
static struct mm_struct *test_mm(void)
{
    struct mm_struct *mm = mm_create();
    assert(mm != NULL);
    struct Page *root = alloc_page();
    assert(root != NULL);
    mm->pgdir = page2kva(root);
    memcpy(mm->pgdir, boot_pgdir_va, PGSIZE);
    return mm;
}

static void destroy_test_mm(struct mm_struct *mm)
{
    exit_mmap(mm);
    free_page(kva2page(mm->pgdir));
    mm_destroy(mm);
}

static void one_round(void)
{
    uintptr_t va = UTEXT, ro = UTEXT + 4 * PGSIZE, far = UTEXT + 4 * 1024 * 1024;
    struct mm_struct *a = test_mm(), *b = test_mm();
    assert(mm_map(a, va, 2 * PGSIZE, VM_READ | VM_WRITE, NULL) == 0);
    assert(mm_map(a, ro, PGSIZE, VM_READ, NULL) == 0);
    assert(mm_map(a, far, PGSIZE, VM_READ | VM_WRITE, NULL) == 0);
    assert(mm_map(a, va - PGSIZE, 4 * PGSIZE, VM_READ, NULL) == -E_INVAL);
    struct Page *shared = pgdir_alloc_page(a->pgdir, va, PTE_U | PTE_R | PTE_W);
    struct Page *readonly = pgdir_alloc_page(a->pgdir, ro, PTE_U | PTE_R);
    assert(shared && readonly);
    memset(page2kva(shared), 0x31, PGSIZE);
    memset(page2kva(readonly), 0x52, PGSIZE);
    assert(pgdir_alloc_page(a->pgdir, far, PTE_U | PTE_R | PTE_W));
    assert(user_mem_check(a, va, 1, 1));
    char *stale = page2kva(shared); // Capture before fork changes the mapping.
    assert(dup_mmap(b, a) == 0);
    pte_t *ap = get_pte(a->pgdir, va, 0), *bp = get_pte(b->pgdir, va, 0);
    assert(pte2page(*ap) == pte2page(*bp) && page_ref(shared) == 2);
    assert((*ap & PTE_COW) && !(*ap & PTE_W));
    assert(!(*get_pte(b->pgdir, ro, 0) & (PTE_COW | PTE_W)));
    cprintf("[COW kernel] shared PPN, permissions and refs PASS\n");

    // Deterministic failure injection: hide free pages temporarily, with IRQs off.
    bool irq; local_intr_save(irq);
    free_area_t saved = free_area;
    list_init(&free_area.free_list); free_area.nr_free = 0;
    pte_t before_a = *ap, before_b = *bp;
    assert(cow_resolve(b, va) == -E_NO_MEM);
    assert(*ap == before_a && *bp == before_b && page_ref(shared) == 2);
    free_area = saved;
    local_intr_restore(irq);
    cprintf("[COW kernel] OOM leaves mapping and refs unchanged PASS\n");

    // Dirty-COW-like stale translation: validation happened before fork; a direct
    // kernel mapping bypasses user PTE write protection. Scratch pages only.
    stale[0] = 'X';
    assert(((char *)page2kva(get_page(b->pgdir, va, NULL)))[0] == 'X');
    stale[0] = '1';
    cprintf("[Dirty COW model] stale direct-map write corrupts peer REPRODUCED\n");
    char x = 'B';
    assert(copy_to_user(b, (void *)va, &x, 1));
    assert(((char *)page2kva(shared))[0] == '1');
    assert(((char *)page2kva(get_page(b->pgdir, va, NULL)))[0] == 'B');
    assert(get_page(a->pgdir, va, NULL) != get_page(b->pgdir, va, NULL));
    assert(page_ref(shared) == 1);
    size_t before = nr_free_pages();
    assert(cow_resolve(a, va) == 0);
    assert(nr_free_pages() == before && !(*ap & PTE_COW) && (*ap & PTE_W));
    assert(!copy_to_user(b, (void *)ro, &x, 1));
    assert(cow_resolve(b, ro) == -E_INVAL);
    assert(((char *)page2kva(readonly))[0] == 0x52);
    cprintf("[Dirty COW model] checked copyout isolates peer and rejects RO PASS\n");

    char data[8] = "1234567", out[8];
    assert(copy_to_user(b, (void *)(va + PGSIZE - 4), data, 8));
    assert(copy_from_user(b, out, (void *)(va + PGSIZE - 4), 8, 0));
    assert(memcmp(data, out, 8) == 0);
    assert(!user_mem_check(b, (uintptr_t)-4, 16, 1));
    assert(!copy_to_user(b, (void *)(va + 2 * PGSIZE - 4), data, 8));
    cprintf("[COW kernel] cross-page copyout, demand zero, overflow checks PASS\n");
    destroy_test_mm(a);
    destroy_test_mm(b);
}

void check_cow(void)
{
    one_round();
    kmalloc_trim();
    size_t before = nr_free_pages();
    for (int i = 0; i < 3; i++) one_round();
    kmalloc_trim();
    assert(nr_free_pages() == before);
    cprintf("[COW kernel] repeated multi-VMA teardown has no page leak PASS\n");
    cprintf("COW kernel selftests PASS\n");
}
#endif

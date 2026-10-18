#include <mmu_probe.h>
#include <pmm.h>
#include <stdio.h>

/* A dedicated data page makes the observed load independent of code fetches. */
volatile uint64_t mmu_probe_word __attribute__((aligned(PGSIZE))) = 0x1122334455667788ULL;

__attribute__((noinline)) void mmu_probe(void) {
    flush_tlb();
    uint64_t first = mmu_probe_word;
    uint64_t second = mmu_probe_word;
    assert(first == 0x1122334455667788ULL && second == first);
    cprintf("[MMU probe] VA=0x%016lx PA=0x%016lx first=0x%016lx second=0x%016lx\n",
            (uintptr_t)&mmu_probe_word, PADDR(&mmu_probe_word), first, second);
}

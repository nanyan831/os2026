#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <buddy_pmm.h>
#include <mmu_probe.h>
#ifdef SLUB_TEST
#include <slub.h>
#endif
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
#if defined(PMM_first_fit)
    pmm_manager = &default_pmm_manager;
#elif defined(PMM_buddy)
    pmm_manager = &buddy_pmm_manager;
#else
    pmm_manager = &best_fit_pmm_manager;
#endif
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 2413807
static void page_init(void) {
    extern char end[];
    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    if (mem_base != DRAM_BASE || mem_size == 0 ||
        mem_size > (uint64_t)-1 - mem_base) {
        panic("invalid DTB physical memory range");
    }
    uint64_t reported_end = mem_base + mem_size;
    uint64_t managed_end = reported_end < PHYSICAL_MEMORY_END
                               ? reported_end : PHYSICAL_MEMORY_END;
    managed_end = ROUNDDOWN(managed_end, PGSIZE);
    if (managed_end <= KERNEL_BEGIN_PADDR) {
        panic("RAM does not contain the kernel");
    }
    npage = PPN(managed_end);
    pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE);
    size_t count = npage - nbase;
    uintptr_t pages_pa = PADDR(pages);
    if (pages_pa < KERNEL_BEGIN_PADDR || pages_pa >= managed_end ||
        count > (managed_end - pages_pa) / sizeof(struct Page)) {
        panic("RAM is too small for Page descriptors");
    }
    uintptr_t metadata_end = pages_pa + count * sizeof(struct Page);
    uintptr_t free_begin = ROUNDUP(metadata_end, PGSIZE);

    cprintf("physical memory map:\n");
    cprintf("  memory: 0x%016lx, [0x%016lx, 0x%016lx].\n",
            mem_size, mem_base, reported_end - 1);
    cprintf("  managed: [0x%016lx, 0x%016lx), descriptors=%lu x %lu bytes\n",
            mem_base, managed_end, count, (size_t)sizeof(struct Page));
    cprintf("  descriptors PA: [0x%016lx, 0x%016lx)\n", pages_pa, metadata_end);
    for (size_t i = 0; i < count; i++) {
        struct Page *page = pages + i;
        page->flags = 0;
        SetPageReserved(page);
        page->property = 0;
        set_page_ref(page, 0);
        list_init(&page->page_link);
    }
    if (free_begin >= managed_end) {
        panic("no complete free physical page");
    }
    size_t free_count = (managed_end - free_begin) / PGSIZE;
    init_memmap(pa2page(free_begin), free_count);
    cprintf("  free PA: [0x%016lx, 0x%016lx), pages=%lu\n",
            free_begin, managed_end, free_count);
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();
#if defined(PMM_first_fit)
    first_fit_regression_check();
#endif
#ifdef SLUB_TEST
    slub_check();
#endif

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
    mmu_probe();
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

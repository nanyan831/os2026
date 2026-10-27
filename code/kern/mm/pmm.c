#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <../sync/sync.h>
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
#ifdef PMM_best_fit
    pmm_manager = &best_fit_pmm_manager;
#else
    pmm_manager = &default_pmm_manager;
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
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

//lab2 function1 2413807
static void page_init(void) {
    extern char end[];
    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    // 当前教学框架只接受固定起点的单个RAM区，先拒绝空范围与地址加法溢出。
    if (mem_base != DRAM_BASE || mem_size == 0 ||
        mem_size > (uint64_t)-1 - mem_base) {
        panic("invalid DTB physical memory range");
    }
    uint64_t reported_end = mem_base + mem_size;
    // DTB报告的RAM可能更大；框架只管理到PHYSICAL_MEMORY_END，末端向下取整。
    uint64_t managed_end = reported_end < PHYSICAL_MEMORY_END
                               ? reported_end : PHYSICAL_MEMORY_END;
    managed_end = ROUNDDOWN(managed_end, PGSIZE);
    if (managed_end <= KERNEL_BEGIN_PADDR) {
        panic("RAM does not contain the kernel");
    }
    // end是链接器给出的内核虚拟末端；数组起点向上对齐，避免覆盖内核数据。
    npage = PPN(managed_end);
    pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE);
    size_t count = npage - nbase;
    uintptr_t pages_pa = PADDR(pages);
    // 先用除法判断数组是否放得下，再计算乘积/写数组，避免越界初始化。
    if (pages_pa < KERNEL_BEGIN_PADDR || pages_pa >= managed_end ||
        count > (managed_end - pages_pa) / sizeof(struct Page)) {
        panic("RAM is too small for Page descriptors");
    }
    uintptr_t metadata_end = pages_pa + count * sizeof(struct Page);
    // 数组结束到下一页边界的零头也保留，只交付完整物理页。
    uintptr_t free_begin = ROUNDUP(metadata_end, PGSIZE);

    cprintf("physical memory map:\n");
    cprintf("  memory: 0x%016lx, [0x%016lx, 0x%016lx].\n",
            mem_size, mem_base, reported_end - 1);
    cprintf("  managed: [0x%016lx, 0x%016lx), descriptors=%lu x %lu bytes\n",
            mem_base, managed_end, count, (size_t)sizeof(struct Page));
    cprintf("  descriptors PA: [0x%016lx, 0x%016lx)\n", pages_pa, metadata_end);
    // 先保守地标记全部页为reserved，包含固件保留区、内核和元数据所在页。
    for (size_t i = 0; i < count; i++) {
        struct Page *page = pages + i;
        page->flags = 0;
        SetPageReserved(page);
        page->property = 0;
        set_page_ref(page, 0);
        list_init(&page->page_link);
    }
    // 没有完整空闲页时不能以n=0调用管理器的init_memmap。
    if (free_begin >= managed_end) {
        panic("no complete free physical page");
    }
    size_t free_count = (managed_end - free_begin) / PGSIZE;
    // pa2page把物理页地址换成描述符；具体管理器负责取消这段页的保留状态。
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

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__
#include <pmm.h>
#define BUDDY_MAX_ORDER 20
extern const struct pmm_manager buddy_pmm_manager;
size_t buddy_block_pages(struct Page *base);
void buddy_validate(void);
void buddy_check(void);
#endif

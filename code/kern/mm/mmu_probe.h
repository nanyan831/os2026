#ifndef __KERN_MM_MMU_PROBE_H__
#define __KERN_MM_MMU_PROBE_H__
#include <defs.h>
extern volatile uint64_t mmu_probe_word;
void mmu_probe(void);
#endif

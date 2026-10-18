#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__
#include <defs.h>
void *slub_alloc(size_t size);
int slub_free(void *ptr);
size_t slub_pages_in_use(void);
void slub_check(void);
#endif

#include <slub.h>
#include <buddy_pmm.h>
#include <list.h>
#include <pmm.h>
#include <string.h>

/* lab2 challenge 2413807: single-hart teaching SLUB, not Linux SLUB. */
#define SLAB_MAGIC 0x534c5542534d414cULL
#define LARGE_MAGIC 0x534c55424c415247ULL
#define NCACHE 8
struct slab_cache {
    size_t object_size;
    list_entry_t partial;
};
struct slab_page {
    uint64_t magic;
    struct slab_cache *cache;
    list_entry_t link;
    size_t capacity;
    size_t inuse;
    void *free_head;
    struct Page *backing;
    size_t requested_pages;
    size_t actual_pages;
    uint64_t allocated[4];
};
static struct slab_cache caches[NCACHE];
static bool initialized;
static size_t active_pages;

static size_t header_size(void) {
    return ROUNDUP(sizeof(struct slab_page), 16);
}

static void initialize(void) {
    if (!initialized) {
        for (size_t i = 0; i < NCACHE; i++) {
            caches[i].object_size = (size_t)16 << i;
            list_init(&caches[i].partial);
        }
        initialized = 1;
    }
}

static struct slab_page *allocate_backing(size_t requested_pages) {
    struct Page *p = alloc_pages(requested_pages);
    if (p == NULL) {
        return NULL;
    }
    struct slab_page *slab = (void *)(page2pa(p) + va_pa_offset);
    memset(slab, 0, header_size());
    slab->backing = p;
    slab->requested_pages = requested_pages;
#if defined(PMM_buddy)
    slab->actual_pages = buddy_block_pages(p);
#else
    slab->actual_pages = requested_pages;
#endif
    active_pages += slab->actual_pages;
    list_init(&slab->link);
    return slab;
}

static void release_backing(struct slab_page *slab) {
    struct Page *p = slab->backing;
    size_t n = slab->requested_pages;
    active_pages -= slab->actual_pages;
    slab->magic = 0;
    free_pages(p, n);
}

void *slub_alloc(size_t size) {
    if (size == 0 || size > (size_t)-1 - header_size() - (PGSIZE - 1)) {
        return NULL;
    }
    initialize();
    if (size > 2048) {
        size_t n = (size + header_size() + PGSIZE - 1) / PGSIZE;
        if (n > nr_free_pages()) {
            return NULL;
        }
        struct slab_page *slab = allocate_backing(n);
        if (slab == NULL) {
            return NULL;
        }
        slab->magic = LARGE_MAGIC;
        return (char *)slab + header_size();
    }
    size_t index = 0;
    while (caches[index].object_size < size) {
        index++;
    }
    struct slab_cache *cache = &caches[index];
    struct slab_page *slab;
    if (list_empty(&cache->partial)) {
        slab = allocate_backing(1);
        if (slab == NULL) {
            return NULL;
        }
        slab->magic = SLAB_MAGIC;
        slab->cache = cache;
        slab->capacity = (PGSIZE - header_size()) / cache->object_size;
        assert(slab->capacity > 0 && slab->capacity <= 256);
        char *objects = (char *)slab + header_size();
        for (size_t slot = slab->capacity; slot-- > 0;) {
            void **object = (void **)(objects + slot * cache->object_size);
            *object = slab->free_head;
            slab->free_head = object;
        }
        list_add(&cache->partial, &slab->link);
    } else {
        slab = to_struct(list_next(&cache->partial), struct slab_page, link);
    }
    void *object = slab->free_head;
    assert(object != NULL);
    slab->free_head = *(void **)object;
    size_t slot = ((uintptr_t)object - (uintptr_t)slab - header_size()) / cache->object_size;
    uint64_t mask = 1ULL << (slot % 64);
    assert((slab->allocated[slot / 64] & mask) == 0);
    slab->allocated[slot / 64] |= mask;
    slab->inuse++;
    if (slab->inuse == slab->capacity) {
        list_del(&slab->link);
        list_init(&slab->link);
    }
    return object;
}

int slub_free(void *ptr) {
    if (ptr == NULL) {
        return 0;
    }
    uintptr_t address = (uintptr_t)ptr;
    if (address < KERNBASE || address - va_pa_offset < DRAM_BASE ||
        address - va_pa_offset >= npage * PGSIZE) {
        return -1;
    }
    struct slab_page *slab = (void *)ROUNDDOWN(address, PGSIZE);
    uintptr_t objects = (uintptr_t)slab + header_size();
    if (slab->magic == LARGE_MAGIC) {
        if (address != objects) {
            return -1;
        }
        release_backing(slab);
        return 0;
    }
    if (slab->magic != SLAB_MAGIC || address < objects) {
        return -1;
    }
    struct slab_cache *cache = slab->cache;
    if (cache < caches || cache >= caches + NCACHE) {
        return -1;
    }
    size_t distance = address - objects;
    if (distance % cache->object_size != 0) {
        return -1;
    }
    size_t slot = distance / cache->object_size;
    if (slot >= slab->capacity) {
        return -1;
    }
    uint64_t mask = 1ULL << (slot % 64);
    if ((slab->allocated[slot / 64] & mask) == 0) {
        return -1;
    }
    if (slab->inuse == slab->capacity) {
        list_add(&cache->partial, &slab->link);
    }
    slab->allocated[slot / 64] &= ~mask;
    slab->inuse--;
    *(void **)ptr = slab->free_head;
    slab->free_head = ptr;
    if (slab->inuse == 0) {
        list_del(&slab->link);
        release_backing(slab);
    }
    return 0;
}

size_t slub_pages_in_use(void) {
    return active_pages;
}

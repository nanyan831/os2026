#include <slub.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>

static void *objects[600];

static void verify_bytes(void *p, size_t n, unsigned char pattern) {
    unsigned char *bytes = p;
    for (size_t i = 0; i < n; i++) {
        assert(bytes[i] == pattern);
    }
}

void slub_check(void) {
    size_t baseline = nr_free_pages();
    size_t sizes[] = {1, 15, 16, 17, 31, 32, 33, 127, 128, 129,
                      1024, 2048, 2049, 4096, 6000, 16384};
    assert(slub_alloc(0) == NULL && slub_alloc((size_t)-1) == NULL);
    assert(slub_free(NULL) == 0);
    assert(slub_pages_in_use() == 0 && nr_free_pages() == baseline);
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        objects[i] = slub_alloc(sizes[i]);
        assert(objects[i] != NULL && (uintptr_t)objects[i] % 16 == 0);
        memset(objects[i], (unsigned char)(i + 1), sizes[i]);
        uintptr_t begin = (uintptr_t)objects[i];
        for (size_t j = 0; j < i; j++) {
            uintptr_t other = (uintptr_t)objects[j];
            assert(begin + sizes[i] <= other || other + sizes[j] <= begin);
        }
    }
    assert(slub_free((char *)objects[3] + 1) == -1);
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        verify_bytes(objects[i], sizes[i], (unsigned char)(i + 1));
        assert(slub_free(objects[i]) == 0);
    }
    assert(slub_pages_in_use() == 0 && nr_free_pages() == baseline);
    cprintf("[SLUB 1] size boundaries/alignment/data/large objects: PASS\n");

    for (size_t i = 0; i < 600; i++) {
        objects[i] = slub_alloc(64);
        assert(objects[i] != NULL);
        memset(objects[i], (unsigned char)(i % 251), 64);
    }
    size_t occupied = slub_pages_in_use();
    assert(occupied > 1);
    for (size_t i = 0; i < 600; i += 2) {
        verify_bytes(objects[i], 64, (unsigned char)(i % 251));
        assert(slub_free(objects[i]) == 0);
    }
    assert(slub_pages_in_use() == occupied);
    for (size_t i = 0; i < 600; i += 2) {
        objects[i] = slub_alloc(64);
        assert(objects[i] != NULL);
        memset(objects[i], (unsigned char)(i % 251), 64);
    }
    assert(slub_pages_in_use() == occupied);
    for (size_t i = 1; i < 600; i += 2) {
        verify_bytes(objects[i], 64, (unsigned char)(i % 251));
        assert(slub_free(objects[i]) == 0);
    }
    for (size_t i = 0; i < 600; i += 2) {
        verify_bytes(objects[i], 64, (unsigned char)(i % 251));
        assert(slub_free(objects[i]) == 0);
    }
    assert(slub_pages_in_use() == 0 && nr_free_pages() == baseline);
    cprintf("[SLUB 2] cross-page allocation/reuse/reclaim: PASS (%lu slab pages)\n", occupied);

    void *a = slub_alloc(16), *b = slub_alloc(16);
    assert(a != NULL && b != NULL);
    assert(slub_free(a) == 0 && slub_free(a) == -1);
    assert(slub_free((char *)b + 1) == -1);
    assert(slub_free(b) == 0);
    assert(nr_free_pages() == baseline && slub_pages_in_use() == 0);
    cprintf("[SLUB 3] invalid/interior/double free: PASS\n");
    cprintf("[SLUB] ALL TESTS PASSED; free pages restored: %lu\n", baseline);
    cprintf("slub physical pages in use: %lu\n", slub_pages_in_use());
}

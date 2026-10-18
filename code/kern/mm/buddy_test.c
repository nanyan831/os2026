#include <buddy_pmm.h>
#include <stdio.h>
#include <string.h>

#define TEST_MAX_PAGES ((PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE)
static struct Page *held[TEST_MAX_PAGES];
static struct Page *lookup[TEST_MAX_PAGES];

static size_t rounded_pages(size_t requested) {
    size_t rounded = 1;
    while (rounded < requested) {
        rounded *= 2;
    }
    return rounded;
}

static uint64_t *block_words(struct Page *p) {
    return (uint64_t *)(page2pa(p) + va_pa_offset);
}

static void check_disjoint(struct Page *a, size_t an, struct Page *b, size_t bn) {
    assert(page2ppn(a) + an <= page2ppn(b) || page2ppn(b) + bn <= page2ppn(a));
}

void buddy_check(void) {
    size_t baseline = nr_free_pages();
    buddy_validate();
    size_t requests[] = {1, 2, 3, 5, 17, 64};
    struct Page *blocks[6];
    size_t used = 0;
    for (size_t i = 0; i < 6; i++) {
        size_t actual = rounded_pages(requests[i]);
        blocks[i] = alloc_pages(requests[i]);
        assert(blocks[i] != NULL && buddy_block_pages(blocks[i]) == actual);
        assert(page2ppn(blocks[i]) % actual == 0);
        for (size_t j = 0; j < i; j++) {
            check_disjoint(blocks[i], actual, blocks[j], rounded_pages(requests[j]));
        }
        uint64_t marker = 0xbadd0000UL + i;
        block_words(blocks[i])[0] = marker;
        block_words(blocks[i])[actual * PGSIZE / sizeof(uint64_t) - 1] = marker;
        used += actual;
        assert(nr_free_pages() == baseline - used);
    }
    for (size_t i = 6; i-- > 0;) {
        size_t actual = rounded_pages(requests[i]);
        uint64_t marker = 0xbadd0000UL + i;
        assert(block_words(blocks[i])[0] == marker);
        assert(block_words(blocks[i])[actual * PGSIZE / sizeof(uint64_t) - 1] == marker);
        free_pages(blocks[i], requests[i]);
    }
    assert(nr_free_pages() == baseline);
    assert(alloc_pages(0) == NULL && alloc_pages(TEST_MAX_PAGES + 1) == NULL);
    assert(nr_free_pages() == baseline);
    buddy_validate();
    cprintf("[Buddy 1] rounding/alignment/data/failure: PASS\n");

    memset(lookup, 0, sizeof(lookup));
    size_t count = 0;
    struct Page *p;
    while ((p = alloc_page()) != NULL) {
        assert(count < TEST_MAX_PAGES && lookup[page2ppn(p) - nbase] == NULL);
        held[count++] = p;
        lookup[page2ppn(p) - nbase] = p;
    }
    assert(count == baseline && nr_free_pages() == 0 && alloc_page() == NULL);
    ppn_t first = 0;
    for (ppn_t ppn = nbase; ppn + 16 <= npage; ppn += 16) {
        bool complete = 1;
        for (size_t i = 0; i < 16; i++) {
            if (lookup[ppn - nbase + i] == NULL) {
                complete = 0;
            }
        }
        if (complete) {
            first = ppn;
            break;
        }
    }
    assert(first != 0);
    struct Page *base = lookup[first - nbase];
    free_page(base);
    free_page(base + 2);
    assert(nr_free_pages() == 2 && alloc_pages(2) == NULL);
    free_page(base + 1);
    p = alloc_pages(2);
    assert(p == base && nr_free_pages() == 1);
    free_pages(p, 2);
    free_page(base + 3);
    p = alloc_pages(4);
    assert(p == base && nr_free_pages() == 0);
    free_pages(p, 4);
    for (size_t i = 16; i-- > 4;) {
        free_page(base + i);
    }
    assert(nr_free_pages() == 16);
    p = alloc_pages(16);
    assert(p == base && nr_free_pages() == 0);
    free_pages(p, 16);
    for (size_t i = count; i-- > 0;) {
        ppn_t ppn = page2ppn(held[i]);
        if (ppn < first || ppn >= first + 16) {
            free_page(held[i]);
        }
    }
    assert(nr_free_pages() == baseline);
    buddy_validate();
    cprintf("[Buddy 2] exhaustion/non-buddies/recursive merge: PASS (%lu pages)\n", count);

    struct Page *slots[64] = {0};
    size_t slot_requests[64] = {0};
    unsigned int rng = 0x2413807;
    for (int step = 0; step < 1000; step++) {
        rng = rng * 1664525U + 1013904223U;
        size_t index = (rng >> 16) % 64;
        if (slots[index] != NULL) {
            assert(block_words(slots[index])[0] == index + 1);
            free_pages(slots[index], slot_requests[index]);
            slots[index] = NULL;
        } else {
            size_t request = (rng >> 24) % 33 + 1;
            p = alloc_pages(request);
            assert(p != NULL);
            for (size_t j = 0; j < 64; j++) {
                if (slots[j] != NULL) {
                    check_disjoint(p, rounded_pages(request), slots[j], rounded_pages(slot_requests[j]));
                }
            }
            slots[index] = p;
            slot_requests[index] = request;
            block_words(p)[0] = index + 1;
        }
    }
    for (size_t i = 0; i < 64; i++) {
        if (slots[i] != NULL) {
            assert(block_words(slots[i])[0] == i + 1);
            free_pages(slots[i], slot_requests[i]);
        }
    }
    assert(nr_free_pages() == baseline);
    buddy_validate();
    cprintf("[Buddy 3] 1000 deterministic mixed operations: PASS\n");
    cprintf("[Buddy] ALL TESTS PASSED; free pages restored: %lu\n", baseline);
}

#include <buddy_pmm.h>
#include <list.h>

/* lab2 buddy 2413807: order lists use absolute physical-page alignment. */
#define BUDDY_ALLOCATED (1UL << 2)
#define BUDDY_HEAD (1UL << 3)
static list_entry_t order_lists[BUDDY_MAX_ORDER + 1];
static size_t free_count;

static int request_order(size_t n) {
    if (n == 0 || n > ((size_t)1 << BUDDY_MAX_ORDER)) {
        return -1;
    }
    int order = 0;
    while (((size_t)1 << order) < n) {
        order++;
    }
    return order;
}

static void add_block(struct Page *base, int order) {
    base->property = (size_t)1 << order;
    SetPageProperty(base);
    list_add(&order_lists[order], &base->page_link);
}

static void buddy_init(void) {
    for (int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_init(&order_lists[order]);
    }
    free_count = 0;
}

static void buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    assert(base >= pages && n <= npage - page2ppn(base));
    for (struct Page *p = base; p < base + n; p++) {
        assert(PageReserved(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
        list_init(&p->page_link);
    }
    free_count += n;
    while (n != 0) {
        int order = 0;
        while (order < BUDDY_MAX_ORDER && ((size_t)1 << (order + 1)) <= n &&
               (page2ppn(base) & (((size_t)1 << (order + 1)) - 1)) == 0) {
            order++;
        }
        size_t block = (size_t)1 << order;
        add_block(base, order);
        base += block;
        n -= block;
    }
}

static struct Page *buddy_alloc_pages(size_t n) {
    int wanted = request_order(n);
    if (wanted < 0 || ((size_t)1 << wanted) > free_count) {
        return NULL;
    }
    int order = wanted;
    while (order <= BUDDY_MAX_ORDER && list_empty(&order_lists[order])) {
        order++;
    }
    if (order > BUDDY_MAX_ORDER) {
        return NULL;
    }
    struct Page *base = le2page(list_next(&order_lists[order]), page_link);
    list_del(&base->page_link);
    ClearPageProperty(base);
    while (order > wanted) {
        order--;
        add_block(base + ((size_t)1 << order), order);
    }
    size_t actual = (size_t)1 << wanted;
    for (struct Page *p = base; p < base + actual; p++) {
        assert(!PageReserved(p) && !(p->flags & BUDDY_ALLOCATED));
        p->flags = BUDDY_ALLOCATED;
        p->property = 0;
        set_page_ref(p, 0);
    }
    base->flags |= BUDDY_HEAD;
    base->property = actual;
    free_count -= actual;
    return base;
}

size_t buddy_block_pages(struct Page *base) {
    assert(base >= pages && page2ppn(base) < npage);
    assert((base->flags & (BUDDY_HEAD | BUDDY_ALLOCATED)) ==
           (BUDDY_HEAD | BUDDY_ALLOCATED));
    return base->property;
}

static void buddy_free_pages(struct Page *base, size_t n) {
    int order = request_order(n);
    assert(order >= 0);
    size_t actual = (size_t)1 << order;
    assert(buddy_block_pages(base) == actual);
    assert(page2ppn(base) + actual <= npage);
    for (struct Page *p = base; p < base + actual; p++) {
        assert(!PageReserved(p) && (p->flags & BUDDY_ALLOCATED));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }
    free_count += actual;
    while (order < BUDDY_MAX_ORDER) {
        size_t block = (size_t)1 << order;
        ppn_t other_ppn = page2ppn(base) ^ block;
        if (other_ppn < nbase || other_ppn + block > npage) {
            break;
        }
        struct Page *other = pages + (other_ppn - nbase);
        if (!PageProperty(other) || other->property != block) {
            break;
        }
        list_del(&other->page_link);
        ClearPageProperty(other);
        other->property = 0;
        if (other < base) {
            base = other;
        }
        order++;
    }
    add_block(base, order);
}

static size_t buddy_nr_free_pages(void) {
    return free_count;
}

void buddy_validate(void) {
    size_t total = 0;
    for (int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        size_t block = (size_t)1 << order;
        list_entry_t *head = &order_lists[order];
        for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
            struct Page *p = le2page(le, page_link);
            assert(PageProperty(p) && p->property == block);
            assert(page2ppn(p) >= nbase && page2ppn(p) + block <= npage);
            assert((page2ppn(p) & (block - 1)) == 0);
            assert(list_prev(list_next(le)) == le);
            assert(list_next(list_prev(le)) == le);
            for (size_t i = 0; i < block; i++) {
                assert(!PageReserved(p + i) && !((p + i)->flags & BUDDY_ALLOCATED));
                if (i != 0) {
                    assert(!PageProperty(p + i));
                }
            }
            total += block;
        }
    }
    assert(total == free_count);
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};

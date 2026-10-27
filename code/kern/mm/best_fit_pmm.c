#include <pmm.h>
#include <list.h>
#include <string.h>
#include <best_fit_pmm.h>
#include <stdio.h>

//lab2 function2-2 2413807
// 空闲链表按地址递增，便于合并；申请时再遍历选择“最小满足块”。
// 固定测试会直接保存/清空/恢复free_list和nr_free，不能另藏不一致的索引。
static free_area_t free_area;
#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void best_fit_check(void);

static void best_fit_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

static void insert_free_block(struct Page *base) {
    // 停在第一个地址不小于base的节点之前；到哨兵则等价于插入尾部。
    list_entry_t *le = list_next(&free_list);
    while (le != &free_list && le2page(le, page_link) < base) {
        le = list_next(le);
    }
    list_add_before(le, &base->page_link);
}

static void merge_free_block(struct Page *base) {
    // 先尝试与前块合并；合并后块头变为prev，后续检查必须从新块头继续。
    list_entry_t *le = list_prev(&base->page_link);
    if (le != &free_list) {
        struct Page *prev = le2page(le, page_link);
        if (prev + prev->property == base) {
            prev->property += base->property;
            ClearPageProperty(base);
            base->property = 0;
            list_del(&base->page_link);
            base = prev;
        }
    }
    // 再检查后块，处理“左空闲块 + 本次释放区间 + 右空闲块”三段合一。
    le = list_next(&base->page_link);
    if (le != &free_list) {
        struct Page *next = le2page(le, page_link);
        if (base + base->property == next) {
            base->property += next->property;
            ClearPageProperty(next);
            next->property = 0;
            list_del(&next->page_link);
        }
    }
}

static void best_fit_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    // 只有page_init交付的合法reserved区间才能转为空闲区。
    for (struct Page *p = base; p < base + n; p++) {
        assert(PageReserved(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }
    // property只对空闲块头有意义；中间页不单独进入链表。
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    insert_free_block(base);
    merge_free_block(base);
}

static struct Page *best_fit_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    // 总空闲页数足够仍可能没有连续满足块，因此还要完整查找。
    struct Page *best = NULL;
    for (list_entry_t *le = list_next(&free_list); le != &free_list;
         le = list_next(le)) {
        struct Page *p = le2page(le, page_link);
        // 仅在严格更小时替换候选，同大小自然保留先遇到的低地址块。
        if (p->property >= n && (best == NULL || p->property < best->property)) {
            best = p;
            // 精确匹配已经是最优，无须继续扫描。
            if (p->property == n) {
                break;
            }
        }
    }
    if (best == NULL) {
        return NULL;
    }
    // 从前端取走n页；先保存链表位置，再把剩余段的新块头放回原位置。
    size_t remaining = best->property - n;
    list_entry_t *prev = list_prev(&best->page_link);
    list_del(&best->page_link);
    ClearPageProperty(best);
    best->property = 0;
    if (remaining != 0) {
        struct Page *tail = best + n;
        tail->property = remaining;
        SetPageProperty(tail);
        list_add(prev, &tail->page_link);
    }
    nr_free -= n;
    return best;
}

static void best_fit_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    assert(base >= pages && (size_t)(base - pages) < npage - nbase);
    assert(n <= npage - nbase - (size_t)(base - pages));
    // 约定传入合法已分配区间，也允许归还较早分配块的一部分。
    // 这些断言检查边界和已知状态，并非对所有非法释放的完备检测。
    for (struct Page *p = base; p < base + n; p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    // 插入恢复地址顺序，再按物理连续关系与邻块合并；合并不改变总页数。
    insert_free_block(base);
    merge_free_block(base);
}

static size_t best_fit_nr_free_pages(void) {
    return nr_free;
}

// 注册到统一PMM接口；check继续指向课程原有检查函数。
const struct pmm_manager best_fit_pmm_manager = {
    .name = "best_fit_pmm_manager",
    .init = best_fit_init,
    .init_memmap = best_fit_init_memmap,
    .alloc_pages = best_fit_alloc_pages,
    .free_pages = best_fit_free_pages,
    .nr_free_pages = best_fit_nr_free_pages,
    .check = best_fit_check,
};

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the best fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
best_fit_check(void) {
    int score = 0 ,sumscore = 6;
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    // * - - * -
    free_pages(p0 + 1, 2);
    free_pages(p0 + 4, 1);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 1) && p0[1].property == 2);
    // * - - * *
    assert((p1 = alloc_pages(1)) != NULL);
    assert(alloc_pages(2) != NULL);      // best fit feature
    assert(p0 + 4 == p1);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    p2 = p0 + 1;
    free_pages(p0, 5);
    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
    #ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n",score, sumscore);
    #endif
}



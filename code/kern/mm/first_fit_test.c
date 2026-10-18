#include <pmm.h>
#include <stdio.h>

/* A public-interface regression missing from the provided First-Fit checks. */
void first_fit_regression_check(void) {
    size_t baseline = nr_free_pages();
    assert(baseline >= 4);
    struct Page *all = alloc_pages(baseline);
    assert(all != NULL && nr_free_pages() == 0);
    free_page(all);
    free_page(all + 1);
    assert(nr_free_pages() == 2);
    struct Page *pair = alloc_pages(2);
    assert(pair == all);
    assert(nr_free_pages() == 0);
    free_pages(pair, 2);
    free_pages(all + 2, baseline - 2);
    assert(nr_free_pages() == baseline);
    struct Page *restored = alloc_pages(baseline);
    assert(restored == all && nr_free_pages() == 0);
    free_pages(restored, baseline);
    assert(nr_free_pages() == baseline);
    cprintf("[First-Fit regression] tail insertion/coalescing: PASS (%lu pages restored)\n", baseline);
}

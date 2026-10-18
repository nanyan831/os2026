# 阶段 2：完整 Best-Fit 管理器

```text
[PROMPT]
任务：在 code/kern/mm/best_fit_pmm.c 的 lab2 function2-2 位置实现完整 Best-Fit，接入 pmm.c。
操作要求：真实修改文件；保留 basic_check 和 best_fit_check 的全部原始文本；标记学号为 2413807。
输出要求：完整管理器、默认 Best-Fit 配置、可复现的普通运行/测试/评分证据。

[RELY]
阅读 default_pmm.c、pmm.h、memlayout.h、list.h 的真实定义。
struct pmm_manager 包含 name/init/init_memmap/alloc_pages/free_pages/nr_free_pages/check。
free_area_t 包含 free_list 和 nr_free；Page.property 是空闲块页数，PG_property 只标记块头。
固定测试会直接保存、清空、恢复本文件的 free_list 和 nr_free；还会部分释放五页分配，并要求一页请求选择单页块而非两页块。
alloc_pages/free_pages 通过当前管理器派发；测试回调运行前 PMM 已初始化且有空闲页。

[GUARANTEE]
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
const struct pmm_manager best_fit_pmm_manager;
check 指向原有 best_fit_check；必要时添加前置声明。

[SPECIFICATION]
init/init_memmap
Pre-Condition：init 先于 memmap；待注册描述符均 reserved。
Post-Condition：维护按地址递增的空闲块链表与准确空闲页计数，每块只有头页 PG_property=1；初始化时清零 ref。
alloc_pages
Pre-Condition：n>0。
Post-Condition：返回最小满足块的前 n 页，若同大小取较低地址；返回区间不再空闲，余块位置、长度和计数正确。
Case 1：精确匹配，移除块；Case 2：较大块，保留 n 页之后的余块；Case 3：没有连续满足块或空链表，返回 NULL 且状态不变。
free_pages
Pre-Condition：合法的已分配连续区间，可为早先分配的子区间；不允许释放保留页或重复释放。
Post-Condition：ref 清零，空闲计数增加 n；按地址插入并与物理连续前后块合并，清除不再作为块头的标记。
nr_free_pages 返回准确计数，不依赖不可随固定测试一起恢复的缓存。
Requirements：固定测试不可修改；分配失败不破坏状态；完成后实际启用 best_fit_pmm_manager。
```

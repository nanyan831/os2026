# 阶段 1：Lab2 内存管理迁移与接口保护

**责任成员：** 王子卓（2411070）。**工作量：** 3 点。

本阶段按现有实现和真实验证材料整理，2026-10-27。成员责任见[等量分工](../division.md)，失败、修订与验证见[迭代记录](../iterations.md)。

```text
[PROMPT]
任务：以课程 Lab3 骨架为基础，在 code/kern/mm/pmm.c 和 best_fit_pmm.c 的 lab2 留空位置补齐上一实验的必要实现。
操作要求：直接修改真实工程文件，对照已有 Lab2 的 page_init 和 Best-Fit 迁移；保留课程固定检查函数、Lab3 原子操作和中断保护。
输出要求：可编译的物理页初始化、完整 Best-Fit 管理器、必要常量和可选择的 PMM 配置；说明迁移边界。

[RELY]
struct Page 包含 ref、flags、property、page_link；RV64 下实际大小为40字节。
PGSIZE=4096，DRAM_BASE=0x80000000，PHYSICAL_MEMORY_END=0x88000000，PHYSICAL_MEMORY_OFFSET=0xffffffff40000000。
get_memory_base/get_memory_size 提供 DTB 内存范围；pages 是描述符数组虚拟地址，npage 为物理页号上界，数组长度是 npage-nbase。
可使用 end、PADDR、pa2page、PPN、ROUNDUP、ROUNDDOWN、init_memmap、panic、cprintf 和框架 Page 状态宏。
Lab3 的 alloc_pages/free_pages/nr_free_pages 已通过 local_intr_save/restore 保护调用；memlayout.h 使用 atomic.h。
Makefile 会编译 kern/mm 下全部 C 文件，不能因为默认使用 First-Fit 就留下 Best-Fit 实现空缺。

[GUARANTEE]
static void page_init(void);
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
const struct pmm_manager best_fit_pmm_manager;
保持原有公开 PMM 接口，支持 PMM=first_fit|best_fit，默认保持 Lab3 的 first_fit。

[SPECIFICATION]
page_init
Pre-Condition：启动地址映射、内核栈和 DTB 解析可用；管理器已 init，但尚未登记空闲区。
Post-Condition：va_pa_offset、npage、pages 正确；固件、内核、Page 数组和对齐空隙均保留；只有元数据之后的完整页面进入管理器。
Case 1：合法 RAM。按 DTB 与框架上界确定管理末端，检查描述符数组能容纳后再初始化，并登记非空空闲区。
Case 2：基址不支持、零大小、加法溢出或空间不足。明确 panic，不越界初始化或登记零页区间。
Best-Fit
Pre-Condition：登记区间合法且描述符处于 reserved；申请 n>0，释放属于先前分配的合法区间。
Post-Condition：申请选择最小满足块，同大小取先遇到的低地址块；分裂、地址顺序、相邻合并及空闲页计数正确。
Case 3：精确匹配、分裂和无连续满足块分别处理；失败返回 NULL 且不破坏状态。
Requirements：保留 basic_check/best_fit_check 原文；保留 Lab3 的中断保护和原子位操作；不整份覆盖 pmm.c/memlayout.h，不迁入 Buddy、SLUB 或 MMU 探针。
```

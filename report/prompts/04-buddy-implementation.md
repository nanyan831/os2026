# 阶段 4：Buddy System 实现提示词

```text
[PROMPT]
任务：从零新增 code/kern/mm/buddy_pmm.c、buddy_pmm.h，接入现有 PMM；Makefile 支持 PMM=buddy，默认仍为 best_fit。
操作要求：真实修改工程，保持 First-Fit/Best-Fit 固定测试与官方注入流程；本提示词只规定实现接口，下一提示词独立设计测试。
输出要求：完整按阶空闲表、正确分配/释放与可重复选择配置；描述元数据和空间损耗。

[RELY]
struct Page 的 ref、flags、property、page_link；pmm.h 的六项回调、page2ppn、page2pa、pa2page；list.h 链表操作。
物理空闲区起点经过内核和描述符布局计算，通常不是最大阶对齐；初始化的描述符处于 reserved。
alloc_pages/free_pages 接口允许处理至少 n 页。内核为单 hart、无并发分配；不需要引入锁。

[GUARANTEE]
const struct pmm_manager buddy_pmm_manager;
六项管理器回调和 void buddy_check(void) 测试回调接入点。
size_t buddy_block_pages(struct Page *base); void buddy_validate(void);
新增头文件公开测试所需的只读状态检查接口，内部辅助函数 static。

[SPECIFICATION]
init/init_memmap
Pre-Condition：先 init，后将合法 reserved 区间交给 init_memmap。
Post-Condition：区间完整分解为以物理页号对齐的2的幂次块，计数不丢页，只有空闲块头带 PG_property，内部元数据状态清晰。
alloc_pages
Case 1：n>0，向上取整到2的幂，找到不小于目标阶的最小非空阶，递归拆分；返回对齐且不重叠块，实际占用页数可查询，nr_free 减少实际页数。
Case 2：n=0、超过最大阶或连续块不足，返回 NULL，状态不变。
free_pages
Pre-Condition：base 是先前分配的块头，n 的取整大小与实际块一致；禁止 interior free、double free、reserved free。
Post-Condition：按真实块大小回收，以物理页号异或找到同阶伙伴，只有伙伴确实空闲且同阶才合并，递归向上；nr_free 增加实际页数。
buddy_validate 检查每阶块大小、对齐、边界、链表和空闲总计，不修改分配状态。
Requirements：不修改 struct Page 布局；可用附加 flags 区分 allocated 和 allocated-head；ref 保持正常语义；不得假设空闲区天然最大阶对齐。
```

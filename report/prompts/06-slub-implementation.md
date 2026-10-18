# 阶段 6：简化 SLUB Challenge 实现提示词

```text
[PROMPT]
任务：从零新增 code/kern/mm/slub.c、slub.h，基于当前页分配器实现两层对象分配，作为 SLUB 思想的简化教学实现。
操作要求：真实接入内核的可选 SLUB_TEST 配置；不得声称实现 Linux 完整 SLUB；保持默认评分路径不变。
输出要求：对象大小类别、页内空闲对象组织、对象验证和空页回收；下一阶段单独设计测试。

[RELY]
底层 alloc_pages/free_pages/nr_free_pages；上层只能使用内核已提供的类型、链表、字符串操作和 VA/PA 转换。
Best-Fit 精确分配页数，Buddy 向上取整；可用 buddy_block_pages 确认 Buddy 实际占用。
当前内核单 hart，无并发；对象要求至少16字节对齐。

[GUARANTEE]
void *slub_alloc(size_t size); int slub_free(void *ptr);
size_t slub_pages_in_use(void); void slub_check(void);
辅助函数和缓存状态限制在模块内；不修改已有 PMM 公开接口。

[SPECIFICATION]
alloc
Pre-Condition：物理页管理器已初始化。
Case 1：1..2048字节，选择16/32/64/128/256/512/1024/2048类别，先复用非满 slab 页中的空闲对象，无可用对象才申请新页。对象区和头部不重叠。
Case 2：更大对象，检查长度加法溢出后申请连续页，保存真实分配信息。
Case 3：零大小、溢出或资源不足，返回 NULL，不泄漏。
free
Case 1：合法对象，回收到其类别；满页重新成为可分配页；最后一个对象释放时归还整页。
Case 2：合法大对象，归还对应完整分配块。
Case 3：NULL 为无操作；非对象起点或已释放对象返回 -1，不破坏状态。
Post-Condition：活跃对象不重叠，返回值对齐；所有对象释放后本模块页占用回到0，底层资源恢复。
Requirements：用 bitmap 验证对象是否正在分配；每页只有一个页头；不保留空缓存页；计数考虑 Buddy 的取整占用。
```

# 追加阶段：First-Fit 尾插与合并回归

```text
[PROMPT]
任务：复核课程提供的 First-Fit 链表尾插，增加独立回归验证其连续页回收；发现问题后做最小修复。
操作要求：先用原实现实际复现，不修改 basic_check/default_check；修复后保留原固定测试与新回归。
输出要求：失败原始日志、修复差异、两个 RAM 配置下的通过证据。

[RELY]
default_init_memmap/default_free_pages 按地址查找插入点，在尾部 list_add 后没有 break。
list_add 的 __list_add 会同时修改 prev/next；循环继续经过新插入的节点可能把其 prev 置为自身。
公开接口 alloc_pages/free_pages/nr_free_pages；启动后初始空闲区为一个连续块。

[GUARANTEE]
void first_fit_regression_check(void);
固定测试代码原样；管理实现仅修复已证实的插入后循环问题。

[SPECIFICATION]
Pre-Condition：First-Fit 固定测试已经完成，所有页面已恢复。
Case 1：申请全部可用连续区间，清空自由链表；依次释放前两个相邻单页，申请2页必须成功返回同一基址。该场景可击穿尾插后未与前块合并的错误。
Case 2：重新释放该2页块及剩余区域，必须合并成原来的完整连续区间，能再次整块申请；最终全部恢复。
Case 3：如果原实现失败，检查尾插继续循环造成的反向链接，再在成功插入后结束循环；初始化插入的同类问题同步修复。
Post-Condition：原固定测试及新增回归均通过，空闲页计数完全恢复。
Requirements：不绕过断言，不硬编码空闲地址和页数，不扩展为无关分配策略改写。
```

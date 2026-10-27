# 阶段 7：First-Fit 尾插回归与中断保护验证

**责任成员：** 王子卓（2411070）。**工作量：** 2 点。

本阶段按现有实现和真实验证材料整理，2026-10-27。成员责任见[等量分工](../division.md)，失败、修订与验证见[迭代记录](../iterations.md)。

```text
[PROMPT]
任务：通过公开 PMM 接口独立验证尾插合并和中断状态保留，复现 First-Fit 实际问题后只做最小修复。
操作要求：不修改 basic_check/default_check 或 Best-Fit 固定检查；用临时附加测试驱动当前管理器。
输出要求：失败日志、两处尾插退出循环修复，以及 First-Fit/Best-Fit 的通过与资源恢复证据。

[RELY]
附加测试在 tools/test_lab3_extra.c 的 check_tail_insertion、check_interrupt_state 中组织。
alloc_pages/free_pages/nr_free_pages 为公开接口，管理器自检后可用区恢复为一个连续区间。
原 default_init_memmap/default_free_pages 尾部 list_add 后继续遍历，可能再次处理新节点，破坏合并使用的前驱关系。
local_intr_save/restore 保存先前 SIE 状态，恢复时只能恢复到原状态；测试发生在时钟尚未开放的阶段。

[GUARANTEE]
static void check_tail_insertion(void);
static void check_interrupt_state(void);
测试入口由 lab3_extra_check 聚合；保持两个固定检查区域原文。
default_init_memmap/default_free_pages 仅在已经完成尾插的分支补上必要的循环结束。

[SPECIFICATION]
Pre-Condition：当前管理器初始化与自检已完成，空闲页数由运行时 nr_free_pages 获取，不硬编码地址和页数。
Case 1：申请整个连续可用区间，依次释放两个非相邻页，重新申请时地址正确；释放全区后计数恢复。
Case 2：再申请整个区间，依次释放相邻两页，2页申请必须返回该连续区间；此场景应暴露尾插后前驱关系损坏造成的合并失败。
Case 3：仅在实际复现失败后修复算法循环；记录原始 panic/超时，不能把终止失败运行算作正常关机。
Case 4：分别从 SIE=0、SIE=1 状态调用分配、统计和释放，调用后保留原状态并恢复全部资源。
Case 5：两层 local_intr_save 后，内层恢复仍关闭中断，外层恢复才回到原开启状态。
Post-Condition：两个管理器均通过回归，资源计数等于初值，附加测试不留在正式 kern 初始化中。
Requirements：使用可观察结果验证，不读取算法私有链表替代行为测试；SIE 检查不被描述为多核竞争验证。
```

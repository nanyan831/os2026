# 01 Lab2 必要实现迁移规格

本文件归档本次执行采用的模块约束，用于复现；不是完整聊天逐字稿。

## [PROMPT]

在独立 Lab3 分支中，以课程 Lab3 骨架为基础，补齐 Lab2 留空的物理页初始化和 Best-Fit。修改真实工程文件，只迁移必要实现，保留官方检查函数、Lab3 中断保护和原子操作。对相邻尾块释放增加回归验证；确有失败时只修复算法实现，不修改检查函数。

## [RELY]

- 参考上一实验当前 `page_init()`、Best-Fit 算法。
- `get_memory_base/get_memory_size` 提供 DTB 范围。
- `Page`、`nbase`、`PADDR/pa2page` 和 `pmm_manager` 的含义以 Lab3 头文件为准。
- Lab3 的 `alloc_pages/free_pages/nr_free_pages` 已有 `local_intr_save/restore`；`memlayout.h` 使用 `atomic.h`。
- 每个源码文件仍会编译，即使该管理器暂未被选择。

## [GUARANTEE]

实现 `page_init()`、Best-Fit 初始化/登记/分配/释放/统计及管理器实例；增加构建选择 `PMM=first_fit|best_fit`。保留原有内存接口、课程自检以及 DTB 初始化和查询接口。

## [SPECIFICATION]

- 前置条件：内核栈和映射可用，DTB 已解析，PMM 空状态已初始化。
- 后置条件：Page 数组覆盖受管理 RAM；固件、内核、描述符和对齐空隙保留；只登记完整空闲页。
- 检查范围非空、加法溢出和描述符容量；使用当前结构体实际大小。
- Best-Fit 在所有满足块中选择最小块，同大小保留先遇到的低地址块；释放后恢复地址顺序并合并物理相邻块。
- 每个公开接口返回时保持调用前 SIE 状态；不得整份用 Lab2 `pmm.c/memlayout.h` 覆盖 Lab3。
- 不迁入 Buddy、SLUB 或 MMU 探针；固定检查函数原文保留。

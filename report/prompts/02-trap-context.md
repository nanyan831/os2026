# 02 Trap 上下文与汇编入口规格

本文件归档本次执行采用的模块约束，用于复现；不是完整聊天逐字稿。

## [PROMPT]

补齐 `kern/trap/trapentry.S` 两处 function1 标记。以实际 `SAVE_ALL` 和 `struct trapframe` 为唯一布局依据，实现汇编入口调用 C 处理函数、随后恢复现场。保持原函数签名和框架。

## [RELY]

RV64 中 `REGBYTES=8`，36 个槽位共 288 字节；0–31 为通用寄存器，32/33 为 status/epc，34/35 为 badvaddr/cause。原 x2 经 `sscratch` 保存至第 2 槽。`trap(struct trapframe *tf)` 按 RISC-V ABI 接收参数。硬件进入 Trap 时已保存 SPP/SPIE 并清 SIE。

## [GUARANTEE]

补齐 `RESTORE_ALL` 和 `__alltraps`；保持 `SAVE_ALL`、`struct trapframe`、`trap()` 和 `__trapret` 的公开接口以及 `sret`。

## [SPECIFICATION]

- 前置条件：发生在本实验内核 S 模式中，当前内核栈可写。
- 保存完成后将帧地址作为第一个参数交给 C 函数，保持 ABI 栈对齐。
- 处理函数可以修改帧内 epc；恢复流程必须使用修改后的帧内容。
- 恢复所有可写通用寄存器和返回所需 CSR。恢复过程中所用临时寄存器最终也要恢复。
- 所有以当前帧为基址的访存结束后，最后恢复原 x2，不依赖一个已经改变的 sp。
- 不为返回无条件清 SPP，也不在恢复尚未完成时开启 SIE；由 `sret` 按保存的状态返回。
- 用独立哨兵寄存器测试和 GDB 检查实际入口、帧布局和返回，不能只凭打印文本判断上下文恢复。

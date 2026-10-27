# 03 中断与异常处理规格

本文件归档本次执行采用的模块约束，用于复现；不是完整聊天逐字稿。

## [PROMPT]

在 `kern/trap/trap.c` 的 function2 标记区域补齐两个处理函数及必要的局部声明。保持区域外既有函数原文不变。不要为评分构建增加伪造输出或关闭正常功能的分支。

## [RELY]

使用实际 `IRQ_*`、`CAUSE_*`、`struct trapframe`、`clock_set_next_event()`、volatile `ticks`、已有 `print_ticks()`、`print_trapframe()` 和 `sbi_shutdown()`。官方评分需要可识别的异常、软件中断、未知中断文本。本地 `TICK_NUM=100`；指导书未给关机阈值数值，本次明确采用 `SHUTDOWN_AFTER=10`。

## [GUARANTEE]

实现 `void interrupt_handler(struct trapframe *tf)` 和 `void exception_handler(struct trapframe *tf)`。允许新增仅本模块可见的指令长度助手和统计状态，不改其他函数。

## [SPECIFICATION]

- 中断原因号去掉最高中断标志位后分类；未知原因号给出原因及完整帧。
- 每次 S 时钟中断重新设置下一事件，ticks 恰增加一次；每 100 次打印 `100 ticks`；第 10 次统计输出后通过 SBI 真正关机。
- 时钟中断不修改返回 epc。
- 非法指令与断点分别输出 `Illegal instruction at`、`Breakpoint at` 及地址；根据可读内核指令的实际 16/32 位编码后移 epc。
- 不假定 stval 一定含有完整指令；对于不支持的更长指令明确终止，不误报支持。
- 其他未要求恢复的异常只输出帧，不任意跳过指令。
- 范围仅为本实验可读内核地址与整数现场，不宣称已实现用户态安全取指、多核调度或完整异常恢复。

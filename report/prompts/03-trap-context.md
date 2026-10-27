# 阶段 3：Trap 上下文恢复与汇编入口

**责任成员：** 刘宸旭（2414139）。**工作量：** 4 点。

本阶段按现有实现和真实验证材料整理，2026-10-27。成员责任见[等量分工](../division.md)，失败、修订与验证见[迭代记录](../iterations.md)。

```text
[PROMPT]
任务：补齐 code/kern/trap/trapentry.S 两处 lab3 function1 标记，实现 RESTORE_ALL 和 __alltraps 调用 C 处理函数的路径。
操作要求：依据真实 SAVE_ALL 和 struct trapframe 实现；保留已填写课程标记、SAVE_ALL 原文、结构体布局和 trap 函数签名。
输出要求：形成保存现场、调用处理函数、恢复现场和 sret 返回的完整闭环，并给出帧偏移与恢复顺序说明。

[RELY]
RV64 的 REGBYTES=8，struct trapframe 共36个槽位、288字节；0..31为通用寄存器，32为status，33为epc，34/35为badvaddr/cause。
SAVE_ALL 先通过 sscratch 保存原 sp，再将其存入第2槽；保存结束后 sp 指向帧基址。
void trap(struct trapframe *tf) 按 RISC-V ABI 通过 a0 接收帧指针，并允许修改帧内 epc。
硬件入 Trap 时保存先前特权级到 SPP、先前中断状态到 SPIE，并清 SIE；本次主要处理 S 模式内核中的 Trap。

[GUARANTEE]
.macro RESTORE_ALL
__alltraps
保持现有 __trapret、sret、SAVE_ALL、struct trapframe 和 void trap(struct trapframe *tf) 接口。

[SPECIFICATION]
RESTORE_ALL
Pre-Condition：当前 sp 指向由 SAVE_ALL 产生的合法帧，C 处理函数已返回，帧可能包含更新后的 epc。
Post-Condition：所有可写通用寄存器及返回所需 sstatus/sepc 恢复，返回后原执行流能够继续。
Case 1：先使用帧基址恢复返回 CSR，再恢复通用寄存器；作为临时寄存器使用的寄存器最终也恢复。
Case 2：原 sp 来自第2槽，必须在所有依赖当前帧基址的访存结束后恢复；不得提前改变 sp 或靠错误的统一偏移恢复它。
__alltraps
Pre-Condition：SAVE_ALL 已构造帧，栈满足 ABI 对齐。
Post-Condition：trap 接收到当前帧地址；C 函数返回后 sp 仍指向该帧，并衔接现有 __trapret。
Case 3：把帧地址作为第一个实参调用 trap，保持后续恢复需要的信息和原调用现场。
Requirements：不无条件清 SPP，不在恢复途中主动打开 SIE；由 sret 按保存的 SPP/SPIE 返回。badvaddr/cause 用于诊断，不作为可恢复通用寄存器写回。
```

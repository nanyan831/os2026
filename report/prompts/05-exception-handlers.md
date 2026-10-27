# 阶段 5：断点、非法指令与 EPC 长度处理

**责任成员：** 缪臻（2413807）。**工作量：** 4 点。

本阶段按现有实现和真实验证材料整理，2026-10-27。成员责任见[等量分工](../division.md)，失败、修订与验证见[迭代记录](../iterations.md)。

```text
[PROMPT]
任务：在 code/kern/trap/trap.c 的 function2 允许区域实现 exception_handler 及内部指令长度助手。
操作要求：使用真实 CAUSE 宏和 trapframe 字段，严格保留处理区域外函数；按指令编码更新返回位置，不统一假定 epc+4。
输出要求：16/32位断点与非法指令都能处理后返回，未知异常输出帧，规定输出文本可以被官方测试识别。

[RELY]
void exception_handler(struct trapframe *tf) 接收同步异常帧；tf->cause 表示原因，tf->epc 指向触发异常的指令地址。
CAUSE_ILLEGAL_INSTRUCTION=2，CAUSE_BREAKPOINT=3；可使用 cprintf、print_trapframe 和 panic。
本实验异常指令位于可读的内核地址；不能依赖 stval/badvaddr 一定提供完整指令。
16位编码的最低两位不为11；标准32位编码的最低两位为11且最低5位不全为1。
边界测试编码为断点9002/00100073、非法指令0000/0000007b。

[GUARANTEE]
static size_t exception_instruction_length(uintptr_t epc);
void exception_handler(struct trapframe *tf);
保持 trap_dispatch、trap、print_trapframe 和已有公开接口不变。

[SPECIFICATION]
exception_instruction_length
Pre-Condition：epc 为本实验可读内核指令地址。
Case 1：16位编码，返回2；Case 2：标准32位编码，返回4；Case 3：更长且本实现不支持的编码，明确终止，不错误声称支持。
exception_handler
Pre-Condition：tf 为合法同步异常帧，已识别异常的 epc 可读取。
Case 4：非法指令。输出 Illegal instruction at 和地址，依据实际长度后移 tf->epc。
Case 5：断点。输出 Breakpoint at 和地址，依据实际长度后移 tf->epc。
Case 6：其他异常。调用 print_trapframe，不擅自跳过未知指令或伪造恢复。
Post-Condition：已支持异常返回到对应下一条指令，既不重复陷入原指令，也不跳过后续正常指令。
Requirements：不修改通用寄存器现场；范围限于本实验整数内核路径，不将它描述为完整用户态安全取指或通用异常恢复。
```

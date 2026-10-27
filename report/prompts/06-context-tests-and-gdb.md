# 阶段 6：寄存器上下文独立测试与 GDB 往返验证

**责任成员：** 刘宸旭（2414139）。**工作量：** 3 点。

本阶段按现有实现和真实验证材料整理，2026-10-27。成员责任见[等量分工](../division.md)，失败、修订与验证见[迭代记录](../iterations.md)。

```text
[PROMPT]
任务：独立验证 Trap 保存与恢复，不只依赖打印标记；在附加汇编和 GDB 中检查寄存器、帧、原栈和返回位置。
操作要求：附加测试只注入临时副本；保持官方 test_lab3_context.S 不变；测试函数自身遵守调用方 ABI。
输出要求：哨兵寄存器检查、具名结果、真实 GDB 会话、源版本指纹和断言失败时的原始输出。

[RELY]
RESTORE_ALL、__alltraps、trap 和 __trapret 已形成闭环，帧为288字节。
附加源文件为 code/tools/test_lab3_extra_context.S 与 test_lab3_extra.c，临时复制到 kern/extra。
公开测试接口 uintptr_t lab3_extra_register_context(void)；Python 验证入口 gdb_trace(qemu, label)。
ra、t、s、a 类寄存器可设置独立哨兵；gp/tp 保留给 C 处理函数使用并比较前后原值。
GDB 可通过 QEMU stub 观察 __alltraps、trap、__trapret 和被打断PC；不使用旧构建硬编码地址。

[GUARANTEE]
uintptr_t lab3_extra_register_context(void);
verify_lab3.py 中可复现的 gdb_trace(qemu, label)。
测试返回0表示比较通过，非0由调用者断言；记录真实原 sp、a0、sepc、scause 和 sstatus。

[SPECIFICATION]
寄存器测试
Pre-Condition：内核栈和 Trap 路径可用；调用方现场必须先由测试自身妥善保存。
Case 1：给可设置的通用寄存器写入独立值后触发强制32位断点，返回后逐一比较；a0 必须在作为比较暂存前核对原值。
Case 2：比较原 sp、gp、tp；恢复测试调用者的被调用者保存寄存器及返回地址，不能让测试自身破坏 ABI。
GDB 往返
Case 3：在首次真实时钟进入时保存原 sp/epc；在 C 函数机器入口确认 a0 等于帧地址、帧长288、帧内原 sp/epc 正确。
Case 4：在 __trapret 检查帧基址和时钟 epc 未变；返回原 PC 后确认原 sp 及 SIE 恢复。
Post-Condition：具名 GPR 和 GDB 断言均通过，输出可回读；测试进程和自己的调试 QEMU 正确结束。
Requirements：完整保留源码与测试指纹；区分真实执行、GDB检查和已保存日志回读；不把整数现场检查扩展为浮点/向量上下文证明。
```

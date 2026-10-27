# 阶段 4：中断分发、连续时钟与自动关机

**责任成员：** 王子卓（2411070）。**工作量：** 4 点。

本阶段按现有实现和真实验证材料整理，2026-10-27。成员责任见[等量分工](../division.md)，失败、修订与验证见[迭代记录](../iterations.md)。

```text
[PROMPT]
任务：在 code/kern/trap/trap.c 的 function2 允许区域实现 interrupt_handler，完成中断类型提示、时钟重设、计数和关机。
操作要求：保留区域外的函数和已有 print_ticks；使用框架 IRQ 宏、clock 接口及 SBI 声明，不修改评分断言。
输出要求：可持续产生时钟中断，每100次输出统计，第10次统计后真正关机；其他中断给出类型或完整兜底信息。

[RELY]
void interrupt_handler(struct trapframe *tf) 接收 cause 含最高中断标志位的帧；trap_dispatch 已完成中断/异常入口分类。
clock_set_next_event() 使用一次性 SBI 定时器；volatile ticks 记录次数，TICK_NUM=100。
框架有 IRQ_U/S/H/M_SOFT、IRQ_U/S/H/M_TIMER、IRQ_U/S/H/M_EXT 等宏，以及 print_trapframe、print_ticks 和 sbi_shutdown。
指导书命名 SHUTDOWN_AFTER 但未给数值；本实现显式采用10，需保留这一参数说明。
官方测试要求可识别 Supervisor software interrupt、Unknown interrupt: cause 15、100 ticks 等输出。

[GUARANTEE]
void interrupt_handler(struct trapframe *tf);
允许指定区域内的必要模块状态和声明；复用 clock_set_next_event、print_ticks、sbi_shutdown。

[SPECIFICATION]
Pre-Condition：stvec 和 Trap 返回路径可用，tf 指向合法帧；真实时钟路径已经初始化 clock。
Case 1：IRQ_S_TIMER。每次重设下一事件，ticks 恰增加一次；每达到100的整数倍调用已有统计输出，并累计统计次数。
Case 2：第10次统计输出。调用 sbi_shutdown 结束正常运行；不能靠超时终止或伪造退出状态认定成功。
Case 3：已定义的其他软件、时钟或外部中断。输出对应类型提示；本实验不扩展为完整外设驱动。
Case 4：未知原因号。去掉最高中断标志位后输出原因，并调用 print_trapframe。
Post-Condition：未关机时正确返回原执行流，时钟能够再次到来；中断处理不改变 tf->epc。
Requirements：不得只计数不重设下一事件；不得为评分配置单独关闭正常关机；输出及退出码均来自真实内核/QEMU。
```

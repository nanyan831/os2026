# 阶段 8：异常边界、综合验收与交付归档

**责任成员：** 缪臻（2413807）。**工作量：** 2 点。

本阶段按现有实现和真实验证材料整理，2026-10-27。成员责任见[等量分工](../division.md)，失败、修订与验证见[迭代记录](../iterations.md)。

```text
[PROMPT]
任务：汇集各模块已有独立验证，补齐异常长度边界，完成两套 QEMU 的综合矩阵、真实证据和交付包。
操作要求：在最终源码的临时副本运行官方测试和附加检查；固定测试文件不变；原始失败与适配后成功分别留存。
输出要求：两套矩阵、构建/评分/串口日志、真实终端截图、指纹与交付审计、可复现的源码和报告归档。

[RELY]
官方 grade.sh 三组分值35/25/40，分别检查断点上下文、非法指令与兜底、持续时钟达到200 ticks。
verify_lab3.py 的 grade/boot/extra/gdb_trace 组成6项场景；阶段6和7提供上下文、GDB、PMM和SIE独立检查。
make grade 在临时目录注入；原工程不能出现 kern/test、kern/extra、test_lab3.h 或已注入标记。
正常配置每100 ticks统计一次，共10次后通过 SBI 关机；两个 QEMU 分别为6.2/OpenSBI0.9、7.0/OpenSBI1.0。
capture_evidence.py 生成真实终端截图，audit_lab3.py 校验框架、来源指纹、链接与图片。

[GUARANTEE]
verify_lab3.py 中 grade(qemu,label)、boot(qemu,label,pmm)、extra(qemu,label,pmm) 及矩阵入口。
report/logs 中两套结果与原始证据，report/images 中五张截图，report/audit.json。
来自本地 Git 提交的交付 ZIP 与 SHA-256，保留 wrapper 的0755权限与LF行尾。

[SPECIFICATION]
Pre-Condition：阶段1至7的接口完整，官方五个测试/注入文件与原始SHA-256一致。
Case 1：在临时副本强制9002/00100073断点和0000/0000007b非法编码，结合反汇编确认长度及返回标记。
Case 2：分别运行官方评分、两种PMM普通关机、两种PMM附加回归和GDB，共6项；记录退出码、是否超时、QEMU路径及源码指纹。
Case 3：普通计时必须有10行100 ticks并真实退出；未知中断/异常的构造帧测试应明确其证据范围。
Case 4：真实终端执行构建/评分，或明确回读命名的原始日志和JSON；图片不得代替运行数据。
Case 5：核对Git归档CRC、源码和固定测试指纹、执行权限；解压重建后比较内核镜像内容。
Post-Condition：矩阵、日志和截图对应同一实现，证据完整；代码、报告与交付版本一致。
Requirements：透明说明payload适配；保留真实失败；不纳入bin/obj/.git/缓存；本地提交与远程推送分开，推送须有明确授权。
```

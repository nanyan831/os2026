# 南开大学操作系统实验 2026 — Lab3

本分支 `lab3` 基于课程 `lab3_new` 骨架完成中断与异常支持，沿用小组仓库 `code/`、`report/` 的交付布局。课程所说的 `labcodes/lab3` 对应本分支的源码和报告目录。

- [实验报告](report/report.md)
- [四段式执行规格与提示词](report/prompt.md)
- [真实失败与修复记录](report/iterations.md)
- [QEMU 6.2 验证矩阵](report/logs/verification-qemu6.json)
- [QEMU 7.0 验证矩阵](report/logs/verification-qemu7.json)
- [交付审计](report/audit.json)
- [实际终端截图](report/images/)

实现包括必要的 Lab2 内存管理迁移、Trap 入口与上下文恢复、16/32 位断点及非法指令处理、连续时钟计数、十次统计输出后关机。另修复 First-Fit 尾插循环和启动过渡映射。两个 QEMU 版本各通过 6/6 项验证，官方评分均为 100/100。

**评分运行使用已说明的启动参数适配器。** 原始 `loader` 方式在本机停留于固件；适配器仅将课程固定 raw loader 参数转换为 `-kernel`，执行真实 QEMU，官方五个测试/注入文件逐字节保持不变。原始失败和适配后的结果分别保留，详见报告。

## 构建与运行

宿主默认使用 PowerShell 7。执行 `wsl -d Ubuntu-22.04` 后，在 WSL 中运行：

```bash
cd '/mnt/d/缪臻/日常应用/南开大学/大三上/操作系统/labcodes-lab3/code'
make clean
make -j2
make grade QEMU=/usr/bin/qemu-system-riscv64
make -j2
make qemu QEMU=/usr/bin/qemu-system-riscv64
```

默认 First-Fit；切换管理器前清理目标文件：

```bash
make clean
make -j2 PMM=best_fit
make qemu PMM=best_fit QEMU=/usr/bin/qemu-system-riscv64
```

正常运行输出十行 `100 ticks` 后通过 SBI 关机。`SHUTDOWN_AFTER=10` 是本次显式采用的值，指导书未给数值。提前退出交互式 QEMU：`Ctrl+A`，松开后按 `X`。

完整复现已验证的矩阵与审计：

```bash
python3 tools/verify_lab3.py --stage all --qemu /usr/bin/qemu-system-riscv64 --label qemu6
python3 tools/verify_lab3.py --stage all --qemu /opt/qemu/bin/qemu-system-riscv64 --label qemu7
python3 tools/audit_lab3.py
```

两条 QEMU 路径是本机路径，其他机器自行替换。测试在临时副本注入，源码中不要手工加入 `kern/test`、`kern/extra` 或测试 include。`make test` 不代替 `make grade`。

## 调试与证据

```bash
make debug QEMU=/usr/bin/qemu-system-riscv64  # 终端一
make gdb                                  # 终端二，支持 gdb-multiarch
python3 tools/verify_lab3.py --stage gdb --label qemu6
python3 tools/capture_evidence.py           # 需 Xvfb、xterm、xwininfo、ImageMagick
```

验证使用 GCC 10.2.0、QEMU 6.2/OpenSBI 0.9 和 QEMU 7.0/OpenSBI 1.0。精确工具信息见 [环境日志](report/logs/environment.log)。`bin/`、`obj/` 和缓存不纳入 Git；固定测试指纹、实际日志和截图纳入报告。

小组成员沿用仓库登记：刘宸旭 2414139、王子卓 2411070、缪臻 2413807。本轮由缪臻发起 Codex 全量实施；Lab3 人工分工和现场验收由小组据实际情况核对，未把 Lab2 分工直接当作本轮贡献。

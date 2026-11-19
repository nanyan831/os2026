# Lab5 用户程序与 COW

姓名与学号为缪臻（2413807），沿用 Lab4 报告中的信息。实现四个基础模块和 Copy on Write Challenge，默认使用 COW fork。

实验报告见 [report.md](../report/report.md)，COW 状态设计与测试说明见 [Challenge.md](../report/Challenge.md)。实际运行证据保存在 [report/logs](../report/logs)。

小组责任与九阶段安排见[小组分工](../report/division.md)，各阶段的四段式规格见[提示词汇总](../report/prompt.md)。

在 PowerShell 7 中进入实验环境。

```powershell
wsl -d Ubuntu-22.04 --cd 'D:\缪臻\日常应用\南开大学\大三上\操作系统\labcodes-lab5'
```

在 WSL 中执行。

```bash
make -j4
make qemu QEMU=/usr/bin/qemu-system-riscv64
# 退出 QEMU 使用 Ctrl+A，然后 X
make grade QEMU=/usr/bin/qemu-system-riscv64
python3 tools/run_checks.py
make grade QEMU=/opt/qemu/bin/qemu-system-riscv64
python3 tools/run_checks.py --qemu /opt/qemu/bin/qemu-system-riscv64
python3 tools/trace_gdb.py
```

`run_checks.py` 运行 cowtest、forktree、waitkill，检查真实串口输出和最终内存回收。`trace_gdb.py` 使用本机带调试符号的 QEMU 7.0 和两个 GDB，端口为 1235。单独调试可用 `make debug` 和 `make gdb`。

`make build-程序名` 会改变后续启动的测试程序。恢复默认 hello 可运行 `make build-hello`。若需比较普通复制 fork，可清理后使用 `make build-hello DEFS+=-DFORK_EAGER`。COW 自检应使用默认 COW 模式。

课程评分器仅调整了启动参数来源，使其读取 Makefile 中的 QEMUOPTS，适配当前 OpenSBI。课程的测试断言、期望输出和分值均保留。完整原始 ZIP 在上级目录。

本分支包含源码、Markdown 报告和测试记录。课程平台提交由小组另行完成。

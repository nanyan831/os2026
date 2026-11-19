# 南开大学操作系统实验五 · 用户程序与 COW

- [实验源码与课程测试](code/)
- [实验报告](report/report.md)
- [小组分工](report/division.md)
- [九阶段四段式提示词](report/prompt.md)
- [COW Challenge 设计与测试](report/Challenge.md)
- [双 GDB 调试与 TCG 分析](report/GDB调试.md)
- [实际运行日志](report/logs/)

QEMU 6.2.0 和 7.0.0 的课程评分均为 **130/130**。两版的 COW、forktree、waitkill 补充测试均通过，双 GDB 已记录 U 态 ecall 与 sret 返回。代码默认采用 COW fork。

在安装 RISC-V GCC、QEMU、GDB 和 Python 3 的 Ubuntu 或 WSL 中运行。

```bash
cd code
make -j4
make qemu QEMU=/usr/bin/qemu-system-riscv64
make grade QEMU=/usr/bin/qemu-system-riscv64
python3 tools/run_checks.py
```

QEMU 使用 Ctrl+A 然后 X 退出。可通过 QEMU 参数指定其他可执行文件。运行 `python3 tools/trace_gdb.py` 可在本实验环境的带符号 QEMU 7.0 上复现双 GDB，端口为 1235。

所有核心源码、报告和测试记录的 SHA-256 见 [SHA256SUMS](SHA256SUMS)。分工文档按模块责任编排，人工复核与课程验收按实际记录确认。

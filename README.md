# 南开大学操作系统实验四 · 进程管理

- [实验源码与课程测试](code/)
- [实验报告与 Challenge 解答](report/report.md)
- [小组分工](report/division.md)
- [各阶段四段式提示词](report/prompt.md)
- [启动、评分与回滚日志](report/logs/)

在装有 RISC-V GCC、QEMU、GDB 和 Python 3 的 Ubuntu 或 WSL 中运行：

```bash
cd code
make -j4
make qemu
make grade
```

QEMU 6.2.0 和 7.0.0 的课程评分均为 30/30。可在命令末尾用 `QEMU=/path/to/qemu-system-riscv64` 选择可执行文件。

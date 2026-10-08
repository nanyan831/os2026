# 南开大学操作系统实验（2026）

小组人数：3 人。组长：刘宸旭。成员及已确认分工如下。

| 成员 | 学号 | 分工 |
|---|---|---|
| 刘宸旭（组长） | 2414139 | 统筹 |
| 王子卓 | 2411070 | 练习一：理解内核启动中的程序入口操作 |
| 缪臻 | 2413807 | 练习二：使用 GDB 验证启动流程 |

## Lab 1：最小可执行内核

- [实验报告](lab1/实验报告.md)
- [真实 Ubuntu 编译与运行截图](lab1/images/qemu_result.png)
- [真实 Ubuntu GDB 截图](lab1/images/gdb_trace.png)

### 环境

在 Ubuntu 中使用 `make`、`riscv64-unknown-elf-gcc`、`riscv64-unknown-elf-binutils`、`qemu-system-riscv64` 和 `gdb-multiarch`。本机使用 WSL 2 Ubuntu 22.04。Makefile 也支持优先使用 `riscv64-unknown-elf-gdb`。

### 编译与运行

```bash
cd lab1
make
make qemu
```

看到 `(THU.CST) os is loading ...` 后内核会持续循环。按 `Ctrl+A`，松开后按 `X` 退出 QEMU。

### 手工调试

两个 Ubuntu 终端均进入 `lab1`。终端 A 执行 `make debug`，终端 B 执行 `make gdb`。

```gdb
p/x $pc
x/6i $pc
b *0x80000000
c
b *0x80200000
c
p/x &bootstacktop
x/3i $pc
si
si
info registers pc sp
b kern_init
c
```

当前构建中，启动路径为 `0x1000 → 0x80000000 → 0x80200000 → kern_init`。栈初始化后 `sp` 应等于 `&bootstacktop`；具体符号地址可能随构建变化。

### 启动流程跟踪

```bash
bash tools/trace_boot.sh
```

脚本使用 QEMU 和 GDB 跟踪复位地址、OpenSBI 入口、内核入口、栈设置和 C 入口。运行前结束占用 1234 端口的其他调试实例。

本实验起始工程没有 `tools/grade.sh`，因此不能用 `make grade` 作为通过依据。编译产物 `bin/`、`obj/` 不提交，按以上命令重新生成。仓库应包含代码、报告及报告引用的图片。

## 本地改动说明

Lab 1 内核核心逻辑沿用课程提供的起始代码。本次工作主要是理解和验证：

1. 将 QEMU 的内核启动参数调整为 `-kernel bin/ucore.img`，适配本机固件交接行为。
2. 为 GDB 增加 `gdb-multiarch` 回退。
3. 增加调试和真实 Ubuntu 终端截图脚本。
4. 按模板整理实验报告。

指导书：[课程实验文档](http://8.135.34.58/lab2026/_book/)。目标仓库：[nanyan831/os2026](https://github.com/nanyan831/os2026)。

# 南开大学操作系统实验2026——Lab2

实验分支：[`lab2`](https://github.com/nanyan831/os2026/tree/lab2)。本分支基于小组导航`main`建立，独立完成实验二，Lab1成果在原`lab1`分支。

- [实验报告](report/report.md)
- [各阶段四段式提示词](report/prompt.md)
- [真实迭代与故障证据](report/iterations.md)
- [最终测试矩阵](report/logs/verification-all.json)
- [QEMU7.0独立矩阵](report/logs/qemu7/verification-all.json)
- [QEMU源码调试结果](report/logs/verification-qemu.json)
- [真实终端截图](report/images/)

已完成物理内存初始化、First-Fit分析与额外尾插回归修复、Best-Fit、Buddy、简化SLUB Challenge和QEMU页表/TLB源码调试。官方评分20/20，Best-Fit内部检查6/6；QEMU6.2和7.0各11项矩阵通过，QEMU4.1.1另有3项源码观察。固定课程测试保持原样。

## 构建与运行

需要WSL/Linux中的RISC-V GNU工具链、make、QEMU、Python3及bc。实际验证使用GCC10.2.0，默认运行环境分别为QEMU6.2/OpenSBI0.9、QEMU7.0/OpenSBI1.0。

```bash
git clone --branch lab2 --single-branch https://github.com/nanyan831/os2026.git
cd os2026/code
make clean && make
make qemu
# 退出QEMU：Ctrl+A，松开后按X。
make clean && make test
make grade
```

普通运行、调试、测试、评分均用`-kernel`。默认Best-Fit。配置或编译宏改变后先clean：

```bash
make clean && make PMM=first_fit
make qemu PMM=first_fit
make clean && make PMM=buddy
make qemu PMM=buddy
make clean && make SLUB_TEST=1
make qemu SLUB_TEST=1
make clean && make PMM=buddy SLUB_TEST=1
make qemu PMM=buddy SLUB_TEST=1
```

完整验证脚本可显式指定QEMU，避免PATH差异：

```bash
python3 tools/verify_lab2.py --stage all --compact --qemu /usr/bin/qemu-system-riscv64
python3 tools/verify_lab2.py --stage all --compact --qemu /opt/qemu/bin/qemu-system-riscv64 --log-label qemu7
make clean && make  # 恢复默认Best-Fit镜像
```

两条路径是本机路径，其他机器自行替换。`make grade`会在临时副本注入测试；源工程不要手工加入`kern/test`或`test_lab2.h`。20/20只覆盖初始化与Best-Fit，扩展任务由独立测试留证。

## 调试与截图

```bash
make debug  # 终端1
make gdb    # 终端2，支持gdb-multiarch回退
python3 tools/trace_paging.py
bash tools/build_qemu_debug.sh  # 独立QEMU4.1.1；需要glib/pixman开发依赖等
python3 tools/trace_qemu_mmu.py --compact
python3 tools/capture_evidence.py
```

独立QEMU不执行`make install`，默认位于`~/oslab2/qemu-4.1.1/build-debug`。源码调试脚本从当前ELF读取探针地址。截图来自真实X终端；需要Xvfb、xterm、xwininfo和ImageMagick。

`code/`包含课程原始布局与实现/验证工具；`report/`包含模板报告、十份阶段提示词、截图和精简日志。`bin/`、`obj/`、Git元数据和QEMU源码/构建目录不提交。

小组成员：刘宸旭2414139、王子卓2411070、缪臻2413807。最终分工：刘宸旭负责Buddy、SLUB、集成验收与报告统稿；王子卓负责物理内存初始化、First-Fit及对应报告；缪臻负责Best-Fit、分页与QEMU调试、提示词和证据整理。三位成员工具均为ChatGPT桌面端，具体模型型号待确认，详见[实验报告](report/report.md)。

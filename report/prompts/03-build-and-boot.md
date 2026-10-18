# 阶段 3：构建、固件交接与分页过渡

```text
[PROMPT]
任务：让当前 ucore 在实际 WSL 的 RISC-V GCC/QEMU/GDB 组合中完整构建，调试并验证进入高地址内核。
操作要求：真实运行构建和调试，先记录故障再做必要修复；不改写或绕过课程算法测试和评分断言。
输出要求：构建日志、固件交接信息、GDB 中的启动过渡证据，以及普通/测试/评分三种路径的一致配置。

[RELY]
原 Makefile 的 qemu/debug/test 与 tools/grade.sh 均使用 -device loader；本机 QEMU=7.0.0。
本机有 gdb-multiarch，专用 riscv64-unknown-elf-gdb 未发现；gdb 配方原来硬编码专用 GDB。
entry.S 写 satp 后继续在低地址执行，再跳到高地址 kern_init；原页表只在索引511提供高地址1GiB映射。
内核物理入口0x80200000，高地址链接0xffffffffc0200000；OpenSBI 经 a0/a1 传递 hartid/DTB。

[GUARANTEE]
一致的 QEMU 启动参数、可用 GDB 配方和确实可执行的分页启动过程。
新增可复现脚本，所有输出来自真实命令；评分注入只在临时副本进行。

[SPECIFICATION]
Pre-Condition：page_init 与 Best-Fit 已补齐，固定测试原样。
Post-Condition：普通运行实际进入内核并完成检查；GDB 能观察到物理入口、satp 和高地址入口；make grade 运行同一启动路径。
Case 1：固件未正确交接。核对 next address/mode，验证 -kernel 适配并同步全部入口。
Case 2：分页切换后取指失败。读取 pc/satp/scause/sepc/stval，分析当前页表，再通过必要的恒等映射或正确跳转修复；保留原高地址映射。
Case 3：编译/汇编兼容问题。记录原始诊断，仅修改相应编译选项或实际使用的接口。
Requirements：不把 OpenSBI 横幅当作内核成功；永久循环的 QEMU 使用有界退出；不覆盖系统 QEMU，不改变全局调试权限。
```

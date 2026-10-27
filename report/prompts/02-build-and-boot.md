# 阶段 2：构建、固件交接与分页过渡

**责任成员：** 缪臻（2413807）。**工作量：** 3 点。

本阶段按现有实现和真实验证材料整理，2026-10-27。成员责任见[等量分工](../division.md)，失败、修订与验证见[迭代记录](../iterations.md)。

```text
[PROMPT]
任务：在实际 WSL RISC-V 工具链和 QEMU 环境中构建 Lab3，并验证 OpenSBI 能交接到高地址内核。
操作要求：先记录原始启动结果，再做必要的最小兼容修复；保持课程评分脚本和测试源码字节不变。
输出要求：构建日志、原始失败、可复现启动命令、透明的 QEMU payload 适配器及 GDB 入口。

[RELY]
宿主默认 PowerShell 7；构建在 Ubuntu-22.04 中执行，已有 GCC 10.2、QEMU 6.2/7.0、gdb-multiarch。
原 Makefile 的 qemu/debug/test 和 grade.sh 使用 -device loader,file=bin/ucore.img,addr=0x80200000。
内核物理入口0x80200000，高地址链接0xffffffffc0200000；OpenSBI 通过 a0/a1 传递 hartid/DTB。
entry.S 写 satp 后仍在低地址执行，稍后才 jr 到高地址；原根表只有索引511提供高地址1GiB映射。
本机原始 loader 方式只显示固件；QEMU 7.0 还暴露了缺少低地址过渡映射的问题。

[GUARANTEE]
可复现的 make/qemu/debug/test 启动配置及可用 GDB 命令。
code/tools/qemu_compat/qemu-system-riscv64：只转换课程固定 raw loader 参数，然后执行真实 QEMU。
kern_entry、boot_page_table_sv39 和 boot_hartid/boot_dtb 的正确启动衔接。

[SPECIFICATION]
Pre-Condition：必要的内存管理和 Trap 接口已经补齐，镜像能生成；固定测试保持原样。
Case 1：只停在 OpenSBI。核对 next address、next mode、DTB 参数及 payload 加载方式；验证 -kernel 对同一镜像的交接效果。
Case 2：开启分页到进入高地址之间取指失效。检查 PC 与根表映射，增加索引2对[0x80000000,0xc0000000)的恒等映射，保留索引511高地址别名。
Case 3：专用 RISC-V GDB 不存在。回退到实际安装的 gdb-multiarch，不修改全局系统配置。
Case 4：官方 grade.sh 内写死 QEMU 名称。通过局部 PATH 和 LAB3_QEMU_REAL 选择透明适配器与真实可执行文件。
Post-Condition：内核实际打印 DTB 和内存自检信息；普通、调试及评分路径使用一致的 payload 语义。
Requirements：不把固件横幅当作内核成功；适配器不生成 PASS 文本或改变退出状态；原始失败日志保留；内核地址布局与接口不随意改写。
```

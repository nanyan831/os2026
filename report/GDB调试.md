# 双 GDB 调试记录与 TCG

本次使用 QEMU 7.0.0 的带符号可执行文件 `/opt/qemu/bin/qemu-system-riscv64`，宿主 GDB 调试 QEMU 的 x86-64 执行流，gdb-multiarch 通过 1235 端口调试 RISC-V 内核。可运行 `python3 tools/trace_gdb.py` 重现，两个 GDB 的命令文件和原始记录均保存在 logs 目录。

## 观测结果

宿主 GDB 在 `riscv_cpu_do_interrupt` 停下，条件为异常编号 8 且模拟 CPU 的 priv 为 0。记录得到 `guest_pc=0x8000aa`。客户机 GDB 同时在 exception_handler 中记录到 cause=8、epc=0x8000aa、系统调用号 30，即 SYS_putc，反汇编确认该地址为 ecall。

系统调用处理完成后，客户机在 __trapret 的 sret 指令处记录 sepc=0x8000ae、sstatus=0x200040020、sscratch=0xffffffffc04c5fe0。SPP 为零，SPIE 为一，sscratch 保存再次进入内核时使用的栈位置。宿主在 helper_sret 记录到 priv=1、sepc=0x8000ae，调用栈经过 code_gen_buffer、cpu_tb_exec 和 cpu_loop_exec_tb。

随后在 sepc 指向的用户地址设置临时断点并继续，得到 `pc=0x8000ae priv=0`。它与 ecall 地址相差四字节，说明异常返回跳过了 ecall，并恢复为 U 态。完整证据见 [客户机记录](logs/gdb-guest.log) 和 [宿主记录](logs/gdb-host.log)。地址对应本次构建，重新修改源码后可能变化。

## QEMU 源码路径

`trans_ecall` 调用 generate_exception，最初生成 U-level ecall 编号，运行期由 riscv_cpu_do_interrupt 结合当前特权级修正。异常处理根据委托寄存器判断目标特权级，保存 sepc、scause 和状态位，更新模拟 CPU 的 PC 和 priv，进入 stvec。源码保存在 [trans_privileged.c.inc](qemu-source/trans_privileged.c.inc) 和 [cpu_helper.c](qemu-source/cpu_helper.c)。

`trans_sret` 生成 helper_sret 调用，然后退出当前 Translation Block。helper_sret 检查当前特权级与返回地址，读取 SPP 决定恢复的特权级，按 SPIE 恢复 SIE，更新状态并返回 sepc。相应源码保存在 [op_helper.c](qemu-source/op_helper.c)。这些文件取自 QEMU 官方 v7.0.0 标签。[官方源码](https://github.com/qemu/qemu/tree/v7.0.0/target/riscv)

TCG 将客户机指令翻译为宿主可执行代码并缓存 Translation Block。宿主看到的是翻译代码和 C helper 的执行，客户机 GDB 看到的是模拟 RISC-V 的寄存器、内存和指令。helper_sret 调用栈中的 code_gen_buffer 展示了翻译代码进入 helper 的路径。此前双 GDB 对地址翻译的观察同样涉及客户机访存指令经过 TCG 和软件 MMU 处理。

## 调试中的问题

第一次只按 exception_index=8 设置宿主断点，捕获到了内核发起的 SBI ecall，记录中 priv=1。根据 trans_ecall 的注释发现编号会在运行期修正，因此增加 priv=0 条件以定位用户态请求。

客户机在 sret 上直接单步时曾等待远程停止回复。改为在 sepc 指向的用户地址设置临时断点并继续，成功验证了返回位置和 priv=0。宿主 helper 断点仍保留，因此同时观察到 QEMU 内部处理和返回后的客户机状态。

宿主 GDB 由测试脚本启动 QEMU 子进程，避免附加现有进程时受 ptrace_scope 限制。采集完成后脚本结束该调试进程组，宿主日志末尾的 KeyboardInterrupt 来自采集器结束 GDB。

# Lab5 用户程序实验报告

姓名与学号为缪臻（2413807），实验日期为 2026 年 11 月 19 日。

## 实验目标与整体逻辑

本实验从 Lab4 的内核线程出发，为线程建立独立的用户地址空间，并通过异常返回进入 U 态。实现顺序依次为系统调用入口、用户映像加载、fork 地址空间复制、退出和等待回收，最后将普通复制扩展为写时复制。实现依据为课程 Lab5 指导书，原始材料来自本地 lab5.zip。[课程手册](http://oslab.mobisys.top/lab2026/_book/lab5/lab5.html)

执行链为 idle 创建 init，init 创建 user_main 内核线程，user_main 选择嵌入内核的用户 ELF，kernel_execve 调用 do_execve 和 load_icode 建立用户态 trapframe，forkrets 转向 __trapret，通过 sret 进入用户入口。用户库以 a0 传系统调用号、a1 至 a5 传参数，ecall 进入内核，syscall 分发后以 a0 返回结果。退出后父进程 wait，init 收养孤儿并回收全部用户进程。

## 四个模块的 spec 与 prompt

以下文字整理了本次实现所采用的约束，可作为复现实验的提示词。代码由本次 AI 协作产生，人工验收仍应结合运行日志和源码阅读。

完整九阶段规格见[提示词汇总](prompt.md)，成员责任、工作量安排与交叉复核见[小组分工](division.md)。

| 模块 | Spec | Prompt |
|---|---|---|
| 系统调用 | 用户 ecall 的 epc 增加 4，按本项目寄存器 ABI 分发，非法编号返回 -E_INVAL，所有用户指针检查权限与范围 | 阅读用户 syscall 封装和 trapframe，实现 exception_handler、syscall 及九个 sys_*，确保非法编号和指针不导致内核失控 |
| 首次用户程序 | 校验 ELF 段边界，装入文件数据，BSS 清零，建立用户栈，SPP=0、SPIE=1，完成 U/S 双向栈恢复 | 补全 alloc_proc、kernel_thread、load_icode、exec、init 与 wait，让 hello 在 U 态打印并退出，init 能验证内存完整回收 |
| fork | 创建独立 PCB、内核栈和 mm，复制 VMA，父返回 PID、子返回 0，CLONE_VM 共享 mm，失败释放已分配资源 | 实现 copy_mm、copy_thread、do_fork、dup_mmap、copy_range，先保证普通复制语义，再加入默认 COW 和 FORK_EAGER 对照路径 |
| 进程控制 | idle/init 保持运行，exit 释放 mm 并成为僵尸，wait 回收 PCB/栈，kill 设置退出标记并唤醒可中断等待 | 完成 do_exit、do_wait、do_yield、do_kill，维护父子和兄弟指针，支持被 kill 的父进程及孤儿收养 |

Challenge 的提示约束进一步要求区分真正只读和暂时只读页，刷新父页表 TLB，处理 ref=1 和 OOM，覆盖内核 copyout，提供状态表、故障注入及 Dirty COW 类问题模拟。详细设计见 [Challenge.md](Challenge.md)。

## 核心函数理解

`RESTORE_ALL` 恢复 sstatus 和 sepc，根据 SPP 判断目标特权级。返回 U 态前将内核栈顶写入 sscratch，最后恢复用户 sp。异常入口交换 sp 与 sscratch，用户异常转到内核栈，内核嵌套异常继续使用原内核栈。switch_to 保存 ra、sp 和 s0 至 s11，proc_run 切换 satp 并刷新 TLB。

`load_icode` 校验 ELF 文件头和程序头范围，检查 filesz 不超过 memsz，为段建立 VMA 和实际映射。每页先清零，再复制文件内容，因此 BSS 的剩余部分为零。用户栈预映射顶端四页，其余合法栈页按需清零分配，栈底一页作为保护区。do_execve 在销毁旧 mm 前保存用户提供的名字和 ELF 数据，防止旧映射释放后继续访问。

`do_fork` 将创建过程组织为 PCB、内核栈、mm、trapframe 和进程树发布几个阶段，失败时沿已完成步骤逆序释放。copy_thread 把父 trapframe 放到子内核栈，将子 a0 置零。dup_mmap 复制区域元数据，copy_range 实现物理页映射的复制或 COW 共享。CLONE_VM 使用 mm_count 管理共享地址空间的生命周期。

`do_exit` 释放用户地址空间，记录退出码并进入僵尸态，唤醒等待的父进程，把孩子移交 init，然后调度。当前内核栈由父进程 do_wait 回收，避免释放正在执行的栈。do_wait 在关中断区间扫描孩子，无僵尸时睡眠并调度，醒来后检查 PF_EXITING，退出码使用 COW 感知的 copy_to_user 写回。

## 实际调试问题

1. 原用户 syscall 封装使用 sd 向 int 返回值写入 8 字节，同时固定读取五个可能未提供的可变参数。改为固定五参数、寄存器约束和 uintptr_t 返回寄存器，最后转换为 int。
2. 原启动参数只通过 loader 装入物理地址，当前 OpenSBI 显示 Next Address 为零。改为 `-kernel bin/ucore.img`，QEMU 将物理入口交给固件。评分脚本读取 Makefile 的同一参数。
3. Sv39 非叶页表项中的 U 位导致版本兼容问题，改为只设 V，页表自检相应验证非叶 U 清零，叶 PTE 的 U 检查保留。
4. 第一次完整评分为 123/130，forktest 完成后 init 的空闲页检查失败。原 SLOB 释放对象后长期保留空闲页，增加 kmalloc_trim，把完整空闲页归还 PMM，在创建测试进程前和回收后执行。修复后 QEMU 6.2 评分为 130/130。
5. exit_mmap 的第二次遍历从链表头结束状态继续，无法释放页表。重置遍历位置，exit_range 检查整个中间页表后再释放，避免多 VMA 提前回收仍在使用的页表。
6. Challenge 的只读写入测试函数被编译器优化删除，加入 used/noinline 保留真实代码符号。短超时曾在故障打印中途结束，最终使用等待完整回收标记的测试驱动。
7. QEMU 7.0 在写 satp 后立即按新页表取指，原入口只有高地址映射，低地址执行流无法继续。增加根索引 2 的临时恒等映射，进入高地址后在 pmm_init 清除该项并刷新 TLB。
8. 在 hello 与 COW_SELFTEST 间切换时，Makefile 原来仅刷新 proc.c，导致 cow_test.o 沿用旧编译开关并出现 check_cow 未定义。将 cow_test.c 与 vmm.c 加入构建切换时的刷新列表，保证 Challenge 自检和 FORK_EAGER 开关能在增量构建中生效。

## 验证与证据

完整课程评分包含 13 个用户程序，总分 130 分。QEMU 6.2 和 7.0 均已通过 130/130，完整输出位于 `logs/grade-qemu6.log` 和 `logs/grade-qemu7.log`。两版的 COW、forktree 和 waitkill 补充测试全部通过，结果位于 checks-qemu6.log、checks-qemu7.log，对应串口日志保存在同一目录。双 GDB 采集脚本也已运行通过。

课程 test_lab5.c、test_lab5.h 与 inject.py 保持原文件内容，基础 PMM 检查保留。grade.sh 仅修改 QEMU 参数来源，测试期望和分数保留。testbss 编译时的越界写警告对应课程故意触发非法访问的用例。

## 用户映像预加载与 OS 原理

构建时用户程序先链接为 ELF，随后被作为二进制段链接进内核。QEMU 启动时把整个内核镜像装入物理内存，因此用户 ELF 字节已经在 RAM 中。到 do_execve 时，load_icode 才把 ELF 内容装到用户页并建立用户映射。常用操作系统通常从文件系统取得可执行文件，通过文件映射和按需调页装入实际使用的部分。本实验通过嵌入 ELF 避免引入尚未完成的文件系统和磁盘加载链。

特权级与系统调用对应受控进入内核，trapframe 保存完整异常现场，context 保存调度切换所需的被调用者保存寄存器。虚拟内存以 VMA 描述合法范围和权限，以页表描述实际映射，二者共同参与访问检查。COW 把 fork 的物理复制推迟到首次写入，以异常处理维护进程隔离。僵尸态让运行资源释放与父进程读取退出状态分开完成。

课程原理中的多核同步、复杂调度策略、换页与磁盘后备、文件页缓存、动态链接、完整信号语义和权限凭据仍需后续系统机制支持。当前 Lab5 使用单 hart、简单轮转调度、静态嵌入 ELF 和有限系统调用集合。

双 GDB 调试及 TCG 分析见 [GDB调试.md](GDB调试.md)。

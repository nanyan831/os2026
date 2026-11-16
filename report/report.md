# 操作系统实验四 · 进程管理

课程：南开大学操作系统  
姓名与学号：缪臻（2413807）  
实验环境：Windows 11、WSL Ubuntu 22.04、RISC-V GCC、QEMU 6.2.0 / 7.0.0  
验证日期：2026-11-16

实验任务和报告内容依据课程站的[练习](http://oslab.mobisys.top/lab2026/_book/lab4/lab4_2_1_exercises.html)与[报告要求](http://oslab.mobisys.top/lab2026/_book/lab4/lab4_5_labs_requirement.html)。

配套材料包括[小组分工与工作量安排](division.md)和[各阶段四段式提示词](prompt.md)。

## 启动和执行顺序

`kern_init` 先读 DTB，再通过 `pmm_init` 建立物理页分配器并检查 Sv39 页映射。设置 Trap 入口后，`vmm_init` 运行 VMA 的有序插入、查找和销毁自检。`proc_init` 把已有的启动执行流登记为 PID 0 的 `idleproc`，再创建 PID 1 的 `initproc`。启用时钟与中断后，`cpu_idle` 看到 `need_resched`，经 `schedule → proc_run → switch_to` 将 CPU 交给 `initproc`。

Lab2 的物理页描述符初始化和 Best-Fit 分配器提供连续物理页，供页表和线程内核栈使用。Lab3 的 Trap 入口、寄存器恢复和中断分发提供 `__trapret`，也用于新线程首次进入内核。Lab4 的 `mm_struct` 和 `vma_struct` 描述地址空间及区间。本实验创建的内核线程共用启动页表，PCB 的 `mm` 为 `NULL`。

## 练习一 · 虚拟内存页操作

### Spec

| 函数 | 行为及验收条件 |
| --- | --- |
| `page_insert` | 返回 0 后，指定虚拟页指向目标物理页并具有所请求的权限。同页重插只改权限，异页替换时更新新旧页引用计数。缺内存时返回 `-E_NO_MEM`，失败前后的映射、目标页引用计数和空闲页数一致。 |
| `page_remove` | 已映射时清除有效项、失效对应地址的 TLB 翻译、递减物理页引用。最后一个引用消失时回收物理页。未映射时直接返回，允许重复调用。 |

`get_pte` 用 `PDX1`、`PDX0`、`PTX` 定位 4 KiB 叶项。新建两级页表时，先在未挂接的目录页中准备下级表，成功后才写根项。第二次分配失败时释放新目录页。启动的大页直映在根级终止，4 KiB 映射接口遇到高层叶项就停止遍历。撤销或替换叶项后执行 `sfence.vma`，再把引用降到零的物理页归还分配器。空页表可以继续供后续映射复用。

### Prompt

> 在 Lab4 的 `pmm.c` 中实现 `page_insert` 和 `page_remove`，使用现有 Sv39 页表和引用计数接口。分别处理未映射、同页重插和异页替换，重复撤销应安全。检查第二级页表分配失败时有没有漏掉第一张页表，更新叶项后再失效 TLB。复用 Lab2 的物理页初始化与 Best-Fit 实现，保留原有 Best-Fit 测试。

## 练习二 · 进程创建与初始化

### Spec

| 函数 | 行为及验收条件 |
| --- | --- |
| `alloc_proc` | 分配并初始化 PCB，设为 `PROC_UNINIT`、PID 为 -1、页表为启动根页表，初始化链表节点。 |
| `proc_init` | 初始化进程链表和 PID 哈希桶。PID 0 复用 `bootstack` 并设为 `current`，PID 1 由 `kernel_thread(init_main, ...)` 创建，两者都处于 `PROC_RUNNABLE`。 |
| `kernel_thread` / `copy_thread` | 前者构造临时 Trap 帧，通过 `s0/s1` 传递入口和参数。后者把帧复制到独立内核栈，设置 `a0=0`、返回栈指针和首次运行上下文。 |
| `do_fork` / `copy_mm` | 依次分配 PCB、内核栈和准备现场，在关中断区分配 PID、登记哈希表与链表、发布可运行状态。分配失败按已占用资源逆序回收。`copy_mm` 确认创建路径使用共享内核空间。 |

`idleproc` 已经处于启动调用链上，它的第一个有效 `context` 由首次切出时的 `switch_to` 写入。`initproc` 的 `context.ra` 指向 `forkret`。`forkret → forkrets → __trapret` 装载 Trap 帧，`sret` 根据 `SPP=1` 在 S 态进入 `kernel_thread_entry`，并按 `SPIE=1` 恢复中断使能。入口汇编把 `s1` 移入 `a0` 作为实参，再调用 `s0` 中保存的函数。初始帧本身留在新线程的栈上，父线程的临时变量退出作用域后仍可调度子线程。

### Prompt

> 在 `proc.c` 补全六个创建函数。按课程原型保留 `copy_mm`，实现 PID 查找、链表登记和失败回收。先封装启动线程，再创建 init；把独立内核栈、初始 Trap 帧和 forkret 串起来。用官方 PCB 断言和 init 的实际输出验收，并检查向短字符串读取 `PROC_NAME_LEN` 字节的问题。

## 练习三 · 调度与上下文切换

### Spec

| 函数 | 行为及验收条件 |
| --- | --- |
| `cpu_idle` | 在空闲循环检查 `need_resched` 并触发调度。 |
| `schedule` | 清除当前重调度标志，从链表中的当前位置向后环形扫描，仅选择 `PROC_RUNNABLE`，无候选时回到 idle。更新选中线程的 `runs`。 |
| `proc_run` | 关中断更新 `current`，由物理页表基址合成 Sv39 `satp`。页表变化时写入 `satp` 并刷新 TLB，然后切换寄存器和栈。 |
| `switch_to` | 按 `struct context` 的字段顺序保存和恢复 `ra`、`sp`、`s0–s11`。普通线程恢复后从上次切换位置继续，新线程从 `forkret` 开始。 |

当前任务从所在节点之后开始查找，普通任务按加入链表的顺序轮转。官方调度测试在 init 中创建两个额外线程，多次显式调用 `schedule` 并检查二者都得到执行。这里的切换点是主动调度，时钟处理继续沿用 Lab3 的周期通知。

### Prompt

> 在 `sched.c`、`proc.c`、`switch.S` 实现单核协作调度。跳过链表哨兵和非就绪 PCB，处理只有当前线程和只能回到 idle 的情形。按照 RV64 ABI 保存被调用者保存寄存器、栈和返回地址。保护选线程及切换过程，使用 Sv39 mode 和物理 PPN 构造 satp。用课程注入的两个反复让出 CPU 的线程验收。

以上提示词是根据本次实现与验证时采用的约束整理的复现版本。Lab2、Lab3 的迁移分别沿用原有页描述符、Best-Fit 和 Trap 上下文恢复设计。

## 调试中处理的问题

| 现象 | 定位与处理 |
| --- | --- |
| 原始 QEMU 命令启动后只见 OpenSBI，`Domain0 Next Address` 为 0 | 原命令只把镜像装入物理内存。本机使用 `-kernel bin/ucore.img` 让固件取得入口。启用 `satp` 后到跳入高地址之前仍在低地址取指，移植 Lab3 的恒等过渡映射并完整装载栈顶地址。 |
| QEMU 6.2 能启动，7.0 在页表自检访问 `0x100` 时出现 `store_page_fault` | Sv39 非叶项的 U 位是保留位，原框架给两级非叶项设置了 U。改用 `ptd_create`，U 位由需要用户访问的叶项携带。调整启动自检中对非叶项 U 位的错误断言，课程注入测试源码保持原样。 |
| 原评分流程仅注入测试，然后调用脚本运行 QEMU | 在临时副本中显式编译注入后的内核，再运行原有评分断言。让评分与普通启动共用 QEMU 参数，并支持用 `gdb-multiarch` 连接调试。 |
| 映射第二张下级表时耗尽空闲页 | 保留一张空闲页的故障注入覆盖了两次分配中的第二次失败。验证返回 `-E_NO_MEM`、根项仍为空、目标页引用计数仍为零、空闲页数保持为一。解除故障后继续启动到 init。 |

## 与操作系统原理的对应

| 实验知识点 | 对应原理 |
| --- | --- |
| `Page.ref` 与共享映射 | 同一物理页可经不同虚拟地址访问。只有最后一个映射被移除时，它才可由分配器重新交付。 |
| 三级页表与 TLB | 页表记录地址翻译及权限，TLB 缓存部分结果。修改 PTE 后需要使相应翻译失效，内核访问同一物理页的不同别名应观察到相同内容。 |
| VMA、PCB 和内核栈 | VMA 记录逻辑范围和属性，页表记录具体翻译。PCB 保存管理状态，每个线程的内核栈保存各自的调用链。 |
| Trap 帧与调度上下文 | 异步 Trap 需要保存程序可见的寄存器和返回 CSR。显式调用的 `switch_to` 遵循函数调用 ABI，保存足以恢复调用链的寄存器。 |
| 调度临界区 | 单 hart 上临时屏蔽 S 态中断，使选择、更新 `current` 和切换过程在本核保持一致。 |

后续原理内容包括用户地址空间隔离、系统调用、按需调页、页面置换、时间片抢占、线程退出与父进程回收，以及多核锁和跨 hart 的 TLB shootdown。课程给定的 `do_exit` 仍是占位实现，普通启动中 init 打印告别后会进入内核监视器。

## Challenge 思考题

1. `local_intr_save(flag)` 读取 `sstatus.SIE`，把原状态存入 `flag`。若原本开启，则通过 `clear_csr` 清除 SIE。`local_intr_restore(flag)` 仅在保存值为真时重新开启。因此外层已经关中断时，内层保存值为假，退出内层也不会提前开启中断。
2. Sv32、Sv39、Sv48 使用同一种逐级索引原则。它们的 VPN 分别是两个 10 位索引、三个 9 位索引、四个 9 位索引，页内偏移都是 12 位。Sv32 每个 PTE 4 字节，Sv39 和 Sv48 每个 PTE 8 字节，所以三个分页模式的单级页表都可放在一张 4 KiB 页内。Sv39 的两个中间步骤同样需要判断有效性、识别非叶项、提取 PPN，再取下一级表地址。
3. 当前 `get_pte(create)` 让查询和创建共用遍历，避免复制索引逻辑。查询路径使用 `create=false`，不会分配资源。若后续增加多种分页深度、锁或大页拆分，可以保留共享 walker，再提供语义独立的查询和创建入口，由创建入口负责分配失败后的回滚。

## 复现与结果

在 WSL Ubuntu 中进入分支根目录下的 `code/`。课程入口使用 GNU Make，默认寻找 `riscv64-unknown-elf-` 工具链。

```bash
make -j4
make qemu QEMU=/usr/bin/qemu-system-riscv64
make grade QEMU=/usr/bin/qemu-system-riscv64
make grade QEMU=/opt/qemu/bin/qemu-system-riscv64
```

普通启动看到 init 输出并进入监视器后，按 Ctrl+A 再按 X 退出 QEMU。

| 验证 | QEMU 6.2.0 | QEMU 7.0.0 |
| --- | --- | --- |
| 课程评分 | [30/30](logs/grade-qemu6.log) | [30/30](logs/grade-qemu7.log) |
| PMM / Process Create / Schedule | 三项均通过 | 三项均通过 |
| 页表分配失败回滚（临时测试副本） | [通过](logs/rollback-qemu6.log) | [通过](logs/rollback-qemu7.log) |

[QEMU 6.2 普通启动](logs/boot-qemu6.log)和 [QEMU 7.0 普通启动](logs/boot-qemu7.log)均包含内存、页表、VMA 的自检结果和 init 的实际输出。评分前在临时目录注入课程测试。课程测试将返回 `void` 的测试函数传给要求 `int (*)(void *)` 的接口，因此会产生两处编译警告。该测试线程持续主动调度，课程用例与评分条件保留原样。

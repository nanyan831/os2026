# Lab5 各阶段提示词汇总

沿用 Lab2、Lab3、Lab4 的四段式，将本次实现、调试和验证约束整理为可复现的阶段提示词。以下内容为按最终代码和实际问题整理的规格文本。阶段责任见[小组分工](division.md)，运行结果见[实验报告](report.md)。

| 阶段 | 主题 | 责任成员与工作量 |
| --- | --- | --- |
| [1](#阶段-1) | 前序内存管理迁移、VMA 边界与页表回收 | 王子卓，3 点 |
| [2](#阶段-2) | U/S Trap、系统调用分发与用户 ABI | 刘宸旭，3 点 |
| [3](#阶段-3) | PCB、首个用户进程与 ELF/exec | 刘宸旭，4 点 |
| [4](#阶段-4) | fork、mm 复制和 COW 核心 | 王子卓，4 点 |
| [5](#阶段-5) | 退出等待、孤儿收养与 COW 内核写回 | 缪臻，4 点 |
| [6](#阶段-6) | 启动兼容、双版本评分与双 GDB | 缪臻，3 点 |
| [7](#阶段-7) | COW 内核边界与 Dirty COW 类问题验证 | 王子卓，2 点 |
| [8](#阶段-8) | COW 用户隔离、进程树与状态设计 | 缪臻，2 点 |
| [9](#阶段-9) | 报告统稿、交叉复核与提交包 | 刘宸旭，2 点 |

## 阶段 1

**前序内存管理迁移、VMA 边界与页表回收**

```text
[PROMPT]
以本地 lab5.zip 解压的 RISC-V Lab5 骨架为基础，补齐 Lab2 的 page_init 和 Best-Fit，以及 Lab4 的 page_insert/page_remove。
对照本版本 VMA 和三级页表，检查 mm_map、exit_mmap 和 exit_range 的边界与回收顺序。

[RELY]
DTB getter 提供 RAM 起点和大小，end 是内核虚拟末端。
Page 数组长度为 npage-nbase，描述符大小以本版本 sizeof(struct Page) 为准。
PGSIZE=4096，当前用户区间为 USERBASE 到 USERTOP，内核使用高地址直映。
使用 alloc_page/free_page、get_pte、page_ref_inc/dec、PADDR/KADDR 和 local_intr_save/restore。

[GUARANTEE]
保持公开 PMM/VMM 接口和课程 default_pmm_manager 名称。
保留 basic_check、default_check 及课程测试的原有检查条件。
用户数据页先解除映射，再回收空页表，最后由进程层释放根页表。

[SPECIFICATION]
初始化时裁剪可管理 RAM，检查整数溢出、描述符数组容量和对齐边界，只登记完整空闲页。
Best-Fit 选择最小满足块，同大小保留低地址候选，正确分裂和合并相邻空闲块。
page_insert 处理新映射、同页重插和异页替换，page_remove 支持重复删除，更新引用数并失效 TLB。
Sv39 非叶项只设 V，叶项才具有用户访问和读写执行权限，更新相应非叶权限自检。
mm_map 拒绝零长度、地址溢出、超出用户区间和任何区间重叠。
exit_mmap 第二轮从链表头重新遍历，exit_range 在确认整张下级表无有效项后才释放。
验收 PMM、页表和 VMA 自检，后续多 VMA 回收应恢复空闲页数。
```

## 阶段 2

**U/S Trap、系统调用分发与用户 ABI**

```text
[PROMPT]
扩展 Lab3 的 trapentry.S，使异常入口和恢复支持用户态与内核态往返。
实现模块一的 exception_handler、syscall 和九个 sys_*，核对用户库的寄存器传参及返回值宽度。

[RELY]
RV64 trapframe 有 32 个通用寄存器槽和 4 个 CSR 槽，共 288 字节。
用户态运行时 sscratch 保存内核栈位置，内核执行期间 sscratch 为零。
a0 保存系统调用号，a1 到 a5 保存参数，返回值写入 a0。
sys_* 调用 do_exit/do_fork/do_wait/do_execve/do_yield/do_kill，字符输出和页表展示使用现有接口。

[GUARANTEE]
保留 SAVE_ALL、RESTORE_ALL、__alltraps、__trapret 和 forkrets 的接口及帧布局。
非法系统调用号或空分发表项返回 -E_INVAL。
用户 ecall 在分发前令 epc 增加 4，用户访问错误结束相应进程，未处理内核异常保留诊断。

[SPECIFICATION]
用户异常交换到内核栈，嵌套内核异常保留当前内核栈，保存完整寄存器和 CSR。
根据目标帧 SPP 决定返回模式，返回 U 态前设置 sscratch，最后恢复 sp 后执行 sret。
用户 syscall 封装使用固定五参数和 uintptr_t 寄存器变量，避免读取未传入的可变参数。
用编译器寄存器约束声明 a0 到 a5 和 memory，避免向 32 位 int 存储 8 字节。
异常分发将用户页故障交给 VMM，成功处理后重试原访存指令。
验收官方 Syscall 检查及 hello 输出，后续用双 GDB 检查 ecall 的 epc 和 sret 返回位置。
```

## 阶段 3

**PCB、首个用户进程与 ELF/exec**

```text
[PROMPT]
实现模块二的首个用户进程启动链，补齐 alloc_proc、kernel_thread、copy_thread、load_icode、do_execve、kernel_execve、user_main、init_main 和 proc_init。
与阶段 5 的 do_wait 对接，使用户程序退出后 init 能完成回收检查。

[RELY]
kernel_thread_entry 从 s0 取函数、从 s1 取参数，forkret 经 forkrets 进入异常恢复路径。
用户 ELF 通过 Makefile 链接进内核，KERNEL_EXECVE/KERNEL_EXECVE2 提供二进制地址和大小。
使用 mm_create、setup_pgdir、mm_map、pgdir_alloc_page、lsatp、flush_tlb 和 ELF 头结构。
USTACKSIZE 描述整个栈 VMA，用户栈底一页用于保护，顶端初始映射四页。

[GUARANTEE]
idle 与 init 的 PID 分别为 0、1，普通进程具有独立 PCB 和内核栈。
用户 ELF 的文件数据正确复制，BSS 清零，入口位于可执行 VMA。
新用户帧的 sp 指向 USTACKTOP，epc 指向 ELF 入口，SPP 清零、SPIE 置位。

[SPECIFICATION]
清零 PCB 并初始化 pid、状态、内核页表和链表，进程名按最大长度安全保存。
校验 ELF magic、机器类型、程序头大小及范围，检查 filesz<=memsz 和各段用户地址边界。
按段创建 VMA，每页先清零再复制文件部分，权限由 ELF flags 转换为合法 PTE。
分配用户栈顶四页，其余合法栈范围由页故障路径按需分配。
do_execve 在释放旧 mm 前保存名字和用户提供的 ELF 数据，失败清理新映像并按接口返回或退出。
kernel_execve 准备 trapframe 后进入 forkrets，使 CPU 实际执行用户入口。
验收官方 Process Create、hello 和 testbss，用 init 的回收检查验证启动与退出形成完整流程。
```

## 阶段 4

**fork、mm 复制和 COW 核心**

```text
[PROMPT]
完成模块三的 do_fork、copy_mm、dup_mmap 和 copy_range，并实现 Challenge 的默认 COW fork。
增加 PTE_COW、cow_resolve 和 do_pgfault，保留 FORK_EAGER 的普通复制对照路径。

[RELY]
CLONE_VM 共享整个 mm，mm_count 管理其生命周期，普通 fork 创建独立 mm 和页表。
PTE_COW 使用 RSW 软件位 8，Page.ref 统计物理页映射引用。
VM_WRITE 表示区域原本可写，PTE_W 表示当前硬件是否允许写。
当前课程运行环境为单 hart，内核修改映射时可使用本地关中断保护和 sfence.vma。

[GUARANTEE]
父进程返回子 PID，子进程从原 Trap 帧返回且 a0=0。
fork 创建独立内核栈、PCB、VMA 和页表，子进程在发布完整状态后变为可运行。
真正只读页保持只读，COW 页必须有效、用户可访问、原 VMA 可写且当前 W 清零。

[SPECIFICATION]
创建失败时逆序释放子进程已获得的栈、mm、映射及 PCB。
fork 复制 VMA，逐页处理已映射页，未映射合法页保留按需分配语义。
COW fork 先准备子 PTE，将父子原可写页清 W、设 COW，共享物理页并增加引用，刷新父翻译。
ref>1 的 COW 写故障分配并复制一页，替换写者映射，降低旧页引用。
ref=1 时直接恢复 W 并清 COW，避免分配。
OOM 时保留原 PTE、数据与引用数，真正只读写入和栈保护页访问返回错误。
FORK_EAGER 直接复制物理内容，并把源 COW 语义还原为子进程可写权限。
验收共享 PPN、PTE 权限、引用数、父子独立写入、嵌套 fork 和最终回收。
```

## 阶段 5

**退出等待、孤儿收养与 COW 内核写回**

```text
[PROMPT]
完成 do_exit、do_wait、do_yield、do_kill 和父子兄弟关系维护。
使 copy_to_user 支持 COW，检查 fork 压力测试后 SLOB 和用户页表是否完整回收。

[RELY]
进程具有 parent/cptr/yptr/optr、wait_state、exit_code 和 PF_EXITING。
WT_CHILD 表示等待孩子，WT_INTERRUPTED 表示可被 kill 唤醒的等待。
释放当前 mm 前切回启动页表，mm_count 降到零后释放映射、页表和 mm。
do_wait 是内核向用户退出码缓冲区写入的正常入口，该目标页可能仍与其他进程共享。

[GUARANTEE]
idle/init 保持运行，普通进程 exit 后成为僵尸并调度离开，其内核栈由等待者回收。
退出时孩子移交 init，僵尸孩子能够唤醒等待的 init。
无匹配孩子返回 -E_BAD_PROC，非法退出码指针返回错误，kill 可中断睡眠中的等待者。

[SPECIFICATION]
在临界区扫描孩子、设置睡眠和唤醒条件，避免退出通知与等待状态更新错开。
wait 成功后解除哈希和进程树关系，回收 PCB 与内核栈。
copy_to_user 先验证整个范围，再逐页处理缺页和 COW，重新查询当前 PTE 后检查 U/W 并写入。
处理跨页缓冲区、地址加法溢出和真正只读区域，避免复用分裂前取得的物理页指针。
do_yield 设置重调度标志，时钟中断请求抢占，在返回用户态前执行调度和退出检查。
若 forktest 的回收断言失败，区分实际映射泄漏与 SLOB 完整空闲页留存，必要时实现 kmalloc_trim 归还 PMM。
验收 exit、spin、badarg、forktest，以及 observer 检查 waitpid 写回隔离的用户用例。
```

## 阶段 6

**启动兼容、双版本评分与双 GDB**

```text
[PROMPT]
使用 PowerShell 7 和现有 WSL Ubuntu 22.04 工具链完成构建与 QEMU 6.2/7.0 验证。
按手册完成宿主 GDB 与客户机 gdb-multiarch 联合调试，记录 ecall 和 sret 的实际处理。

[RELY]
内核高地址链接，物理入口为 0x80200000，OpenSBI 通过 a0/a1 交接 hartid 与 DTB。
QEMU 6.2 位于 /usr/bin，带符号 QEMU 7.0 位于 /opt/qemu/bin。
trace_gdb.py 使用端口 1235，宿主函数为 riscv_cpu_do_interrupt 和 helper_sret。
QEMU 官方 v7.0.0 的 trans_ecall/trans_sret、cpu_helper.c 和 op_helper.c 可用于解释 TCG 路径。

[GUARANTEE]
make、qemu、debug、gdb、grade 可用，保留原课程测试断言、期望输出和分值。
课程 test_lab5.c、test_lab5.h 和 inject.py 与原始文件一致。
日志记录真实运行结果，区分编译警告、运行故障和达到验收条件后的结束动作。

[SPECIFICATION]
使用 -kernel bin/ucore.img 让 OpenSBI 取得入口，评分器从 Makefile 读取相同 QEMUOPTS。
写 satp 后的低地址取指需要根索引 2 的临时恒等映射，进入高地址后清除并刷新 TLB。
补齐 bootstacktop 的低位装载，非叶项按 Sv39 要求设置权限。
切换 TEST、COW_SELFTEST、FORK_EAGER 时刷新 proc.c、cow_test.c、vmm.c，避免沿用旧目标文件。
两版分别运行课程评分和补充检查，保存总分与串口证据。
宿主断点同时要求异常编号 8 和 priv=0，避免捕获内核 SBI ecall。
客户机记录 Trap 帧、ecall、返回前 sepc/sstatus/sscratch，再在 sepc 设置临时断点验证返回 PC 和 priv=0。
结合 code_gen_buffer 与 helper 调用栈说明 TCG 翻译、运行期异常处理和 Translation Block 退出。
```

## 阶段 7

**COW 内核边界与 Dirty COW 类问题验证**

```text
[PROMPT]
编写 COW_SELFTEST 内核测试，验证 COW 状态不变量、分配失败和内核写回。
在临时 mm 与测试页中模拟检查后映射变化、继续使用旧物理页指针的错误，验证修复后的 copyout。

[RELY]
可创建独立测试 mm 和用户 VMA，通过物理直映检查真实页内容。
当前 PMM 暴露 free_area，关中断后临时隐藏空闲链表可确定性制造分配失败。
测试中的旧指针在 fork 前捕获，fork 后父子已共享该页。
Dirty COW 公开说明涉及私有映射 COW 处理的竞争问题，课程系统提供有限的模拟机制。

[GUARANTEE]
测试结束时恢复 PMM 状态并销毁临时映射，故障注入只在自检开关下运行。
保留对原数据、父子 PTE、Page.ref 和空闲页数量的断言。
报告说明本实验模拟的不变量及现有系统机制，依据实际输出解释问题和修复。

[SPECIFICATION]
检查 fork 后共享 PPN、ref=2、W 清零和 COW 置位，真正只读页无 COW。
隐藏空闲页后调用 cow_resolve，断言 -E_NO_MEM 且父子 PTE/ref 不变，再恢复分配器。
通过旧直映指针写临时共享页，观察同伴数据变化并恢复测试初值。
通过 copy_to_user 写同一地址，确认先分裂后写，另一方保持原值，真正只读目标被拒绝。
检查 ref=1 恢复权限时空闲页数不变，跨页 copyout、缺页清零及地址溢出检查正确。
创建多个 VMA 并重复销毁，清理 SLOB 完整空闲页后核对 PMM 页数。
两版 QEMU 均取得 COW kernel selftests PASS，并保存具体断言输出。
```

## 阶段 8

**COW 用户隔离、进程树与状态设计**

```text
[PROMPT]
编写并运行 cowtest 用户程序，补充 forktree 和 waitkill 验证，完成 Challenge 的状态设计与用户映像预加载解释。
使用自动驱动等待真实串口验收标记，记录各程序的最终回收结果。

[RELY]
用户库提供 fork、waitpid、yield、kill、getpid 和 exit，COW 内核路径已集成。
可用对齐的数据页观察独立写入，使用 observer 和 worker 检查父进程内核写回的隔离。
init 输出 init check memory pass. 前会检查进程数、链表和空闲物理页恢复。

[GUARANTEE]
cowtest 覆盖父写、子写、未修改页、嵌套 fork、只读代码写入和 waitpid 写回。
run_checks.py 遇到用户或内核 panic、超时或缺少完成标记时失败。
Challenge 文档将每条状态转换关联到实现和测试。

[SPECIFICATION]
父子修改不同页后分别检查本地变化及同伴原值，三级 fork 检查孙进程写入隔离。
observer 多次 yield 后仍读取原值，父进程 waitpid 把 worker 退出码写到自己的 COW 页。
子进程写只读代码页，父进程等待并检查退出码为 -E_KILLED。
保留被写代码符号，必要时使用 used/noinline 避免优化导致测试无法链接。
运行 forktree 与 waitkill，观察孤儿收养、可中断等待和最终回收。
状态表覆盖私有可写、共享 COW、单引用 COW、真正只读、按需分配、OOM 和最后引用释放。
解释用户 ELF 在构建时嵌入内核、启动时随镜像进入 RAM、exec 时装入用户映射的各个时点。
结合常用 OS 的文件映射与按需调页说明课程模型的加载方式。
```

## 阶段 9

**报告统稿、交叉复核与提交包**

```text
[PROMPT]
依据指导书、最终源码和真实日志，完成 Lab5 Markdown 报告、小组分工和完整四段式提示词。
按前三次实验的成员信息和九阶段安排组织责任，核对交叉链接并更新提交 ZIP。

[RELY]
源码位于 labcodes-lab5，报告为 report.md，分工和提示词分别为 report/division.md 与 report/prompt.md。
Challenge 与双 GDB 文档、原始运行日志已位于 report/。
三名成员为王子卓 2411070、刘宸旭 2414139、缪臻 2413807，每人估算 9 个工作量点。
lab5/package.py 生成差异文件、SHA-256 清单和 labcodes/lab5 目录结构的 ZIP，并逐项比对归档内容。

[GUARANTEE]
四个基础功能、COW Challenge、Dirty COW 类问题说明、状态转换、双 GDB/TCG 和知识点分析都有对应内容。
每人维护自己模块的规格、提示词和报告段落，统稿负责人统一术语与证据链接。
分工表按责任安排表述，自动测试结果与人工复核记录依据各自证据填写。

[SPECIFICATION]
九个阶段均具备 PROMPT、RELY、GUARANTEE、SPECIFICATION，名称、负责人和点数与分工表一致。
保留真实调试问题，包括 ABI 宽度、OpenSBI 入口、过渡映射、SLOB 回收、页表遍历和编译开关。
根据日志填写两版 130/130、COW 与进程树测试和双 GDB 结果，不把对照路径存在写成额外已验证结论。
核对学号标记、原课程测试完整性和全部本地 Markdown 链接。
更新 README 与报告中的文档入口，重新打包并检查 ZIP、文件清单和 SHA-256。
说明本地交付、远程仓库推送和课程平台提交的实际状态。
```

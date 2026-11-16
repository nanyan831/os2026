# Lab4 各阶段提示词汇总

沿用 Lab2、Lab3 的四段式。以下提示词按本次实现、调试和验证中的要求整理，可用于从课程骨架复现实验。阶段负责人来自[小组分工](division.md)，源码路径以 `code/` 为起点，实际结果见[实验报告](report.md)。

| 阶段 | 主题 | 责任成员与工作量 |
| --- | --- | --- |
| [1](#阶段-1) | Lab2 内存管理迁移 | 王子卓，3 点 |
| [2](#阶段-2) | Lab3 Trap 迁移与首次运行接口 | 刘宸旭，3 点 |
| [3](#阶段-3) | 页映射、引用计数与 TLB | 王子卓，4 点 |
| [4](#阶段-4) | PCB、内核栈与线程创建 | 刘宸旭，4 点 |
| [5](#阶段-5) | 协作调度与 RV64 上下文切换 | 缪臻，4 点 |
| [6](#阶段-6) | 构建、OpenSBI 交接与双版本评分 | 缪臻，3 点 |
| [7](#阶段-7) | 页表分配失败专项回归 | 王子卓，2 点 |
| [8](#阶段-8) | Challenge 思考题与原理核对 | 缪臻，2 点 |
| [9](#阶段-9) | 报告统稿、交叉复核与归档 | 刘宸旭，2 点 |

## 阶段 1

**Lab2 内存管理迁移**

```text
[PROMPT]
在 Lab4 骨架的 kern/mm/pmm.c 和 kern/mm/default_pmm.c 补齐 Lab2 标记。
以已有 Lab2/Lab3 的 page_init 和 Best-Fit 为依据，适配 Lab4 的 Page 结构、直映范围和 PMM 接口。

[RELY]
DTB getter 提供 RAM 起始地址和大小。end 是链接时内核虚拟末端。
当前 Lab4 以 DRAM_BASE 为描述符下标的起点，npage 是物理页号上界。
使用本版本 sizeof(struct Page)、KERNBASE、KERNTOP、PHYSICAL_MEMORY_OFFSET 与 PGSIZE。
alloc_pages、free_pages、nr_free_pages 已负责保存和恢复本地中断状态。

[GUARANTEE]
保留 page_init、default_pmm_manager 和课程 Best-Fit 自检的接口。
确认 PMM 初始化后才登记空闲页，自检完成后才进入页表与 SLOB 分配器初始化。

[SPECIFICATION]
合法 RAM：裁剪到直映区可管理范围并以页对齐，先验证描述符数组能完整放入 RAM，再初始化描述符。
异常 RAM：基址错误、大小为零、加法溢出或描述符无法放入时报告错误。
分配：选择最小满足块。同尺寸保留低地址候选，区分精确匹配、分裂和连续页不足。
释放：按物理地址有序插回并合并相邻块，更新空闲计数。
验收：保留 basic_check 和 best_fit_check 原文，普通启动出现 check_alloc_page() succeeded。
```

## 阶段 2

**Lab3 Trap 迁移与首次运行接口**

```text
[PROMPT]
对照 Lab3，在 kern/trap/trapentry.S 和 trap.c 恢复 Trap 保存、分发、恢复与返回路径。
检查 Lab4 新增的 forkrets 能使用同一份 Trap 帧布局启动线程。

[RELY]
Trap 帧包含 32 个通用寄存器槽和 4 个 CSR 槽，RV64 总计 288 字节。
a0 按 ABI 传递 struct trapframe *。恢复 status 和 epc 时可暂借 s0/s1，随后恢复其原值。
forkrets 接收新线程的 Trap 帧指针并设置 sp，再跳到 __trapret。

[GUARANTEE]
__alltraps 调用 trap(tf)，__trapret 恢复寄存器、sstatus、sepc 和原 sp 后执行 sret。
保持课程 struct trapframe、入口与时钟及同步异常分发的接口。

[SPECIFICATION]
进入 Trap：完整保存调用者和被调用者保存寄存器，使用 sscratch 暂存原 sp。
退出 Trap：最后加载 sp，保证在恢复其他寄存器前帧基址一直有效。
首次运行：允许恢复由 kernel_thread 人工构造的帧，sepc 和 SPP/SPIE 取自帧的初始值。
验收：核对汇编偏移和 C 结构布局。用普通启动时 init 实际执行确认首次返回链路。
```

## 阶段 3

**页映射、引用计数与 TLB**

```text
[PROMPT]
完成 kern/mm/pmm.c 的 LAB4 function1，实现 page_insert、page_remove，并核对 get_pte 创建路径。
排查 QEMU 7.0 自检在低地址 0x100 的 store_page_fault。

[RELY]
PGSIZE 为 4096，PDX1/PDX0/PTX 分别索引 Sv39 的三级页表。上两层可能遇到大页叶项。
使用 get_pte、get_page、pte_create、ptd_create、page_ref_inc/dec、free_page 和 tlb_invalidate。
页表本身占用物理页。非叶项的 U 位保留，叶项保存映射权限。

[GUARANTEE]
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
void page_remove(pde_t *pgdir, uintptr_t la)

[SPECIFICATION]
新映射：新页引用数加一，写叶项并失效对应 TLB。
同页重插：更新请求的权限，引用数和空闲物理页数保持原值。
异页替换：增加新页引用，写新叶项并失效旧翻译，再处理旧页最后引用的回收。
删除：清除有效项，降引用并在必要时回收。重复删除无效项直接返回。
失败：根项新增时先准备两层目录，第二个分配失败要释放第一个，返回 -E_NO_MEM。
验收：运行 check_pgdir、check_boot_pgdir 与官方 PMM 用例，核对 QEMU 6.2 和 7.0。
```

## 阶段 4

**PCB、内核栈与线程创建**

```text
[PROMPT]
补齐 kern/process/proc.c 的 LAB4 function2，实现 PCB 初始化、启动线程登记和内核线程创建。
对照课程测试中的 idleproc/initproc 断言和汇编入口检查字段与栈布局。

[RELY]
复用 kmalloc/kfree、setup_kstack/put_kstack、get_pid/hash_proc/find_proc 和 wakeup_proc。
bootstack 属于启动上下文，alloc_pages(KSTACKPAGE) 为新线程提供栈。
kernel_thread_entry 从 s0 取函数地址、从 s1 取参数。共享内核页表的线程使用 boot_pgdir_pa。

[GUARANTEE]
保留 alloc_proc、proc_init、kernel_thread、do_fork、copy_thread、copy_mm 及其原定接口。
成功返回 PID，资源不足返回负错误码。idleproc 和 initproc 分别使用 PID 0 和 PID 1。

[SPECIFICATION]
先清零 PCB，再初始化 pid、状态、链表节点和页表。进程名按最大长度复制并保留结尾零字节。
idleproc 复用启动栈并成为 current，设置首次重调度标记。initproc 按普通内核线程走完整创建路径。
将人工 Trap 帧复制到子线程栈，令 a0 为 0。设置 SPP/SPIE、入口 epc、上下文 ra/sp 和返回栈指针。
在关中断区分配 PID、登记哈希和链表，再设为可运行。失败时按已获得的资源逆序释放。
验收：官方 Process Create 通过，并从串口确认 init 的 PID、名称和参数输出。
```

## 阶段 5

**协作调度与 RV64 上下文切换**

```text
[PROMPT]
补齐 LAB4 function3。在 sched.c 按链表实现轮转选择，在 proc.c 调用 RV64 上下文切换。
对照 struct context 逐项核实 switch.S 的存取偏移。

[RELY]
普通线程位于 proc_list，idleproc 单独保留。只有 PROC_RUNNABLE 线程可被选中。
struct context 依次存放 ra、sp、s0 到 s11。Sv39 的 satp 由 MODE 和物理根页表 PPN 合成。
首次运行时 context.ra 为 forkret，已运行过的线程从已保存的 ra 和 sp 续跑。

[GUARANTEE]
cpu_idle、schedule、proc_run、switch_to 接口与骨架一致。
当前线程不需要切换时仍记录本次 runs。普通线程都不可运行时选择 idle。

[SPECIFICATION]
在选线程前清除需重调度标记，从当前节点后环形扫描。idle 从链表头开始。
跳过哨兵和非就绪线程。一圈内只有当前线程可运行时继续运行当前线程。
更新 current 和切换期间屏蔽本地中断。satp 发生改变时写入并执行 sfence.vma。
保存旧线程的被调用者保存寄存器、栈和返回地址。恢复新线程对应字段后 ret。
验收：官方 Schedule 的两个测试线程在反复主动让出时都能运行，init 能继续打印。
```

## 阶段 6

**构建、OpenSBI 交接与双版本评分**

```text
[PROMPT]
用 PowerShell 7 进入 WSL Ubuntu 22.04，在现有 RISC-V 工具链上执行构建、QEMU 启动与课程评分。
如果只看到固件输出或评分镜像启动失败，要定位到实际交接点和构建步骤。

[RELY]
镜像加载物理入口是 0x80200000，高地址链接入口是 0xffffffffc0200000。
OpenSBI 通过 a0/a1 传递 hartid 和 DTB 地址。写 satp 后，跳转前仍用低地址取指。
可用 QEMU 6.2/7.0 和 gdb-multiarch。评分注入器在临时副本中加测试入口。

[GUARANTEE]
make、make qemu、make debug 和 make grade 可用，QEMU 支持指定实际程序路径。
课程断言、测试线程和分值保持原样。评分入口传递原脚本结果。

[SPECIFICATION]
若 Domain0 Next Address 为零，检查 loader 与 -kernel 的区别，验证 OpenSBI 进入内核。
检查根索引 2 的低地址恒等过渡和索引 511 的高地址直映，完整装载 bootstacktop。
在评分临时副本中先注入再构建，确认真实 QEMU 启动和真实 GDB 连接。
分别记录普通启动与评分输出。核对 PMM、Process Create、Schedule 三项和总分。
区分框架 do_exit 进入监视器与启动故障。原样记录课程测试函数签名的编译警告。
```

## 阶段 7

**页表分配失败专项回归**

```text
[PROMPT]
在临时源码副本中验证页表分配的第二步失败场景，保留测试串口日志。

[RELY]
PMM 和启动页表自检结束后，根索引 0 为空。创建地址 0 的映射需要两张下级页表页。
先预留目标页，再临时持有连续空闲页，使分配器只剩一页供 get_pte 使用。

[GUARANTEE]
实际运行故障注入，并在 QEMU 6.2 与 7.0 各取得一份结果。
临时断言只存在于验证副本。

[SPECIFICATION]
确认剩余一页后调用 page_insert。期望 -E_NO_MEM，且根项为空、get_pte 查询返回空。
确认目标页引用数为零、空闲页数仍为一。归还目标页和临时持有页后核对全量空闲页数恢复。
验证测试结束后仍能完成 VMA 自检和执行 init。保存实际输出及运行版本。
```

## 阶段 8

**Challenge 思考题与原理核对**

```text
[PROMPT]
依据指导书，在 Markdown 报告中完成 Challenge 的三问，每问结合 Lab4 实际代码。

[RELY]
kern/sync/sync.h 对应本地中断状态保存与恢复，kern/driver/intr.c 操作 sstatus.SIE。
kern/mm/pmm.c 中 get_pte 的两个中间步骤结构相似，pte_t 表示页表项。

[GUARANTEE]
给出原中断开启与关闭两种情况、三种分页模式的比较，以及页表查询和创建接口的设计取舍。

[SPECIFICATION]
说明 SIE 检查、关中断和有条件恢复的顺序，推演外层关中断时内层结束的行为。
比较 Sv32、Sv39、Sv48 的索引位宽、层数、PTE 大小和页内偏移，解释逐级索引的重复结构。
讨论当前 create 参数如何复用查表代码，并说明何时拆出接口、如何让创建路径负责回滚。
区分当前单核实验中的本地中断保护与多核同步，并说明实验覆盖的分页和调度范围。
```

## 阶段 9

**报告统稿、交叉复核与归档**

```text
[PROMPT]
结合 Lab4 指导书、最终源码和实际日志，核对 Markdown 报告、分工与提示词文档，整理提交包。

[RELY]
实验报告、小组分工和完整提示词放在 report/，原始验证输出位于 report/logs/。
Lab4 核心功能依次是页操作、进程创建和调度，Challenge 为书面思考题。
学校提交要求中的学号标记、课程用例和评分条件以原骨架及指导书为准。

[GUARANTEE]
提交包内含 Lab4 源码、报告和可追溯的验证日志，报告和分工、提示词相互可跳转。
文档把责任安排、已运行测试、成员复核和课程验收按各自证据记录。

[SPECIFICATION]
复核实验执行链、各练习规格和四段式提示词，以及兼容性问题和修复过程。
依据原始日志写评分与回滚结果，标注 do_exit 占位行为和已知的课程测试编译警告。
核对 Lab4 学号标记和课程测试原件，检查所有文档链接。归档后核对文件清单和内容。
```

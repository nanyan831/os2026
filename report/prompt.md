# Lab3各阶段提示词汇总

- [阶段 1：Lab2 内存管理迁移与接口保护](prompts/01-lab2-port.md)（王子卓，3点）
- [阶段 2：构建、固件交接与分页过渡](prompts/02-build-and-boot.md)（缪臻，3点）
- [阶段 3：Trap 上下文恢复与汇编入口](prompts/03-trap-context.md)（刘宸旭，4点）
- [阶段 4：中断分发、连续时钟与自动关机](prompts/04-timer-interrupts.md)（王子卓，4点）
- [阶段 5：断点、非法指令与 EPC 长度处理](prompts/05-exception-handlers.md)（缪臻，4点）
- [阶段 6：寄存器上下文独立测试与 GDB 往返验证](prompts/06-context-tests-and-gdb.md)（刘宸旭，3点）
- [阶段 7：First-Fit 尾插回归与中断保护验证](prompts/07-pmm-and-sie-regression.md)（王子卓，2点）
- [阶段 8：异常边界、综合验收与交付归档](prompts/08-validation-and-delivery.md)（缪臻，2点）
- [阶段 9：报告统稿、提示词汇总与交叉复核](prompts/09-report-and-review.md)（刘宸旭，2点）

## 阶段 1：Lab2 内存管理迁移与接口保护

**责任成员：** 王子卓（2411070）。**工作量：** 3点。独立文件：[阶段1](prompts/01-lab2-port.md)。

```text
[PROMPT]
任务：以课程 Lab3 骨架为基础，在 code/kern/mm/pmm.c 和 best_fit_pmm.c 的 lab2 留空位置补齐上一实验的必要实现。
操作要求：直接修改真实工程文件，对照已有 Lab2 的 page_init 和 Best-Fit 迁移；保留课程固定检查函数、Lab3 原子操作和中断保护。
输出要求：可编译的物理页初始化、完整 Best-Fit 管理器、必要常量和可选择的 PMM 配置；说明迁移边界。

[RELY]
struct Page 包含 ref、flags、property、page_link；RV64 下实际大小为40字节。
PGSIZE=4096，DRAM_BASE=0x80000000，PHYSICAL_MEMORY_END=0x88000000，PHYSICAL_MEMORY_OFFSET=0xffffffff40000000。
get_memory_base/get_memory_size 提供 DTB 内存范围；pages 是描述符数组虚拟地址，npage 为物理页号上界，数组长度是 npage-nbase。
可使用 end、PADDR、pa2page、PPN、ROUNDUP、ROUNDDOWN、init_memmap、panic、cprintf 和框架 Page 状态宏。
Lab3 的 alloc_pages/free_pages/nr_free_pages 已通过 local_intr_save/restore 保护调用；memlayout.h 使用 atomic.h。
Makefile 会编译 kern/mm 下全部 C 文件，不能因为默认使用 First-Fit 就留下 Best-Fit 实现空缺。

[GUARANTEE]
static void page_init(void);
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
const struct pmm_manager best_fit_pmm_manager;
保持原有公开 PMM 接口，支持 PMM=first_fit|best_fit，默认保持 Lab3 的 first_fit。

[SPECIFICATION]
page_init
Pre-Condition：启动地址映射、内核栈和 DTB 解析可用；管理器已 init，但尚未登记空闲区。
Post-Condition：va_pa_offset、npage、pages 正确；固件、内核、Page 数组和对齐空隙均保留；只有元数据之后的完整页面进入管理器。
Case 1：合法 RAM。按 DTB 与框架上界确定管理末端，检查描述符数组能容纳后再初始化，并登记非空空闲区。
Case 2：基址不支持、零大小、加法溢出或空间不足。明确 panic，不越界初始化或登记零页区间。
Best-Fit
Pre-Condition：登记区间合法且描述符处于 reserved；申请 n>0，释放属于先前分配的合法区间。
Post-Condition：申请选择最小满足块，同大小取先遇到的低地址块；分裂、地址顺序、相邻合并及空闲页计数正确。
Case 3：精确匹配、分裂和无连续满足块分别处理；失败返回 NULL 且不破坏状态。
Requirements：保留 basic_check/best_fit_check 原文；保留 Lab3 的中断保护和原子位操作；不整份覆盖 pmm.c/memlayout.h，不迁入 Buddy、SLUB 或 MMU 探针。
```

## 阶段 2：构建、固件交接与分页过渡

**责任成员：** 缪臻（2413807）。**工作量：** 3点。独立文件：[阶段2](prompts/02-build-and-boot.md)。

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

## 阶段 3：Trap 上下文恢复与汇编入口

**责任成员：** 刘宸旭（2414139）。**工作量：** 4点。独立文件：[阶段3](prompts/03-trap-context.md)。

```text
[PROMPT]
任务：补齐 code/kern/trap/trapentry.S 两处 lab3 function1 标记，实现 RESTORE_ALL 和 __alltraps 调用 C 处理函数的路径。
操作要求：依据真实 SAVE_ALL 和 struct trapframe 实现；保留已填写课程标记、SAVE_ALL 原文、结构体布局和 trap 函数签名。
输出要求：形成保存现场、调用处理函数、恢复现场和 sret 返回的完整闭环，并给出帧偏移与恢复顺序说明。

[RELY]
RV64 的 REGBYTES=8，struct trapframe 共36个槽位、288字节；0..31为通用寄存器，32为status，33为epc，34/35为badvaddr/cause。
SAVE_ALL 先通过 sscratch 保存原 sp，再将其存入第2槽；保存结束后 sp 指向帧基址。
void trap(struct trapframe *tf) 按 RISC-V ABI 通过 a0 接收帧指针，并允许修改帧内 epc。
硬件入 Trap 时保存先前特权级到 SPP、先前中断状态到 SPIE，并清 SIE；本次主要处理 S 模式内核中的 Trap。

[GUARANTEE]
.macro RESTORE_ALL
__alltraps
保持现有 __trapret、sret、SAVE_ALL、struct trapframe 和 void trap(struct trapframe *tf) 接口。

[SPECIFICATION]
RESTORE_ALL
Pre-Condition：当前 sp 指向由 SAVE_ALL 产生的合法帧，C 处理函数已返回，帧可能包含更新后的 epc。
Post-Condition：所有可写通用寄存器及返回所需 sstatus/sepc 恢复，返回后原执行流能够继续。
Case 1：先使用帧基址恢复返回 CSR，再恢复通用寄存器；作为临时寄存器使用的寄存器最终也恢复。
Case 2：原 sp 来自第2槽，必须在所有依赖当前帧基址的访存结束后恢复；不得提前改变 sp 或靠错误的统一偏移恢复它。
__alltraps
Pre-Condition：SAVE_ALL 已构造帧，栈满足 ABI 对齐。
Post-Condition：trap 接收到当前帧地址；C 函数返回后 sp 仍指向该帧，并衔接现有 __trapret。
Case 3：把帧地址作为第一个实参调用 trap，保持后续恢复需要的信息和原调用现场。
Requirements：不无条件清 SPP，不在恢复途中主动打开 SIE；由 sret 按保存的 SPP/SPIE 返回。badvaddr/cause 用于诊断，不作为可恢复通用寄存器写回。
```

## 阶段 4：中断分发、连续时钟与自动关机

**责任成员：** 王子卓（2411070）。**工作量：** 4点。独立文件：[阶段4](prompts/04-timer-interrupts.md)。

```text
[PROMPT]
任务：在 code/kern/trap/trap.c 的 function2 允许区域实现 interrupt_handler，完成中断类型提示、时钟重设、计数和关机。
操作要求：保留区域外的函数和已有 print_ticks；使用框架 IRQ 宏、clock 接口及 SBI 声明，不修改评分断言。
输出要求：可持续产生时钟中断，每100次输出统计，第10次统计后真正关机；其他中断给出类型或完整兜底信息。

[RELY]
void interrupt_handler(struct trapframe *tf) 接收 cause 含最高中断标志位的帧；trap_dispatch 已完成中断/异常入口分类。
clock_set_next_event() 使用一次性 SBI 定时器；volatile ticks 记录次数，TICK_NUM=100。
框架有 IRQ_U/S/H/M_SOFT、IRQ_U/S/H/M_TIMER、IRQ_U/S/H/M_EXT 等宏，以及 print_trapframe、print_ticks 和 sbi_shutdown。
指导书命名 SHUTDOWN_AFTER 但未给数值；本实现显式采用10，需保留这一参数说明。
官方测试要求可识别 Supervisor software interrupt、Unknown interrupt: cause 15、100 ticks 等输出。

[GUARANTEE]
void interrupt_handler(struct trapframe *tf);
允许指定区域内的必要模块状态和声明；复用 clock_set_next_event、print_ticks、sbi_shutdown。

[SPECIFICATION]
Pre-Condition：stvec 和 Trap 返回路径可用，tf 指向合法帧；真实时钟路径已经初始化 clock。
Case 1：IRQ_S_TIMER。每次重设下一事件，ticks 恰增加一次；每达到100的整数倍调用已有统计输出，并累计统计次数。
Case 2：第10次统计输出。调用 sbi_shutdown 结束正常运行；不能靠超时终止或伪造退出状态认定成功。
Case 3：已定义的其他软件、时钟或外部中断。输出对应类型提示；本实验不扩展为完整外设驱动。
Case 4：未知原因号。去掉最高中断标志位后输出原因，并调用 print_trapframe。
Post-Condition：未关机时正确返回原执行流，时钟能够再次到来；中断处理不改变 tf->epc。
Requirements：不得只计数不重设下一事件；不得为评分配置单独关闭正常关机；输出及退出码均来自真实内核/QEMU。
```

## 阶段 5：断点、非法指令与 EPC 长度处理

**责任成员：** 缪臻（2413807）。**工作量：** 4点。独立文件：[阶段5](prompts/05-exception-handlers.md)。

```text
[PROMPT]
任务：在 code/kern/trap/trap.c 的 function2 允许区域实现 exception_handler 及内部指令长度助手。
操作要求：使用真实 CAUSE 宏和 trapframe 字段，严格保留处理区域外函数；按指令编码更新返回位置，不统一假定 epc+4。
输出要求：16/32位断点与非法指令都能处理后返回，未知异常输出帧，规定输出文本可以被官方测试识别。

[RELY]
void exception_handler(struct trapframe *tf) 接收同步异常帧；tf->cause 表示原因，tf->epc 指向触发异常的指令地址。
CAUSE_ILLEGAL_INSTRUCTION=2，CAUSE_BREAKPOINT=3；可使用 cprintf、print_trapframe 和 panic。
本实验异常指令位于可读的内核地址；不能依赖 stval/badvaddr 一定提供完整指令。
16位编码的最低两位不为11；标准32位编码的最低两位为11且最低5位不全为1。
边界测试编码为断点9002/00100073、非法指令0000/0000007b。

[GUARANTEE]
static size_t exception_instruction_length(uintptr_t epc);
void exception_handler(struct trapframe *tf);
保持 trap_dispatch、trap、print_trapframe 和已有公开接口不变。

[SPECIFICATION]
exception_instruction_length
Pre-Condition：epc 为本实验可读内核指令地址。
Case 1：16位编码，返回2；Case 2：标准32位编码，返回4；Case 3：更长且本实现不支持的编码，明确终止，不错误声称支持。
exception_handler
Pre-Condition：tf 为合法同步异常帧，已识别异常的 epc 可读取。
Case 4：非法指令。输出 Illegal instruction at 和地址，依据实际长度后移 tf->epc。
Case 5：断点。输出 Breakpoint at 和地址，依据实际长度后移 tf->epc。
Case 6：其他异常。调用 print_trapframe，不擅自跳过未知指令或伪造恢复。
Post-Condition：已支持异常返回到对应下一条指令，既不重复陷入原指令，也不跳过后续正常指令。
Requirements：不修改通用寄存器现场；范围限于本实验整数内核路径，不将它描述为完整用户态安全取指或通用异常恢复。
```

## 阶段 6：寄存器上下文独立测试与 GDB 往返验证

**责任成员：** 刘宸旭（2414139）。**工作量：** 3点。独立文件：[阶段6](prompts/06-context-tests-and-gdb.md)。

```text
[PROMPT]
任务：独立验证 Trap 保存与恢复，不只依赖打印标记；在附加汇编和 GDB 中检查寄存器、帧、原栈和返回位置。
操作要求：附加测试只注入临时副本；保持官方 test_lab3_context.S 不变；测试函数自身遵守调用方 ABI。
输出要求：哨兵寄存器检查、具名结果、真实 GDB 会话、源版本指纹和断言失败时的原始输出。

[RELY]
RESTORE_ALL、__alltraps、trap 和 __trapret 已形成闭环，帧为288字节。
附加源文件为 code/tools/test_lab3_extra_context.S 与 test_lab3_extra.c，临时复制到 kern/extra。
公开测试接口 uintptr_t lab3_extra_register_context(void)；Python 验证入口 gdb_trace(qemu, label)。
ra、t、s、a 类寄存器可设置独立哨兵；gp/tp 保留给 C 处理函数使用并比较前后原值。
GDB 可通过 QEMU stub 观察 __alltraps、trap、__trapret 和被打断PC；不使用旧构建硬编码地址。

[GUARANTEE]
uintptr_t lab3_extra_register_context(void);
verify_lab3.py 中可复现的 gdb_trace(qemu, label)。
测试返回0表示比较通过，非0由调用者断言；记录真实原 sp、a0、sepc、scause 和 sstatus。

[SPECIFICATION]
寄存器测试
Pre-Condition：内核栈和 Trap 路径可用；调用方现场必须先由测试自身妥善保存。
Case 1：给可设置的通用寄存器写入独立值后触发强制32位断点，返回后逐一比较；a0 必须在作为比较暂存前核对原值。
Case 2：比较原 sp、gp、tp；恢复测试调用者的被调用者保存寄存器及返回地址，不能让测试自身破坏 ABI。
GDB 往返
Case 3：在首次真实时钟进入时保存原 sp/epc；在 C 函数机器入口确认 a0 等于帧地址、帧长288、帧内原 sp/epc 正确。
Case 4：在 __trapret 检查帧基址和时钟 epc 未变；返回原 PC 后确认原 sp 及 SIE 恢复。
Post-Condition：具名 GPR 和 GDB 断言均通过，输出可回读；测试进程和自己的调试 QEMU 正确结束。
Requirements：完整保留源码与测试指纹；区分真实执行、GDB检查和已保存日志回读；不把整数现场检查扩展为浮点/向量上下文证明。
```

## 阶段 7：First-Fit 尾插回归与中断保护验证

**责任成员：** 王子卓（2411070）。**工作量：** 2点。独立文件：[阶段7](prompts/07-pmm-and-sie-regression.md)。

```text
[PROMPT]
任务：通过公开 PMM 接口独立验证尾插合并和中断状态保留，复现 First-Fit 实际问题后只做最小修复。
操作要求：不修改 basic_check/default_check 或 Best-Fit 固定检查；用临时附加测试驱动当前管理器。
输出要求：失败日志、两处尾插退出循环修复，以及 First-Fit/Best-Fit 的通过与资源恢复证据。

[RELY]
附加测试在 tools/test_lab3_extra.c 的 check_tail_insertion、check_interrupt_state 中组织。
alloc_pages/free_pages/nr_free_pages 为公开接口，管理器自检后可用区恢复为一个连续区间。
原 default_init_memmap/default_free_pages 尾部 list_add 后继续遍历，可能再次处理新节点，破坏合并使用的前驱关系。
local_intr_save/restore 保存先前 SIE 状态，恢复时只能恢复到原状态；测试发生在时钟尚未开放的阶段。

[GUARANTEE]
static void check_tail_insertion(void);
static void check_interrupt_state(void);
测试入口由 lab3_extra_check 聚合；保持两个固定检查区域原文。
default_init_memmap/default_free_pages 仅在已经完成尾插的分支补上必要的循环结束。

[SPECIFICATION]
Pre-Condition：当前管理器初始化与自检已完成，空闲页数由运行时 nr_free_pages 获取，不硬编码地址和页数。
Case 1：申请整个连续可用区间，依次释放两个非相邻页，重新申请时地址正确；释放全区后计数恢复。
Case 2：再申请整个区间，依次释放相邻两页，2页申请必须返回该连续区间；此场景应暴露尾插后前驱关系损坏造成的合并失败。
Case 3：仅在实际复现失败后修复算法循环；记录原始 panic/超时，不能把终止失败运行算作正常关机。
Case 4：分别从 SIE=0、SIE=1 状态调用分配、统计和释放，调用后保留原状态并恢复全部资源。
Case 5：两层 local_intr_save 后，内层恢复仍关闭中断，外层恢复才回到原开启状态。
Post-Condition：两个管理器均通过回归，资源计数等于初值，附加测试不留在正式 kern 初始化中。
Requirements：使用可观察结果验证，不读取算法私有链表替代行为测试；SIE 检查不被描述为多核竞争验证。
```

## 阶段 8：异常边界、综合验收与交付归档

**责任成员：** 缪臻（2413807）。**工作量：** 2点。独立文件：[阶段8](prompts/08-validation-and-delivery.md)。

```text
[PROMPT]
任务：汇集各模块已有独立验证，补齐异常长度边界，完成两套 QEMU 的综合矩阵、真实证据和交付包。
操作要求：在最终源码的临时副本运行官方测试和附加检查；固定测试文件不变；原始失败与适配后成功分别留存。
输出要求：两套矩阵、构建/评分/串口日志、真实终端截图、指纹与交付审计、可复现的源码和报告归档。

[RELY]
官方 grade.sh 三组分值35/25/40，分别检查断点上下文、非法指令与兜底、持续时钟达到200 ticks。
verify_lab3.py 的 grade/boot/extra/gdb_trace 组成6项场景；阶段6和7提供上下文、GDB、PMM和SIE独立检查。
make grade 在临时目录注入；原工程不能出现 kern/test、kern/extra、test_lab3.h 或已注入标记。
正常配置每100 ticks统计一次，共10次后通过 SBI 关机；两个 QEMU 分别为6.2/OpenSBI0.9、7.0/OpenSBI1.0。
capture_evidence.py 生成真实终端截图，audit_lab3.py 校验框架、来源指纹、链接与图片。

[GUARANTEE]
verify_lab3.py 中 grade(qemu,label)、boot(qemu,label,pmm)、extra(qemu,label,pmm) 及矩阵入口。
report/logs 中两套结果与原始证据，report/images 中五张截图，report/audit.json。
来自本地 Git 提交的交付 ZIP 与 SHA-256，保留 wrapper 的0755权限与LF行尾。

[SPECIFICATION]
Pre-Condition：阶段1至7的接口完整，官方五个测试/注入文件与原始SHA-256一致。
Case 1：在临时副本强制9002/00100073断点和0000/0000007b非法编码，结合反汇编确认长度及返回标记。
Case 2：分别运行官方评分、两种PMM普通关机、两种PMM附加回归和GDB，共6项；记录退出码、是否超时、QEMU路径及源码指纹。
Case 3：普通计时必须有10行100 ticks并真实退出；未知中断/异常的构造帧测试应明确其证据范围。
Case 4：真实终端执行构建/评分，或明确回读命名的原始日志和JSON；图片不得代替运行数据。
Case 5：核对Git归档CRC、源码和固定测试指纹、执行权限；解压重建后比较内核镜像内容。
Post-Condition：矩阵、日志和截图对应同一实现，证据完整；代码、报告与交付版本一致。
Requirements：透明说明payload适配；保留真实失败；不纳入bin/obj/.git/缓存；本地提交与远程推送分开，推送须有明确授权。
```

## 阶段 9：报告统稿、提示词汇总与交叉复核

**责任成员：** 刘宸旭（2414139）。**工作量：** 2点。独立文件：[阶段9](prompts/09-report-and-review.md)。

```text
[PROMPT]
任务：依照本地实验报告模板和 Lab2 文档形式，汇总 Lab3 各阶段提示词、平衡小组分工并完成报告整体编排。
操作要求：各成员撰写自己模块对应章节和提示词，统稿负责人汇总目录、目的、执行流和总结；保留真实证据与未验证边界。
输出要求：报告、含完整阶段正文的 prompt.md、九份独立提示词、责任分工表、README 和有效资料链接。

[RELY]
小组成员为刘宸旭2414139、王子卓2411070、缪臻2413807。
等量分工按实现难度、验证成本及文档责任估算，每人3阶段、9工作量点，各占总量1/3；工作量点不是课程分数或实际工时。
王子卓负责阶段1/4/7，缪臻负责阶段2/5/8，刘宸旭负责阶段3/6/9。
Lab2 的 prompt.md 包含阶段目录和所有完整四段式正文，report/prompts/ 下保留逐阶段文件。
当前 Lab3 日志保留原始加载失败、相邻尾块回归失败、分页过渡修复以及两套最终矩阵。

[GUARANTEE]
README.md、report/report.md、report/division.md、report/prompt.md、report/prompts/01..09。
每阶段明确责任成员、工程接口、前后条件、边界场景、约束及实际证据入口。
报告总体结构遵循实验基本信息、目的、环境、整体逻辑、实现、测试验证、总结。

[SPECIFICATION]
Pre-Condition：实现和日志已有可核查结果，成员责任已按本次等量原则明确。
Case 1：同步README、报告总表、模块负责人、阶段提示词和分工页；每人包含核心实现、独立复核和文档工作。
Case 2：汇总页中的每段四段式正文与独立文件完全一致；包含具体接口、Pre-Condition、Post-Condition、Case和Requirements。
Case 3：按“刘复核王、王复核缪、缪复核刘”设置交叉复核关系，统稿不替代成员的模块章节责任。
Case 4：保留真实失败及自动验证边界；精确模型型号未独立核验的部分不补写猜测值，责任分配不作为历史人工操作记录。
Post-Condition：小组分工一致，提示词目录和正文完整，所有本地链接有效，截图与日志可追溯。
Requirements：文档修订不改算法、课程标记、固定测试和原始运行日志；复用已有验证时明确是已有实测记录，本次只重做文档与指纹审查。
```

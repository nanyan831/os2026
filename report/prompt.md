# Lab2各阶段提示词汇总

以下按任务分解整理本次实际采用的规格和操作指令，保留四段式结构，不伪造成用户逐条发送过的额外对话。实际失败、修订和验证见[迭代记录](iterations.md)。

- [阶段 1：物理内存探测与页描述符初始化](prompts/01-page-init.md)
- [阶段 2：完整 Best-Fit 管理器](prompts/02-best-fit.md)
- [阶段 3：构建、固件交接与分页过渡](prompts/03-build-and-boot.md)
- [阶段 4：Buddy System 实现提示词](prompts/04-buddy-implementation.md)
- [阶段 5：Buddy 独立测试提示词](prompts/05-buddy-tests.md)
- [阶段 6：简化 SLUB Challenge 实现提示词](prompts/06-slub-implementation.md)
- [阶段 7：简化 SLUB 独立测试提示词](prompts/07-slub-tests.md)
- [阶段 8：QEMU 页表与软件 TLB 源码调试](prompts/08-qemu-source-debug.md)
- [阶段 9：最终验证、截图、报告与提交](prompts/09-validation-and-delivery.md)
- [追加阶段：First-Fit 尾插与合并回归](prompts/10-first-fit-regression.md)

## 阶段 1：物理内存探测与页描述符初始化

本阶段实施规格，2026-10-18。真实测试与修订记录在 `../iterations.md`。

```text
[PROMPT]
任务：在 code/kern/mm/pmm.c 的 lab2 function1 标记位置实现 page_init。
操作要求：直接修改工程文件，保留 pmm_init、分配器接口和课程测试；将标记中的 YOUR CODE 替换为 2413807。
输出要求：交付可编译的实际实现、内存布局输出及验证结果。先验证单文件，再与 Best-Fit 完整模块一起构建。

[RELY]
struct Page { int ref; uint64_t flags; unsigned int property; list_entry_t page_link; }。
PGSIZE=4096，DRAM_BASE=0x80000000，PHYSICAL_MEMORY_END=0x88000000，PHYSICAL_MEMORY_OFFSET=0xffffffff40000000。
pages 是 Page 数组虚拟地址；nbase=DRAM_BASE/PGSIZE；npage 是物理页号上界；数组长度为 npage-nbase。
可调用 get_memory_base、get_memory_size、panic、cprintf、init_memmap、pa2page；可用 PADDR、PPN、ROUNDUP、ROUNDDOWN、Page 状态宏和 end 链接符号。
init_memmap 通过当前 pmm_manager 注册空闲区域；其输入页描述符应事先标为 reserved。

[GUARANTEE]
static void page_init(void);
可新增 static 辅助函数；保持现有公开接口与固定测试不变。

[SPECIFICATION]
page_init
Pre-Condition：启动分页与 DTB 解析已完成，PMM 已 init，但没有注册空闲页；本实验只有从 DRAM_BASE 开始的一个 RAM 区域。
Post-Condition：va_pa_offset、npage、pages 正确；描述符覆盖受支持 RAM；OpenSBI、内核、描述符和对齐空隙均保留；只有之后完整页面进入分配器。
Case 1：合法范围。根据 DTB 和框架上界确定管理末端；打印探测范围及实际管理范围；描述符数组从 ROUNDUP(end,PGSIZE) 开始，确认其物理末端未越界之后才写入。先将所有描述符初始化为 reserved、ref=0，再注册对齐后的非空空闲区。
Case 2：无效基址、零大小、加法溢出、受支持区间不足以容纳内核/描述符。panic 并终止，不访问越界描述符。
Case 3：没有完整可分配页面。不得调用 init_memmap；给出明确 panic。
Requirements：必须先设置偏移再调用 PADDR；不得混用 VA、PA、PPN 和 Page 指针；不得把 npage 当作数组元素数量；不硬编码 free 页数。
```


## 阶段 2：完整 Best-Fit 管理器

```text
[PROMPT]
任务：在 code/kern/mm/best_fit_pmm.c 的 lab2 function2-2 位置实现完整 Best-Fit，接入 pmm.c。
操作要求：真实修改文件；保留 basic_check 和 best_fit_check 的全部原始文本；标记学号为 2413807。
输出要求：完整管理器、默认 Best-Fit 配置、可复现的普通运行/测试/评分证据。

[RELY]
阅读 default_pmm.c、pmm.h、memlayout.h、list.h 的真实定义。
struct pmm_manager 包含 name/init/init_memmap/alloc_pages/free_pages/nr_free_pages/check。
free_area_t 包含 free_list 和 nr_free；Page.property 是空闲块页数，PG_property 只标记块头。
固定测试会直接保存、清空、恢复本文件的 free_list 和 nr_free；还会部分释放五页分配，并要求一页请求选择单页块而非两页块。
alloc_pages/free_pages 通过当前管理器派发；测试回调运行前 PMM 已初始化且有空闲页。

[GUARANTEE]
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
const struct pmm_manager best_fit_pmm_manager;
check 指向原有 best_fit_check；必要时添加前置声明。

[SPECIFICATION]
init/init_memmap
Pre-Condition：init 先于 memmap；待注册描述符均 reserved。
Post-Condition：维护按地址递增的空闲块链表与准确空闲页计数，每块只有头页 PG_property=1；初始化时清零 ref。
alloc_pages
Pre-Condition：n>0。
Post-Condition：返回最小满足块的前 n 页，若同大小取较低地址；返回区间不再空闲，余块位置、长度和计数正确。
Case 1：精确匹配，移除块；Case 2：较大块，保留 n 页之后的余块；Case 3：没有连续满足块或空链表，返回 NULL 且状态不变。
free_pages
Pre-Condition：合法的已分配连续区间，可为早先分配的子区间；不允许释放保留页或重复释放。
Post-Condition：ref 清零，空闲计数增加 n；按地址插入并与物理连续前后块合并，清除不再作为块头的标记。
nr_free_pages 返回准确计数，不依赖不可随固定测试一起恢复的缓存。
Requirements：固定测试不可修改；分配失败不破坏状态；完成后实际启用 best_fit_pmm_manager。
```


## 阶段 3：构建、固件交接与分页过渡

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


## 阶段 4：Buddy System 实现提示词

```text
[PROMPT]
任务：从零新增 code/kern/mm/buddy_pmm.c、buddy_pmm.h，接入现有 PMM；Makefile 支持 PMM=buddy，默认仍为 best_fit。
操作要求：真实修改工程，保持 First-Fit/Best-Fit 固定测试与官方注入流程；本提示词只规定实现接口，下一提示词独立设计测试。
输出要求：完整按阶空闲表、正确分配/释放与可重复选择配置；描述元数据和空间损耗。

[RELY]
struct Page 的 ref、flags、property、page_link；pmm.h 的六项回调、page2ppn、page2pa、pa2page；list.h 链表操作。
物理空闲区起点经过内核和描述符布局计算，通常不是最大阶对齐；初始化的描述符处于 reserved。
alloc_pages/free_pages 接口允许处理至少 n 页。内核为单 hart、无并发分配；不需要引入锁。

[GUARANTEE]
const struct pmm_manager buddy_pmm_manager;
六项管理器回调和 void buddy_check(void) 测试回调接入点。
size_t buddy_block_pages(struct Page *base); void buddy_validate(void);
新增头文件公开测试所需的只读状态检查接口，内部辅助函数 static。

[SPECIFICATION]
init/init_memmap
Pre-Condition：先 init，后将合法 reserved 区间交给 init_memmap。
Post-Condition：区间完整分解为以物理页号对齐的2的幂次块，计数不丢页，只有空闲块头带 PG_property，内部元数据状态清晰。
alloc_pages
Case 1：n>0，向上取整到2的幂，找到不小于目标阶的最小非空阶，递归拆分；返回对齐且不重叠块，实际占用页数可查询，nr_free 减少实际页数。
Case 2：n=0、超过最大阶或连续块不足，返回 NULL，状态不变。
free_pages
Pre-Condition：base 是先前分配的块头，n 的取整大小与实际块一致；禁止 interior free、double free、reserved free。
Post-Condition：按真实块大小回收，以物理页号异或找到同阶伙伴，只有伙伴确实空闲且同阶才合并，递归向上；nr_free 增加实际页数。
buddy_validate 检查每阶块大小、对齐、边界、链表和空闲总计，不修改分配状态。
Requirements：不修改 struct Page 布局；可用附加 flags 区分 allocated 和 allocated-head；ref 保持正常语义；不得假设空闲区天然最大阶对齐。
```


## 阶段 5：Buddy 独立测试提示词

```text
[PROMPT]
任务：依据 Buddy 的接口契约，在 code/kern/mm/buddy_test.c 独立设计 buddy_check。真实运行，不能仅复述实现逻辑。
操作要求：不访问实现的私有空闲链表；通过 alloc_pages/free_pages/nr_free_pages、地址和公开只读检查验证可观察行为；不创建 kern/test 构建目录。
输出要求：断言、具名通过项、资源恢复证据，以及各测试为何能发现错误的说明。

[RELY]
上一阶段的外部契约：向上取整、物理页号对齐、实际占用计数、同阶伙伴合并、分配失败不改变状态。
buddy_block_pages 只读查询实际块大小，buddy_validate 检查内部不变量。
测试开始时可用页数由 nr_free_pages 动态取得，不能硬编码本机内核大小。

[GUARANTEE]
void buddy_check(void);
测试独立源文件，可使用静态测试指针数组；退出时所有测试占用必须释放。

[SPECIFICATION]
Pre-Condition：当前管理器为 Buddy，初始空闲区已建立。
Post-Condition：无 assert/panic；结束空闲页数等于开始值；输出每组行为结果。
Case 1：请求1、2、3、5、17、64页，以独立的幂次取整期望验证实际页数、对齐、互不重叠和计数变化；在块中写入并检查数据，防止地址重叠。
Case 2：零大小和超过内存的请求返回 NULL，计数不变。
Case 3：耗尽全部可用单页，验证失败；在一个对齐16页区域内只释放两个非伙伴页，2页申请必须失败；补齐真伙伴后2页成功，再逐级补齐并申请4页/16页，证明递归合并。释放其余单页并核对完全恢复。
Case 4：固定种子的混合申请/释放，检查地址区间和数据；全部释放后计数恢复。
Requirements：构造能够击穿“只相邻就合并”和“从不合并”两种错误的案例；不能把 Best-Fit 固定测试改成 Buddy 测试；不依据管理器自己声称成功就判通过。
```


## 阶段 6：简化 SLUB Challenge 实现提示词

```text
[PROMPT]
任务：从零新增 code/kern/mm/slub.c、slub.h，基于当前页分配器实现两层对象分配，作为 SLUB 思想的简化教学实现。
操作要求：真实接入内核的可选 SLUB_TEST 配置；不得声称实现 Linux 完整 SLUB；保持默认评分路径不变。
输出要求：对象大小类别、页内空闲对象组织、对象验证和空页回收；下一阶段单独设计测试。

[RELY]
底层 alloc_pages/free_pages/nr_free_pages；上层只能使用内核已提供的类型、链表、字符串操作和 VA/PA 转换。
Best-Fit 精确分配页数，Buddy 向上取整；可用 buddy_block_pages 确认 Buddy 实际占用。
当前内核单 hart，无并发；对象要求至少16字节对齐。

[GUARANTEE]
void *slub_alloc(size_t size); int slub_free(void *ptr);
size_t slub_pages_in_use(void); void slub_check(void);
辅助函数和缓存状态限制在模块内；不修改已有 PMM 公开接口。

[SPECIFICATION]
alloc
Pre-Condition：物理页管理器已初始化。
Case 1：1..2048字节，选择16/32/64/128/256/512/1024/2048类别，先复用非满 slab 页中的空闲对象，无可用对象才申请新页。对象区和头部不重叠。
Case 2：更大对象，检查长度加法溢出后申请连续页，保存真实分配信息。
Case 3：零大小、溢出或资源不足，返回 NULL，不泄漏。
free
Case 1：合法对象，回收到其类别；满页重新成为可分配页；最后一个对象释放时归还整页。
Case 2：合法大对象，归还对应完整分配块。
Case 3：NULL 为无操作；非对象起点或已释放对象返回 -1，不破坏状态。
Post-Condition：活跃对象不重叠，返回值对齐；所有对象释放后本模块页占用回到0，底层资源恢复。
Requirements：用 bitmap 验证对象是否正在分配；每页只有一个页头；不保留空缓存页；计数考虑 Buddy 的取整占用。
```


## 阶段 7：简化 SLUB 独立测试提示词

```text
[PROMPT]
任务：在 code/kern/mm/slub_test.c 独立测试上一阶段对象分配契约；分别在 Best-Fit 和 Buddy 下真实运行。
操作要求：只依据接口、地址、数据和底层空闲页数断言，不复制分配器查找逻辑。
输出要求：具名测试结果、边界、复用、数据独立性和回收证据。

[RELY]
slub_alloc/slub_free/slub_pages_in_use；nr_free_pages；对象16字节对齐；所有释放后不保留空页。

[GUARANTEE]
void slub_check(void);
保留测试前后资源计数，失败通过 assert 明确暴露。

[SPECIFICATION]
Pre-Condition：页分配器检查已经通过。
Case 1：测试0、1、15、16、17、31、32、33、2048、2049、4096、6000、16384和溢出长度。
Case 2：给同时存活对象写入不同模式，验证每个请求范围中的内容不被其他对象覆盖及16字节对齐。
Case 3：同一类别跨多个 slab 页，先释放部分再分配，检查复用且页占用未增加；奇偶次序释放后全部回收。
Case 4：释放内部偏移地址和已释放对象应失败，合法对象及计数保持正确；NULL 释放成功且无副作用。
Post-Condition：slub_pages_in_use()==0，nr_free_pages 等于初始值；分别在两个底层分配器配置留证。
Requirements：测试验证尺寸边界与资源守恒，不能仅检查返回非NULL。
```


## 阶段 8：QEMU 页表与软件 TLB 源码调试

```text
[PROMPT]
任务：依据指导书编译 QEMU 4.1.1 的独立调试版本，实际观察一条内核 load 的 Sv39 翻译、软件 TLB 填充和代码 TLB 命中；对比 Bare 模式。
操作要求：使用真实内核与 QEMU 源码，按当前版本找断点；宿主 GDB 启动自己的 QEMU 子进程，避免修改系统 ptrace 策略；保留内核 GDB 的分页过渡观察。
输出要求：可重复运行脚本、原始 GDB 日志、关键调用路径与分支解释、准确的 VA/PTE/PA/软件TLB数据。

[RELY]
QEMU 4.1.1 源码 target/riscv/cpu_helper.c 的 riscv_cpu_tlb_fill、get_physical_address；accel/tcg/cputlb.c 的 get_page_addr_code、tlb_fill、tlb_set_page_with_attrs。
当前启动根页表索引2为低地址恒等映射，索引511为高地址1GiB叶子；内核映射偏移0xffffffff40000000。
添加一个对齐的 volatile probe word 和连续两次 load，保证可观察的数据地址与结果；probe 前执行 sfence.vma。
裸地址入口0x80200000，符号的实际地址由当前 ELF/nm 读取，不能沿用旧构建常量。

[GUARANTEE]
void mmu_probe(void); volatile uint64_t mmu_probe_word;
可重现的宿主 GDB 源码跟踪工具及内核 tools/trace_paging.py。
调试日志标明调试的是宿主 QEMU 还是 RISC-V 客体，不混淆软件 TLB 和真实硬件 TLB。

[SPECIFICATION]
Pre-Condition：默认 Best-Fit 内核可正常启动，独立 QEMU 含源码与调试符号。
Case 1：在 mmu_probe_word 的真实 data load 上观察 tlb_fill 进入页表遍历，记录 satp、访问类型、PTE地址/值、叶子层级；返回 caller 后核对 pa=va-offset，并观察软件TLB填充。
Case 2：在 get_page_addr_code 的 C 查询处，记录同一代码页从无效 tag 到有效 tag 的变化，显示后续查询命中并跳过重新填充。
Case 3：Bare 模式下记录相同翻译接口返回物理地址等于输入地址，仍可填充 QEMU 软件TLB，说明它同时承担客体到宿主内存的映射缓存。
Post-Condition：脚本有界退出，只清理自己启动的进程；日志来自实际断点，不以推测值代替运行数据。
Requirements：本内核根级大页会提前结束页表遍历，不能伪造三次读表；说明 TCG 内联数据快路径与 C 慢路径的区别；记录调试中真实遇到的问题。
```


## 阶段 9：最终验证、截图、报告与提交

```text
[PROMPT]
任务：按实验报告模板完成 Markdown 报告和全量复现材料，保留各阶段四段式提示词、实际修订与原始证据。
操作要求：从最终代码运行构建/测试/评分；截图实际 X 终端，不用生成图片替代运行结果；只提交实验代码与资料，不包含 QEMU 源码/构建目录。
输出要求：报告、提示词索引、迭代日志、真实截图、测试矩阵、README、可复现提交状态。

[RELY]
实验报告模板.md；当前工程代码；report/prompts/01..08；report/logs 实测结果。
固定测试原始 SHA-256 和评分注入清洁约束；当前默认 Best-Fit，Buddy 与 SLUB 独立配置。
小组成员姓名和学号已由仓库 README 确认，但 Lab2 的其他成员分工与工具/模型没有提供。

[GUARANTEE]
report/report.md、report/prompt.md、report/iterations.md、report/images、report/logs。
code/tools 中可复现的验证与调试脚本，仓库 README 的实际运行步骤。

[SPECIFICATION]
Pre-Condition：核心分配器和扩展功能已实现，QEMU 调试能获取真实数据。
Case 1：最终矩阵验证 First-Fit/Best-Fit 多 RAM、Best-Fit 6/6 与官方20/20、Buddy、两种底层SLUB；固定测试原样，结果写JSON和原始日志。
Case 2：真实终端运行最终矩阵、默认构建/运行及调试脚本，捕获并目视核对截图。
Case 3：按模板写各模块函数、逻辑、提示词与迭代；回答 First-Fit/Best-Fit 改进和内存获取思考题；解释 Buddy/SLUB 测试覆盖、QEMU 路径和原理关系。
Case 4：未知分工、工具或模型留待确认，区分实测覆盖和尚未故障注入的保护分支，不编造对话、失败轮次或生产能力。
Post-Condition：报告链接有效、图片存在、代码可构建、固定测试未改变、默认配置恢复；提交/推送后核对对应远端提交。
Requirements：维护原始 Lab2 模板和 Lab1 改动；上传内容与验证版本一致；不把20/20视为所有扩展任务也已验收。
```


## 追加阶段：First-Fit 尾插与合并回归

```text
[PROMPT]
任务：复核课程提供的 First-Fit 链表尾插，增加独立回归验证其连续页回收；发现问题后做最小修复。
操作要求：先用原实现实际复现，不修改 basic_check/default_check；修复后保留原固定测试与新回归。
输出要求：失败原始日志、修复差异、两个 RAM 配置下的通过证据。

[RELY]
default_init_memmap/default_free_pages 按地址查找插入点，在尾部 list_add 后没有 break。
list_add 的 __list_add 会同时修改 prev/next；循环继续经过新插入的节点可能把其 prev 置为自身。
公开接口 alloc_pages/free_pages/nr_free_pages；启动后初始空闲区为一个连续块。

[GUARANTEE]
void first_fit_regression_check(void);
固定测试代码原样；管理实现仅修复已证实的插入后循环问题。

[SPECIFICATION]
Pre-Condition：First-Fit 固定测试已经完成，所有页面已恢复。
Case 1：申请全部可用连续区间，清空自由链表；依次释放前两个相邻单页，申请2页必须成功返回同一基址。该场景可击穿尾插后未与前块合并的错误。
Case 2：重新释放该2页块及剩余区域，必须合并成原来的完整连续区间，能再次整块申请；最终全部恢复。
Case 3：如果原实现失败，检查尾插继续循环造成的反向链接，再在成功插入后结束循环；初始化插入的同类问题同步修复。
Post-Condition：原固定测试及新增回归均通过，空闲页计数完全恢复。
Requirements：不绕过断言，不硬编码空闲地址和页数，不扩展为无关分配策略改写。
```

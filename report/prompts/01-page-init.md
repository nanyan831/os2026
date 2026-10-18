# 阶段 1：物理内存探测与页描述符初始化

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

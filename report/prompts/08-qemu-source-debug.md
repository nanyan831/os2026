# 阶段 8：QEMU 页表与软件 TLB 源码调试

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

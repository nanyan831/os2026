# Lab5 小组分工与工作量安排

沿用 Lab3、Lab4 的三人名单和九阶段安排，按实现难度、集成验证与材料整理成本分配。每人承担三个阶段，共 9 个工作量点，占总量的三分之一。工作量点为责任分配的估算单位，不代表实际工时或课程得分。

本表确定各模块的维护和复核责任。代码与自动验证由本次 AI 协作完成，成员实际操作、交叉复核和课程验收情况按各自记录填写。

## 成员责任

| 成员 | 核心模块 | 配套工作 | 报告与材料 | 阶段与工作量 |
| --- | --- | --- | --- | --- |
| 2411070 王子卓 | 功能模块三，fork 地址空间复制与 Challenge 的 COW 页共享、写故障分裂 | 迁移 Lab2 内存管理，完善 VMA 与页表回收，检查引用计数、OOM 和 Dirty COW 类问题模型 | 整理内存复制、COW 内核设计、失败处理及内核测试章节，维护对应提示词和日志说明 | 1、4、7，共 3+4+2=9 点 |
| 2414139 刘宸旭 | 功能模块二，首个用户进程、ELF 装载和 exec | 扩展 U/S 态 Trap 恢复，完成模块一的系统调用入口与 ABI，核对首次用户态执行 | 整理 Trap、系统调用、ELF 和启动链章节，统稿报告、分工及提示词，核对归档 | 2、3、9，共 3+4+2=9 点 |
| 2413807 缪臻 | 功能模块四及模块二的等待回收，exit/wait/kill/yield 与 COW 感知的内核写回 | 适配 OpenSBI 与双版本 QEMU，完成双 GDB 调试、COW 用户隔离及进程树验证 | 整理进程生命周期、copyout、兼容性调试与 TCG 章节，完成 Challenge 状态转换和用户测试说明 | 5、6、8，共 4+3+2=9 点 |

## 阶段清单

| 阶段 | 任务 | 责任成员 | 点数 | 交付与验收材料 |
| --- | --- | --- | --- | --- |
| [1](prompt.md#阶段-1) | 前序内存管理迁移、VMA 边界与页表回收 | 王子卓 | 3 | page_init、Best-Fit、mm_map、exit_mmap/exit_range，PMM 与 VMA 自检 |
| [2](prompt.md#阶段-2) | U/S Trap、系统调用分发与用户 ABI | 刘宸旭 | 3 | trapentry.S、exception_handler、syscall.c 和用户封装，非法编号检查 |
| [3](prompt.md#阶段-3) | PCB、首个用户进程与 ELF/exec | 刘宸旭 | 4 | alloc_proc、kernel_thread、copy_thread、load_icode、exec 和 init 启动链，hello/testbss |
| [4](prompt.md#阶段-4) | fork、mm 复制和 COW 核心 | 王子卓 | 4 | do_fork、copy_mm、dup_mmap、copy_range、cow_resolve、do_pgfault，PTE/ref/TLB 核对 |
| [5](prompt.md#阶段-5) | 退出等待、孤儿收养与 COW 内核写回 | 缪臻 | 4 | do_exit/do_wait/do_kill/do_yield、copy_to_user、kmalloc_trim，exit/spin/forktest |
| [6](prompt.md#阶段-6) | 启动兼容、双版本评分与双 GDB | 缪臻 | 3 | QEMU 6.2/7.0 评分、ecall/sret 记录、TCG 源码说明和构建开关检查 |
| [7](prompt.md#阶段-7) | COW 内核边界与 Dirty COW 类问题验证 | 王子卓 | 2 | cow_test.c，OOM、跨页写回、只读保护、旧指针模拟和重复回收日志 |
| [8](prompt.md#阶段-8) | COW 用户隔离、进程树与状态设计 | 缪臻 | 2 | cowtest、forktree、waitkill、run_checks.py，Challenge 状态表和用户映像预加载说明 |
| [9](prompt.md#阶段-9) | 报告统稿、交叉复核与提交包 | 刘宸旭 | 2 | report.md、division.md、prompt.md、链接检查、源码清单和 ZIP 校验 |

## 接口衔接

阶段 2 提供 Trap 帧布局和系统调用 ABI，阶段 3 据此构造首个用户上下文，阶段 4 复制同一布局并设置子进程返回值。阶段 4 与阶段 5 共同约定 copyout 的 COW 处理顺序，阶段 5 通过 wait 回收资源，阶段 7、8 检查写入隔离和最终空闲页恢复。

Challenge 的内核页表与故障处理由王子卓负责，内核写回、进程生命周期及用户测试由缪臻负责，刘宸旭负责与首次用户态入口、系统调用接口的集成及材料统稿。各成员维护自己阶段的规格、提示词和报告段落。

## 交叉复核

| 复核人 | 复核对象 | 重点 |
| --- | --- | --- |
| 刘宸旭 | 王子卓的阶段 1、4、7 | VMA 区间、叶与非叶 PTE 权限、父子共享引用、COW 分裂与 OOM 不变量、完整页表回收 |
| 缪臻 | 刘宸旭的阶段 2、3、9 | Trap 帧偏移、SPP/SPIE/sscratch、ELF 边界和 BSS、用户指针保存、提示词及提交包一致性 |
| 王子卓 | 缪臻的阶段 5、6、8 | 僵尸态与父子链、wait 写回隔离、孤儿收养、双版本评分与双 GDB 记录、状态转换和测试覆盖 |

## 结果与证据入口

两种 QEMU 的课程评分均为 130/130，COW、forktree、waitkill 补充测试及双 GDB 采集已有运行记录。结果摘要见[实验报告](report.md#验证与证据)，专项设计见[Challenge](Challenge.md)，调试记录见[双 GDB](GDB调试.md)。成员交叉复核按上表执行后再记录确认情况。

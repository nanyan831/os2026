# Lab3 真实迭代与故障记录

日期：2026-10-27。以下区分课程骨架缺项、运行环境兼容和真实算法问题；未编造没有发生的 AI 实现错误。

## 0. 原始骨架无法完整构建

在独立 Lab3 工作树第一次构建，编译返回 2。`trap.c` 缺少处理函数，Best-Fit 缺少 `free_list/nr_free`，`page_init()` 也留空。这是课程要求填写的起点。记录见 [原始构建](logs/00-skeleton-build.log)。

迁入 Lab2 必要函数和常量、补齐 Trap 两个模块后，[首次实现构建](logs/01-implementation-build.log) 成功，无编译警告。没有通过临时空处理函数来伪造成功。

## 1. 原始 loader 方式停在 OpenSBI

普通运行原始 `-device loader,file=bin/ucore.img,addr=0x80200000`，4 秒超时，只有固件输出，没有内核 `DTB Init`。相同镜像改为 `-kernel` 后，DTB、内存自检和连续时钟都正常。

- [原始加载日志](logs/02-original-loader.log)
- [同镜像 kernel 加载对照](logs/03-kernel-firstfit-before-fix.log)
- [原始官方评分 0/100](logs/04-official-original-loader.log)

修复：普通/调试/测试目标使用 `-kernel`；评分经独立 `qemu_compat` 适配器执行真实 QEMU。只替换精确匹配的加载参数，不修改串口数据、测试断言、评分脚本或退出状态。最终成绩应表述为“在本机启动适配后通过官方测试”。

## 2. 非相邻尾插通过，相邻尾块合并失败

第一版附加回归释放两个不相邻尾块，两个 PMM 均通过；该检查不足以覆盖相邻合并。增加“清空空闲链表，依次释放相邻两页，再申请连续两页”的检查后，First-Fit 触发：

```text
kernel panic at kern/extra/test_lab3_extra.c:33:
    assertion failed: alloc_pages(2) == all + 1
```

Best-Fit 同一检查通过。失败见 [原始串口日志](logs/tail-adjacent-before-fix-extra-first_fit.log) 和 [当时矩阵](logs/verification-tail-adjacent-before-fix.json)。panic 后监视器持续输出提示符，运行器 8 秒超时终止；这不是正常关机。

原因：First-Fit 的 `default_init_memmap()`、`default_free_pages()` 在尾部插入后继续循环，可能再次处理刚插入的节点，破坏供合并使用的前驱关系。参考 Lab2 已有修复，在两个尾插分支补 `break`。原有 `basic_check/default_check` 原文不变。

最终两个 QEMU 版本的两种 PMM 均通过包括相邻尾块的回归。见 [最终 First-Fit 附加检查](logs/qemu6-extra-first_fit.log)、[最终 Best-Fit 附加检查](logs/qemu6-extra-best_fit.log)。

## 3. QEMU 7.0 暴露低地址过渡映射缺失

使用启动适配器后，QEMU 6.2 首轮 6/6 通过；QEMU 7.0 的三组仍停在 OpenSBI，得到 0/100。保留见 [修复前 QEMU 7.0 评分](logs/before-startup-fix/qemu7-official-grade.log) 和 [修复前矩阵](logs/before-startup-fix/verification-qemu7.json)。

对比当前 Lab2 启动代码发现：Lab3 根页表只有高地址索引 511，缺少 Lab2 已有的索引 2 恒等映射。写 `satp` 不会改变当前低地址 PC；`sfence.vma` 到跳入高地址之间仍需合法取指映射。

修复：保留高地址映射，增加索引 2 对 `[0x80000000, 0xc0000000)` 的恒等映射。之后两个版本重新完整运行，各 6/6 通过。原因判断依据代码差异与修复前后运行对照，没有进行本轮 QEMU 宿主源码级调试。

## 4. 最终验证与证据范围

- [QEMU 6.2 当前源码矩阵](logs/verification-qemu6.json)：6/6。
- [QEMU 7.0 当前源码矩阵](logs/verification-qemu7.json)：6/6。
- [实际 make grade](logs/make-grade-final.log)：100/100，使用透明加载适配器。
- [最终普通启动](logs/final-boot.log)：十次统计输出后 QEMU 正常退出。
- [GDB 往返记录](logs/qemu6-gdb-trap.log)：288 字节帧、原 sp、EPC 和返回中断状态满足断言。

早期日志和历史矩阵保留其当时源码指纹，不替代最终矩阵；自动通过与助教现场验收分开记录。截图中的附加检查和 GDB 页面明确显示的是已命名的原始运行日志。

## 5. 等量分工与提示词文档修订

按本轮要求重新分配最终责任：刘宸旭负责阶段3/6/9，王子卓负责阶段1/4/7，缪臻负责阶段2/5/8；三人各3阶段、9个估算工作量点，详见[分工页](division.md)。责任分配不替换原始 AI 实施与运行记录。

对照 Lab2，将原四份概括性规格细分为九阶段提示词：每份提供任务/操作/输出、依赖、接口清单，以及前后条件、边界场景和约束；[汇总页](prompt.md)同时收录完整正文，保留独立阶段文件。README、报告总表、模块责任与资料入口同步更新。

本次只修改文档，检查提示词汇总与独立正文一致、分工均衡、本地链接有效，以及源码、固定测试、截图和 Lab2 文档指纹未变。原有内核运行结果继续作为已有实测证据，本次没有重跑 QEMU 或新增成绩。

检查结果见[文档修订检查](logs/documentation-revision-check.json)与[当前文档审计日志](logs/documentation-revision-audit.log)。

## 6. 2026-11-04 再次核验与提交收尾

重新对照指导书、当前源码与交付材料，并在临时副本重新构建。QEMU 6.2和7.0各通过6/6项，启动适配后的官方评分均为100/100；源文件与官方测试指纹一致，审计17/17通过。Challenge 未实现，独立回归与 GDB 工具继续保留用于验证必做功能。

核验发现报告表格末尾多一列、文件末尾有多余空行，最新报告和提示词修改尚未进入提交包，远程也没有Lab3分支。用户随后明确要求收尾并推送，因此保留既有文档精简，修正格式，将当日复测日志纳入报告目录，再进行提交、归档和远程推送。收尾没有改变内核实现；详见[再次核验与提交收尾](recheck-20261104.md)。

# 阶段 7：简化 SLUB 独立测试提示词

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

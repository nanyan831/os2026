# 阶段 9：最终验证、截图、报告与提交

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

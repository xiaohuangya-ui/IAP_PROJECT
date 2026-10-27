# 文档从这里找

首页 [README](../README.md) 讲这个项目做了什么；这里放具体操作、实现约束和验证记录。文档没有刻意合并成一篇长文章：用工程的人更需要找到对应的说明。

| 想了解什么 | 建议先看 |
| --- | --- |
| Boot、App、W25Q64 怎么分区，升级何时允许擦写 | [设计与存储边界](DESIGN_SPEC.md) |
| 这个项目哪些地方值得细看、为什么这么处理 | [工程笔记](ENGINEERING_NOTES.md) |
| 用哪版 Keil、怎么检查路径、怎么跑不接板的测试 | [构建与离线测试](REPRODUCING.md) |
| 连接串口后怎么发升级包、怎么查询状态 | [串口操作指南](USER_GUIDE.md) |
| 现在到底验证过什么、哪些还没解决 | [验证范围与已知问题](VALIDATION.md) |
| STM32 / W25Q64 / SSD1306 的官方资料去哪里找 | [参考资料](REFERENCES.md) |

## 两个特别容易误用的地方

**不是所有测试工程都能烧。** `Tests/keil/APP_V2_HEALTH_FAIL_TEST` 用于隔离故障验证，它故意阻止健康确认，不是正式固件。正式 Target 和二进制身份以 `../Firmware/IDENTITY.json` 为准。

**离线 PASS 不是掉电 PASS。** 主机状态机故障注入、Keil Rebuild、串口日志是不同层级的证据。详细限制以 [VALIDATION.md](VALIDATION.md) 为准。对暂时没有真机数据的场景，我保留“未验证”，不把设计预期写成实验结论。

如果只是招聘或面试中想了解思路，先看 [README](../README.md) 和 [工程笔记](ENGINEERING_NOTES.md) 就够；如果真要复现，再从构建文档逐步往下看。

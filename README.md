# STM32F103 串口 IAP Bootloader｜从固件下载到异常回滚

这是我围绕 **STM32F103C8T6（64KB Flash / 20KB SRAM）** 做的一个嵌入式练习工程。它不只是“通过串口把程序烧进去”：我更想弄明白，在内存和 Flash 都很紧张的 MCU 上，**固件下载一半、升级中断、应用启动失败时，系统应该怎么办**。

项目分成两个 Keil 工程：Bootloader 裸机运行，Application 使用 FreeRTOS。固件通过 USART1 + YModem 接收，先放到 W25Q64，经过校验和备份后才写内部 App 分区；新固件启动后还要经过健康确认，不能只凭“能跳转”就算升级成功。

> **当前状态（2026-10）：** 已有正式编译结果、离线回归及部分串口真机记录；**真实掉电、完整回滚、温度精度与长稳等尚未全部通过硬件验收**。这是学习和验证用的工程，不应直接用于安全关键产品。

## 一眼看懂这个项目

| 项目 | 当前实现 |
| --- | --- |
| 主控 | STM32F103C8T6，按 **64KB Flash / 20KB SRAM** 设计，不借用“扩容”空间 |
| 开发环境 | Keil MDK 5.37、ARM Compiler 5.06 update 7、STM32CubeMX、HAL / FreeRTOS |
| 分区 | Boot 16KB：`0x08000000–0x08003FFF`；App 48KB：`0x08004000–0x0800FFFF` |
| 串口 | USART1，115200 8N1，RX/TX DMA，CRLF 命令 + 标准 YModem（不支持断点续传） |
| 外部存储 | W25Q64：下载区、旧固件备份区、双扇区元数据、升级日志及 App 温度历史区 |
| 固件检查 | 32B 自定义文件头、CRC-16/XMODEM、CRC-32/IEEE 与回读校验 |
| 启动策略 | 无升级需求时尝试正常 App；新固件 Trial 健康确认，失败按可信备份执行回滚 |
| 应用演示 | FreeRTOS 三任务、片内温度采样、SSD1306 OLED、历史记录与串口查询 |

**不包含** RSA 签名、网络 OTA、加密启动链或商用安全认证。这里的可靠性设计主要围绕串口 IAP、存储有效性、试运行确认与异常恢复。

## 系统怎么工作

```text
PC / Tera Term
    │  USART1：@APP:UPDATE / @IAP:ENTER + YModem
    ▼
Bootloader（裸机，16KB）
    ├── 接收数据 → W25Q64 Download
    ├── 固件格式、CRC 与回读验证
    ├── 旧 App → W25Q64 Backup
    ├── 擦写内部 App 分区并验证
    └── Trial 启动 → 健康确认 / 失败回滚
                          │
                          ▼
Application（FreeRTOS，48KB）
    ├── 串口命令与升级控制
    ├── ADC 片内温度采样 / OLED 显示
    └── W25Q64 独立历史区（循环记录）
```

这里有个刻意的边界：**App 不直接改写 Boot 使用的下载、备份和元数据区**。升级过程由 Boot 管理；App 只负责请求升级、业务运行和健康确认。详细地址与状态约束见 [设计说明](doc/DESIGN_SPEC.md)。

## 我重点处理了什么

**1. 16KB Bootloader 真正装得下。** 目标芯片只按标准 64KB Flash 使用，不能靠扩大 Boot 分区解决容量问题。当前正式 Boot Target 的 ROM 为 **16,372 / 16,384B**，只剩 12B。为了保证结论可核对，仓库保存了构建 Target、固件身份和离线回归入口；这也意味着后续加代码必须重新测容量，而不是随手改完就烧录。

**2. 把“收到文件”和“可以安装”分开。** YModem 收完并不等于固件有效。先验证下载镜像，再备份旧 App，只有可信镜像和元数据条件满足后才擦写内部 App；安装后还要读回检查。外部 Flash 元数据用于标记进度，防止把不完整下载误判为可启动版本。

**3. 把第一次启动看作试运行。** 新 App 需要在规定条件下报告健康，Boot 才正式确认；最多三次 Trial 尝试，失败时按可信 Backup 恢复。IWDG 与业务健康条件关联，但这不等于已经证明所有断电场景都能恢复。

**4. App 不是一个空循环。** FreeRTOS 侧分成串口通信、升级控制和业务任务；历史温度记录写入外部 Flash 的独立区域。这个业务功能也带来了新的问题：存储扫描、写入、看门狗之间不能相互干扰。

我把设计细节和一些容易踩坑的点单独放在 [工程笔记](doc/ENGINEERING_NOTES.md)，比在首页堆 API 名称更方便阅读。

## 目录：从哪里开始看

```text
IAP_PROJECT/
├── Bootloader/           # Boot 工程、HAL/CMSIS 与 IAP/存储/协议实现
├── Application/          # FreeRTOS 应用、驱动和业务任务
├── Shared/               # Boot / App 共用的 CRC、帧头、缓冲与协议代码
├── Firmware/             # 当前构建固件、身份信息和 SHA256 校验
├── Tests/
│   ├── host/             # 主机离线测试、模拟故障和协议用例
│   ├── hardware/         # 开发阶段的板端验证入口（含历史代码）
│   └── keil/             # 单独的故障测试 Target，禁止当正式升级包
├── Tools/                # 打包、项目检查、编译审核与回归脚本
└── doc/                  # 使用指南、设计说明、验证边界、工程笔记
```

> 目录保留 CubeMX / Keil 工程的原始相对路径，避免只为展示而破坏工程引用。`Tests/hardware` 中也有正式 Boot 依赖的薄入口，不能因名称像“旧测试”就删除。

## 想自己看代码或复现

1. **先看工程：** `Bootloader/MDK-ARM/Bootloader.uvprojx` 和 `Application/MDK-ARM/Application_Phase9.uvprojx`。工具链为 **ARM Compiler 5**；不要照搬 AC6 的 `-Oz` 等选项。
2. **不接板子：** 参考 [离线构建与回归](doc/REPRODUCING.md)，按说明建立 `.venv`，使用脚本运行主机用例。该脚本要求 Windows 项目内 `.venv/Scripts/python.exe`，并依赖 GCC。
3. **准备真机验证：** 请先阅读 [串口操作指南](doc/USER_GUIDE.md) 与 [已知问题](doc/VALIDATION.md)。尤其不要把实验固件直接用于已有数据的板子；第一次烧录还涉及有效 Boot/Metadata 的匹配。
4. **核对二进制身份：** `Firmware/IDENTITY.json`、`Firmware/SHA256SUMS` 和实际构建输出对应。不要把故障注入 Target 与正式 V2 混用。

串口最常用的命令：

```text
@APP:VERSION
@APP:UPDATE
@IAP:STATUS
@IAP:ENTER
@APP:STORE
@APP:HISTORY
```

命令以 **CRLF** 结束；进入升级后按 [USER_GUIDE](doc/USER_GUIDE.md) 使用 Tera Term 的 **YModem 发送**，不要用普通“发送文件”替代。没有确认板上原有 Boot 和 Metadata 的状态前，不要直接执行烧录或安装流程。

## 当前证据与没做完的事

| 项目 | 目前能说明什么 |
| --- | --- |
| Keil 正式构建 | Boot / App V1 / App V2 三个 Target 完整 Rebuild，记录均为 **0 Error / 0 Warning** |
| 固件大小 | Boot **16,372B**；App V1 **25,244B**；App V2 **25,632B**（见 `Firmware/IDENTITY.json`） |
| 主机回归 | **14 组**离线回归有 PASS 记录；模拟故障不等于真实断电 |
| 串口真机观察 | 有 YModem 完成、新版 App 正常启动和复位后历史记录恢复的用户串口记录 |
| 尚待处理 | 实际温度读数异常（约 -38～-39℃）、真实写入中掉电、独立回滚验收、最坏时序与长稳验证 |

所以这里的“完成”主要指**源码集成、构建、主机测试和部分板端现象**，不是“所有可靠性故障都过了”。完整的原始观察和限制见 [VALIDATION.md](doc/VALIDATION.md)。

## 资料与说明

- [文档导航](doc/README.md) · [工程笔记](doc/ENGINEERING_NOTES.md) · [设计规格](doc/DESIGN_SPEC.md)
- [编译与离线测试](doc/REPRODUCING.md) · [串口使用](doc/USER_GUIDE.md) · [验证与已知问题](doc/VALIDATION.md)
- [芯片及器件资料入口](doc/REFERENCES.md)（参考手册采用原厂链接，不再把大体积 PDF 复制进仓库）

本仓库中含 ST HAL/CMSIS、FreeRTOS 等第三方组件，原有版权与许可声明保留。**不要把第三方文件当成我原创的代码**；根目录目前未附加覆盖全仓库的统一开源许可，复用前请核对对应文件的授权范围。

如果你正在找一个“有升级流程，也愿意正视失败场景”的 STM32 工程，希望这份代码和测试记录能提供一点参考。

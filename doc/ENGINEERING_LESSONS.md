# STM32F103C8T6 可靠 UART IAP Bootloader｜工程经验与问题预防手册

**版本**：2.0（2026-10-12，结合已确定 `DESIGN_SPEC.md` V3.1 与 HW05C/HW05E 真机证据修订）  
**性质**：正式 Phase 0～9 的工程经验、阶段预防门禁和未关闭风险，**不替代同目录 `DESIGN_SPEC.md`，不是烧录许可或已交付产品声明**。未来长期仅维护 `DESIGN_SPEC.md`、本文件和 `AGENTS.md` 三份核心管理文档。  
**历史汇总输入范围**：此前整合的 4 份二级汇总文件（工程手册、Codex 规则、Phase 预防清单、OPEN 风险表），其中记载 5 份 LESSONS_LEARNED 来源共 1,912 行。S1～S5 与 HW05C/HW05E 内容是历史汇总的溯源记载；当前未找到全部原始附件，不重新签署其硬件 PASS。2026-10-12 实际本机审查事实见 §14 与设计书 §19。

## 0. 读者须先理解的四条规则

1. **现行 `DESIGN_SPEC.md` 优先于历史方案**。历史 C02～C07、G4-D、HW、P1、D2 等均是候选/验证版本，不会因为一次链接成功而自动成为产品设计。
2. **四种证据互不替代。** `BUILD/LINK`：Keil 目标可编译、符号进入执行区；`HOST`：PC 模型/算法测试；`BOARD-ISOLATED`：特定非破坏真机固件/路径；`PRODUCT-HW`：正式目标及规定状态/故障下真机端到端验收。一个 PASS 必须同时写等级、固件身份、测试条件和覆盖边界。
3. **从错误中形成的安全门禁可以是 MUST，未经实测的产品能力不能写成 PASS。** 例如“启动前检查 SPI SPE”是 MUST；“最新产品的 SPI 已经合格”仍是 OPEN。
4. **历史复盘是二级证据。** 本手册将历史五份来源在汇总资料中记载的日志/源码/MAP 称作“历史报告的实测证据”；本轮没有原件时，不能据此重新签署原始工程验收。

## 1. 文件来源、引用格式与证据等级

| ID | 历史汇总记载的五份原始来源（本轮未单独上传） | 行数 | 主线 |
|---|---|---:|---|
| S1 | `LESSONS_LEARNED_FEASIBILITY(1).md` | 344 | C02～C07-A 累积链接，CRC/Header、YModem、Metadata/Backup/Recovery 原型、向量差异 |
| S2 | `LESSONS_LEARNED_W25Q64_AND_CAPACITY_PREFLIGHT(2).md` | 273 | W25 驱动、O0/O3 容量、Watch/读写/擦除、危险 Flash 探针 |
| S3 | `LESSONS_LEARNED_G4D_OPT2_CAPACITY_PREFLIGHT(2).md` | 441 | G4-D OPT2 编排、ROM 变化、UART/日志/IWDG/LSI 候选、工装预置 |
| S4 | `LESSONS_LEARNED_HW03C_UNIFIED_PREFLIGHT(2).md` | 394 | PB12/BKP/RTC-LSI 安全硬件预验证、BDCR 异常、证据和下载管理 |
| S5 | `LESSONS_LEARNED_C07_PB12_W25_PREFLIGHT(2).md` | 460 | PB12 启动瞬态、P1 实机、W25 SPE 修复、诊断与产品容量隔离 |

`[S4:143-154]` 表示第四份材料对应行段；**不是直接引用原始串口文件**。五份来源分别有 SHA256：S1 `1133f11cf2f5b69c0ee81220029b04a2e5331112d6bd92d168150b38b390b23e`、S2 `158e8ae824ecadad63c4183af87fd314357d88af7b739e4194c73ee00084b57d`、S3 `0dcf5991ff4ab558ec2e54577976fc346b3ca126bb38656cfceed194e5daee45`、S4 `d7a676abdc4408bae1cc603a75119ac42e7a908c4c6d562b83f1934aac7fd9a4`、S5 `d143e9ad59f83c8a243ff8d246be2ff0bc672343c33539c9ba61cfdfd3e4586c`。不要把各文件重用的 `W01-L01` 当作全局唯一编号：必须与 S1～S5 组成复合定位。

**统一证据分类**：`H-R` = 历史复盘记载了真机/串口/调试器实测（本批无全部原始附件）；`L-R` = 历史复盘记载了 ARMCC5 Build+MAP/HEX；`S-R` = 可从复盘呈现的具体源码/接口/差异支持；`M-R` = PC mock/Clang/GCC 模拟记录或报告；`I` = 工程推论/风险分析；`OPEN` = 缺乏能够关闭问题的证据。原始工件实际取得并重算后，方可升级为 `H-VERIFIED` 或 `L-VERIFIED`。证据**数量、文档重复次数、作者用词强弱均不能提高证据等级**。

**验收结果用语**：`PASS@{BUILD|HOST|BOARD-ISOLATED|PRODUCT-HW}`、`FAIL@...`、`BLOCKED`（安全守卫正常拒绝执行）、`NOT RUN`、`OPEN`；不得用一个不带后缀的 PASS 表述跨级成功。

## 2. 已冻结的项目边界（所有历史文档均必须服从）

| 项目 | 不可修改约束 |
|---|---|
| MCU/工具链 | STM32F103C8T6 严格按 **64KiB Flash、20KiB SRAM**；Keil MDK 5.37.0.0、ARM Compiler 5.06 update 7（build 960）、STM32CubeMX + HAL，必要处使用 CMSIS/寄存器。ARMCC5 `O3` 是容量验证基线；不得混用 ARM Compiler 6 的 `-Oz/-Os`。 |
| 内部 Flash | Boot `0x08000000–0x08003FFF`（16KiB）；App `0x08004000–0x0800FFFF`（48KiB）；**无内部 Metadata 页**。 |
| 外部 W25Q64 | Download `0x000000–0x00FFFF` 64KiB；Backup `0x010000–0x01FFFF` 64KiB；Metadata A/B `0x020000/0x021000`，各 4KiB；Log `0x022000–0x025FFF` 16KiB；其余保留。 |
| UART | USART1 **115200 8N1**；DMA1 CH5 RX、CH4 TX；IDLE 检测；2KiB RingBuffer；IDLE 不等于协议帧界。 |
| 传输与固件 | ASCII `@APP/@IAP` + CRLF、**标准 YModem V1 不支持断点续传**；YModem CRC-16/XMODEM 多项式 `0x1021`、初值 `0`；自定义 32B Header + `app.bin`，magic `0x46574D47`、header_version 1、target `0x08004000`；CRC-32/IEEE reflected `0xEDB88320`，init/xor `0xFFFFFFFF`。字节序/字段偏移只允许从最终共享格式定义确认，不从早期样例另立规范。 |
| 状态/持久化 | `IDLE` 为运行态；持久 9 态为 `RECEIVING/DOWNLOADED/VERIFYING/BACKING_UP/INSTALLING/PENDING_CONFIRM/CONFIRMED/ROLLBACK/FAILED`；W25 **72B Metadata A/B 双扇区** append-only sequence+CRC；BKP DR1 升级请求、DR2 App 确认请求、DR3 Boot Trial 标志，均不替代持久 Metadata；恢复高于普通升级请求。 |
| 安全行为 | 裸机 Boot，FreeRTOS App；无恢复/更新请求时有效 App 应立即启动；新 App 需健康确认，`MAX_BOOT_ATTEMPTS=3` 后回滚；IWDG 目标约 4s，须按实际 LSI 论证；备份/回滚/掉电恢复/日志/错误码均不得因 ROM 压力删除。 |
| App 任务 | `Task_UartRecv` 高(3)、`Task_UpgradeCtrl` 中(2)、`Task_Business` 低(1)。App 为 STM32F103C8T6 内部结温估算 + PC13 已验证板载 LED + **SSD1306 128×64 I2C1 PB6/PB7、7-bit 0x3C、100kHz** 的 FreeRTOS 仪表盘；不写 W25。 |

**阶段冻结映射**：Phase 0 双工程基础启动/跳转；Phase 1 UART+Ring；Phase 2 DMA+IDLE；Phase 3 ASCII 命令；Phase 4 标准 YModem+Header；Phase 5 内部 Flash IAP MVP；Phase 6 W25 Download/Backup；Phase 7 Metadata+掉电恢复；Phase 8 确认/回滚/IWDG/Log；Phase 9 FreeRTOS App 闭环。用户生成两工程且检查点 B 审核 PASS 后，才开始 Phase 0 正式代码与构建；此前只做配置准备；在涉及相应真实硬件风险的动作前须满足对应安全门禁；绝不以候选代码充当已经完成的 Phase。

## 3. 跨窗口交叉核对：先判定事实层，再判定是否已关闭

| 聚类问题 | 资料之间的关系 | 判定及边界 |
|---|---|---|
| 16KiB 严重紧张 | S1 C06-C OPT1 16,360B（余24B）、S3 B3 OPT2 16,116B（余268B）；S4/S5 引用后续 C04-B 15,596B（余788B）、S5 P1 实链 15,756B（余628B） | **均为有条件的链接记录；不是相互驳倒，也不能混合求“当前最新占用”**。各版入口、探针、P1、SPE 修复及未保留功能不同；最新完整产品链接仍 OPEN。[S1:208-235][S3:44-55][S4:239-254][S5:278-310] |
| PB12 “按住读高”与“松开误判按下” | S4 稳态 K 样本先有异常，后 3+3 次成功；S5 真 Boot 初次单次读错误，P1 在特定路径成功 | **不同采样时点，并不冲突**。电气根因仍 OPEN；P1 是候选安全决策，不是全部启动条件通过。[S4:39-50][S5:44-64,84-121] |
| W25 初始化能力 | S2 能读写且断电保留（限定场景）；S4 曾发现 SPI CR1.SPE=0；S5 D2 真机失败，增加 `__HAL_SPI_ENABLE` 后通过 | 早期测试工程的成功不能继承到另一产品调用路径；**SPE 修复有窄范围实测**，最新完整 BootMain 的同版复测仍 OPEN。[S2:11-22,73-95][S4:52-63][S5:123-147,393-401] |
| DMA/IDLE 功能 | S2 在本窗口 NOT RUN；S4 引用 HW-02 已有真机 PASS；S3 主机模型通过；S5 当前窗口没有完整 HW-02 原始记录 | **证据覆盖不同窗口/路径**；历史隔离 HW-02 结果可列 `H-R`，但不能直接替代正式 YModem/产品主线验收。[S2:133-143][S3:166-179][S4:11-22][S5:324-330] |
| LSI、RTC/IWDG | S3 提供RTC/LSI测频模型；S4 V3 静态修改 RSF 时序但 F 从未真机执行；观察到 RTCSEL 异常 | **后期真机数据否定了“已经完成测频”的过度认知**，但不能认定哪个程序修改了 BDCR；IWDG 的 W BLOCKED 不算约4s PASS。[S3:196-225][S4:117-154,182-193] |
| MAP 中函数名的含义 | S1/S2 说明源码编译不等于保留；S3 log read 曾 removed；S4/S5 明确 Cross References 有名字也可能最终被 removed | 后期纠正了早期的“grep 命中=已链接”捷径；**以最终执行区、removed/retained、实际入口可达性共同裁定**。[S1:74-82][S3:76-119][S4:91-102][S5:149-160] |
| RTC/BKP 不可擅自修复 | S3 LSI 模型不做 BDRST；S4 真实 BDCR 0→0x8200 且无法归因 | 设计防线成立，现场状态异常仍 OPEN；**安全阻断成功不代表问题已解决**。[S3:196-209][S4:143-154] |

详细冲突与证据边界见本文 §8～§11；**不再要求长期保留独立的 OPEN_RISKS 文件**。

## 4. 十三类正式工程经验

每节采用“原则 / 案例与根因 / 解决经过 / 代码边界 / Codex 检查 / 离线与真机验收 / 禁止事项 / 证据”结构。`MUST` 表示工程或冻结安全规则；`OPEN` 表示能力与根因尚无足够证据。

### 4.1 Keil、ARM Compiler 5、O3、链接与 Warning

**原则（MUST）**：使用 AC5 build 960、O3、One ELF Section per Function；始终 `Rebuild All` 而非仅用增量零告警；同次解析 Build、MAP `LR_IROM1`、保留与删除的 Sections、可达入口。禁止使用 AC6 专属参数，禁止以其他编译器 `.o` 尺寸宣告省字节。

**典型问题及修复**：S3 B1 `hdma_usart1_rx/tx` 漏声明，触发 `#20 identifier undefined`，FIX1 增加 `extern`（不定义第二套 DMA 句柄）后链接通过；S2 完整构建有 `#1-D` 文件末尾缺换行，随后的零秒增量 `0 Warning` 不代表问题已修；S3 B3 OPT1 Clang 预测 −151B，但 ARMCC5 最终仅 −24B，且解析函数局部 Code 反增 6B；S1 C07-A `L6407E 0x41c` 为未安置段合计 **1052B**，不是精确超限值。S4/S5 指出仅 grep Cross References 会误判危险函数是否保留。[S1:40-82][S2:37-71][S3:61-135][S4:91-102]

**正确结构**：CubeMX 生成句柄只定义一次；项目 `.uvprojx`、scatter/Target、编译参数和源码组可追溯；构建 manifest 包含 commit/source hashes、AXF/MAP/HEX hashes、目标入口、保留函数清单和 Warning 清单。`C04-B` 的 `main→G4D2B_BootMain` 链接成立只证明链接，**实验 HEX 未获运行授权**。

**Codex 自动检查**：全量构建若有 Error、未评审的新 Warning、ROM越界、IROM 起止不匹配、缺必需符号、意外 retained `LinkOnly`/`size_probe`、MAP/HEX 身份错配即 FAIL；依赖 `Removing...`、执行区和跨节调用链，不接受全文 grep 作为唯一证据。**真机**：单独记录正确固件身份、下载范围、运行状态。**禁止**：拉大 IROM1；改用 AC6 优化开关；用 0 秒 Build 关闭 Warning；把一份 AXF 与上一版 MAP 配对。

**证据**：L-R（构建问题与增量）；产品启动 OPEN。

### 4.2 Flash 16KiB 优化与 SRAM 管理

**原则（MUST）**：ROM=`Code+RO+RW`，以目标 MAP 的总 ROM、LR_IROM1 装载限制和 HEX 地址独立交叉核对；静态 SRAM=`RW+ZI`，另检查运行栈、ISR 栈、DMA buffer、最坏临时使用。16,384B/20,480B 为物理硬限制；“能链接”不等于“尚有合理集成余量”。

**问题与修复**：S1 C06-C 原版 Load 16,396B 失败，改为对连续内部 App 直接调用已有 CRC32 后变为 16,360B，净减36B；S3 B3 等价重构先省24B、再省68B，最终16,116B；但 S3 OPT1→OPT2-A 缩小356B源于代码/探针移除的**净变化**，不可命名为等价优化；S5 D2 修复 SPI 后小诊断固件比原版增12B，不能据此计算完整产品修复成本。S2 W25 测试自带1024B静态读缓存，曾导致与产品代码无法直接比较的 ZI 差额。[S1:51-60,84-92][S2:121-131][S3:136-149,279-302][S5:276-310]

**正确结构**：每个改动提交 before/after `code, RO, RW, ZI, LR, removed sections, retained functions, feature coverage`；优化先做 host 差分与异常回归，再用 AC5 实链重新测。16KiB 余量不能设未经冻结批准的新硬门槛（如“必须剩1024B”），但余量极小时必须显式登记风险；静态 SRAM 6392B 不等于栈高水位安全。

**自动检查**：CI 比对产品真入口及 Test target 完整性；抓出超过 IROM/IRAM 及缺失 W25/元数据/备份/回滚/IWDG 的改动。**真机**：最终 O3 路径、栈填充/高水位、异常和中断压力。**禁止**：扩大 MCU 容量、消除 Backup/Rollback/IWDG/Log/CRC 来腾字节、用诊断 6,072B 代表产品 ROM。

**证据**：L-R 优化数字；最终完全产品容量 OPEN。

### 4.3 CubeMX、HAL、CMSIS 与寄存器混合开发

**原则（MUST）**：每个资源只由一个 BSP 初始化/关闭所有者负责，明确 HAL handle 与硬件状态之间的同步不变量。CubeMX 生成配置必须结合寄存器读回；自定义寄存器路径要列出写入副作用、清标志规则与恢复路径。

**案例**：S4 HW-01 `SPI1->CR1=0x0317`（SPE=0），使能后 `0x0357`；S5 D2 `W25 result=5`，经在 Init 明确使能 SPI 后 `result=0`。原始固定 `G4D_SPI1_InitFixed` 及测试 `main` 的临时补偿不能保证所有产品入口同样正确。S4 V2 RTC 使用 RTOFF 而忽略先同步 RSF，V3 加同步/超时，仅是**源码修正**，非运行闭环。S1 C07-A 引入 HAL UART/SPI DeInit 可能增加依赖，删除它们的安全等价性未证明。[S4:52-76,130-141][S5:123-134][S1:62-72]

**正确结构**：`User/BSP/spi_bus` 管 SPE、Mode/CS、超时与 HAL state；`backup_domain` 管 PWREN/BKPEN/DBP；`lsi_clock` 管 RTCSEL/RSF/RTOFF；`jump_platform` 管中断、VTOR、MSP。`Core/Src/*` CubeMX 区与 `USER CODE`/自有模块分界明确。**Codex 检查**：Source Group 唯一源文件、句柄唯一定义、配置 hash、寄存器读写所有者、HAL/硬件状态匹配测试；时钟与 ISR 禁止无依据覆盖。**真机**：SPE/CS/SR、UART DMA、BKP权限、RTC同步逐项验证。**禁止**：默认 HAL Handle READY 就代表 SPE=1；基于容量直接删除 DeInit；随手 BDRST；修改 W1C 寄存器而忽略副作用。

**证据**：H-R 窄范围 SPI；RTC 修改 S-R、硬件 OPEN。

### 4.4 代码组织、驱动分层、接口设计与测试隔离

**原则（MUST）**：薄 `main.c`，状态机决策与有副作用的持久化操作分开；产品、主机 mock、硬件诊断、Link-only 容量目标隔离。S4 V1/V3 诊断 `main.c` 约21,788/31,488源码字节，是可见的实验代码集中现象；不能夸大为已证实“正式 main 有1000行”故障。[S4:208-225][S1:182-206]

**推荐映射（非宣称历史工程已完成迁移）**：`Core/{Src,Inc}` 启动与 CubeMX；`User/BSP` GPIO/USART/DMA/SPI/BKP/LSI/IWDG/Jump 平台适配；既有 `User/Drivers` 可保留 W25 驱动，不擅改此前正式目录约定；`User/Protocol` ASCII/YModem/CRC接收；`User/IAP` Metadata、Download、Backup、Flash install、Recovery、Trial/Rollback、Log；`Shared` 32B Header/CRC/Boot-App格式；`Tests/{host,hardware,preflight}` 和 `factory/`（仅工装）。一个硬件资源只有一个 owner，一处定义，一组公开契约。

**典型错误**：老 `main(3).c` 曾重复初始化 W25 并与 UART Probe Poll 共存；S3 B1 补丁漏旧版本已修好的 extern；S5 T1/T2/T3/D1/D2 多工程克隆引起产物混用。**修复**：薄入口、基于 Git+构建 manifest 的关键基线、按差异补丁而不是无限复制工程。**自动检查**：发布组拒绝 `*test*`, `*probe*`, `LinkOnly`, Watch 指令、危险 gate、重复初始化；对 `Decide/Verify` 追踪是否暗藏介质写。**真机**：对照与产品入口一致的初始化次序，不能继承省略关键外设的测试结果。**禁止**：把 Host 模拟、自动测试 TX、工装预置路径放进正式启动程序。

**证据**：S-R/L-R；正式产品结构迁移 OPEN。

### 4.5 UART DMA、IDLE、RingBuffer 与协议状态机

**原则（MUST）**：DMA 提供字节流，`HT/TC/IDLE` 仅是搬运/通知事件而不是协议帧终止；RX 只能有一个前台 Ring 消费者，IDLE 边界不能替代标准 YModem 长度/块号/CRC。2KiB Ring，USART1 RX CH5、TX CH4 不得变动。

**案例**：S1 C03 512B DMA+2048B Ring，`RxEventCallback(Size)` 表示 DMA 当前位置，跨回绕依据此前位置消费；老 `Poll()` 与新增 `PopByte()` 若同时启动，会双消费；S3 B1 去除了自动 `C03\r\n` 以免污染 Sender 会话；B2 使用静态 `s_log_reply[66]` 防止 TX DMA 指向已返回栈帧，并直到 USART TC 保留缓冲；DMA extern 漏声明已修复。[S1:94-103][S3:61-74,151-179]

**正确结构**：IRQ/DMA 事件仅更新写位置与错误锁存；前台唯一消费者处理 ASCII/二进制模式互斥、CRC、超时及重传；TX owner 管 DMA→USART TC 完成前的生命周期和错误传播；Overflow/ORE/TE 明确 fail-closed、重新同步/复位策略。

**自动检查**：扫描所有 Ring tail 修改点；构造 HT/TC/IDLE 交错、DMA一整周超越、512B回绕、2048B溢出、并发命令/YModem、TX TC 超时、Rx ORE/TE、CRLF 拆包/粘包用例。**真机**：115200 8N1 长时间洪泛、回绕、丢字节重传、故障锁停和 NRST 后恢复。**禁止**：ISR内阻塞读取外部 NOR、两个消费者、用 IDLE 作为 YModem 帧边界、DMA 发送栈上短生命周期数据。

**证据**：S-R/M-R，历史 HW02 隔离 H-R 被其他来源沿用；最终产品流 OPEN。

### 4.6 YModem、32B Header 与 CRC

**原则（MUST）**：标准 YModem V1，不添加续传私有扩展；CRC-16/XMODEM poly 0x1021 init 0；Header 32B 并与 App/工具共享唯一布局；固件 CRC32/IEEE reflected poly 0xEDB88320、init/xor 0xFFFFFFFF。协议验收必须覆盖 Block0 文件信息、128B/1KiB包、块号互补、重复包、错误包、重传、EOT 和实际总长度。

**案例**：S1 C04 表查找导致 RO 从312B增至1848B（其中 CRC16 512B、CRC32 1024B 与增长规模吻合），只证明容量，不证明所有输入向量；S1 C06C 恢复重新核验无法凭 Header 重建未持久保存的 YModem Block0 历史声明长度；S3 采用更小的长度解析候选实链时总 ROM 净减68B的一部分，仍需差分验证。[S1:23-34,149-158,225-227][S3:289-301]

**正确结构**：`Shared/firmware_header` 唯一编码/解码和静态大小断言；`Protocol/ymodem` 只负责传输完整性；`IAP` 才授予 Download 有效、进入持久状态；`RECEIVING→DOWNLOADED` 只有在已可信验证 Block0 与传输完整性后提交；掉电后无法证明的事实一律不得由头字段“补想”。

**自动检查**：已知向量/独立算法基准、字节序/长度边界、Header 被截断/溢出、CRC误码、重包、Block0 与 Header 长度不一致及掉电状态模型。**真机**：与标准发送软件互操作，完整握手、重传/超时及非整块尾包测试。**禁止**：把 CRC 初值写成 0xFFFF、混用 CRC16 变体；假装 V1 可以断点续传；仅凭 EOT/32B Header 标记镜像可用。

**证据**：L-R、部分 M-R；标准 Sender 硬件互操作与跨复位可信信息 OPEN。

### 4.7 W25Q64、SPI、片选、擦写与异常恢复

**原则（MUST）**：底层驱动只对器件边界、页拆分、CS 事务、WIP/WEL、超时、HAL错误负责；**IAP 上层**控制逻辑分区白名单和擦写顺序。Read 命令+数据阶段 CS 需保持有效，PageProgram 不跨256B页，错误后 CS 必须释放。禁止 ISR 里阻塞 NOR 操作。

**案例**：S2 JEDEC `EF4017`、已知数据写读和完全断电后回读在指定场景通过，但64KiB BlockErase 全区 65536B=`0xFF` 尚无闭环；Watch `0xE3 LOCKED`、`0xE4 BAD_ADDRESS` 可能是测试门禁而非驱动异常；非法地址 `0x800000`、NULL、零长、页末 `0xFF` 写2B 的部分负例有记录。S4/S5 SPE=0 导致初始化失败，已在隔离路径修复；这和 S2 早期测试路径成功**并不冲突**。[S2:7-34,73-119][S4:52-63][S5:123-147]

**正确结构**：`BSP SPI` 拥有 SPE/CS，`Drivers/w25q64` 提供 Read/PageProgram/Write/SectorErase/BlockErase64K/WaitReady+读回错误；`IAP storage_map` 按 Download/Backup/Meta A/B/Log 逐范围授权，防整数地址加法溢出（优先 `len <= limit-start`）。WIP 轮询应有在实际 tick/IRQ/IWDG 条件下成立的有限期限，超时不自动标记成功。

**自动检查**：参数恶意用例、跨页/跨512B read、CS 在同一 SPI 命令低保持、错误退出拉高CS、全区64KiB读回、驱动/测试器两级结果、读写后 CRC、掉电注入。**真机**：先 JEDEC/SR/READ，再在**经批准的 Download 测试区**做写擦/完整复验；Backup/Metadata/Log 不属于默认实验许可。**禁止**：看到驱动 OK 就宣布全区擦除通过；为了试容量使用可 Watch 打开的 W25/内部 Flash 擦写门；未校验镜像就清除旧 Backup/Metadata。

**证据**：H-R（局部读写、持久）、S-R（驱动），BlockErase/异常恢复 OPEN。

### 4.8 内部 Flash IAP、地址保护、镜像校验

**原则（MUST）**：只有 App `0x08004000–0x0800FFFF` 可以成为 IAP 擦写目标，`0x08000000–0x08003FFF` 永远排除；长度不得超过48KiB，校验 Header/CRC32/向量后才可安装；危险地址判断必须能抗加法溢出、页对齐错误和未擦先写。

**案例**：S2 曾提出以 `volatile gate=0xC0DEF00D` 启动 `HAL_FLASHEx_Erase/Program` 的离线容量探针，虽计划不烧录但存在调试 Watch 可触发的真实擦写路径；**没有真正 C02 目标 Build/硬件结果**。S1 C06-C 对连续 App 区直接 CRC 获得有证据的36B节省，前提是地址/长度确认可信。S5 D2 HEX 向量检查属于诊断镜像静态属性，不能用它证明 `0x08004000` App 跳转或安装成功。[S2:145-155][S1:51-60][S5:290-294]

**正确结构**：`IAP/flash_install` 只接受经统一 `ImageValidate` 和权限验证的目标；**在Phase6～8功能集成后的正式产品升级路径中**，写前已有可信Backup与持久`INSTALLING`；写块→读回→CRC32全镜像→`PENDING_CONFIRM`，复位后由状态机接管。**Phase5内部Flash MVP**仅在有人工备份/可恢复路径的隔离试验App区内验证擦写，不得提前宣称自动Backup或持久恢复已经实现。所有Flash API可在**不可烧录的Link-only目标**做静态容量探测。

**自动检查**：边界 `0x08003FFF`, `0x08004000`, `0x0800FFFF`, `0x08010000`、溢出、零长、非对齐、partial install、错误序列；发布 HEX Boot-only 区间，App HEX 单独范围。**真机**：取得 App 区备份和数据指纹、控制下载器擦除扇区、分阶段可恢复安装/断电/回读。**禁止**：Mass Erase、修改 Option Bytes、在未获授权的预验证固件上真实擦内部 App、任何 Boot 分区写擦。

**证据**：S-R，正式 IAP 断电安装 OPEN。

### 4.9 Metadata、状态机、持久化与掉电恢复

**原则（MUST）**：W25 A/B append-only sequence+CRC 是唯一恢复可信状态源；每项破坏动作前先提交并回读状态，失败时 fail-closed；不能凭 App 向量或空白 NOR 自动建立 CONFIRMED。**历史 C06 的 72B 记录曾是候选，但当前 `DESIGN_SPEC.md` V3.1 已明确固定新正式产品的 72B（18×u32LE）、字段/状态 ABI、68B 业务字节+末尾 CRC、每扇区 56 槽**；仍然没有因此获得 NOR 掉电原子性实测。旧持久数据需受控迁移/初始化，不得混读。[S1:116-158][S3:226-239]

**案例**：S1 先写68B再写末尾4B CRC 的 Journal、两扇区56槽，是一套可审查的软件提交候选，**不是 NOR 4B 硬件原子性证明**；`C06C_RecoveryCommit()` 表面叫 Commit，实际可写页/擦 Metadata 扇区；S3 工装 `factory/provision_meta.py` 能生成候选首启状态，但没有实物安全制造验收；S1 收件 Block0 历史长度没完全持久化。

**正确结构**：`MetaRead/Scan` 只读；`MetaCommit` 明确有擦写副作用；`RecoveryDecide` 纯判定；`RecoveryCommit` 持久写（不会偷偷执行 App 安装）；状态机中每次危险 IO 前先 `commit + read-back + fsync 类等效介质确认`（MCU/NOR 实际采用 WIP结束与CRC回读，不照搬文件系统 fsync）；分叉/损坏/新提交但旧扇区清除失败，应重新扫描而非盲重试。

**自动检查**：单次 Page Program 每边界掉电、扇区轮转、dirty-slot、seq wrap、双副本冲突、介质回读错、日志写失败、无记录制造条件；对每状态列出复位后可可信信息。**真机**：受控 W25 每状态掉电/冷启验证，旧镜像和可信元数据仍可定位。**禁止**：内部 Metadata 页；无有效记录自动 CONFIRMED；先擦旧记录再提交新记录；把日志视为状态持久化；以理论后写CRC声称原子性验收。

**证据**：S-R、M-R；真机写裂与制造流程 OPEN。

### 4.10 BKP、RTC/LSI、IWDG 和复位安全

**原则（MUST）**：BKP_DR1/DR2/DR3 仅是升级请求/试运行确认请求/Trial 标志，不代替永久状态；BKP访问先满足 PWR/BKP 时钟与写保护条件，任何 RTC/BDCR/DBP 写入必须单一所有者、前后快照和保守恢复。IWDG 的使能时点、实际 LSI 频率、喂狗主体、长操作期限与跨 App 交接必须可证明。

**案例**：S4 HW-00 BKPEN/PWREN=0 时读零不能证明 DRx 为零；HW03-B 临时开时钟后DR1/DR3稳定读零只是**只读**。2026-10-10 19:34 `BDCR=0`，2026-10-11 09:16 读到 `0x8200`（RTCSEL=LSI/RTCEN=1），写入主体/时点 UNKNOWN；V3 从未完成F三轮500ms测频，`P armed=0` 不能反证此前没有别的写。S3 IWDG Feed 40B Section 曾被 removed，`LongOpSafe` 不能替代真实健康监督；S4 `W BLOCKED` 不是4s实测。[S3:196-225][S4:104-154,182-193]

**正确结构**：`backup_domain` 保存、只修改、恢复原受控位；`lsi_clock` 先 `StatusOnly`，真要配置RTC需明确授权并做 RSF/RTOFF/CNF/PRL 同步与超时，尊重RTCSEL不可逆影响；`iwdg` 明确 Option Bytes、独立校准、健康驱动喂狗和 Trial 计数先后。不存在可靠时基/不满足前提时保持恢复，不得偷用40kHz标称值或扣试运行次数。

**自动检查**：全代码搜索 `BDRST, RTCSEL, RTCEN, DBP, BKP->DR*, IWDG->KR` 写入者及 RMW；主机状态模型与寄存器写副作用清单；检验“UART故障但CONFIRMED App有效”不被不相关维护门禁锁死。**真机**：调查现有 BDCR 异常、条件许可后测 LSIRDY 与实际复位间隔、保存BKP值、验证App接力和多复位来源。**禁止**：在未解释 `0x8200` 时 BDRST；随意清 RCC 复位标志；IWDG 已在跑还无期限擦写；把只读测试误叫断电保持/写入 PASS。

**证据**：H-R（BKP只读、BDCR异常）、M-R（LSI模型）；产品校准/IWDG OPEN。

### 4.11 Boot→App 跳转、VTOR、MSP、NVIC 与健康确认

**原则（MUST）**：Recovery 优先、正常无请求时有效已确认 App 应尽快启动；进入 Jump 前统一校验镜像 CRC、Metadata、向量区间/对齐、Thumb PC；结束 DMA/TX并确保USART TC、SPI非忙、清 SysTick/NVIC中断与挂起，设置VTOR/MSP并用 ARMCC5 正确汇编终端交接。跨 Boot/App 共用一套 `AppVectorValidate`。

**案例**：S1 `C06C_ValidVectors` MSP 4B 对齐，而 C07A 验证8B并附加最低边界，确有差异且尚未统一；S5 T1/T2 证明 PB12 第一次读取可错判，P1 候选两轮16点纯HIGH才放行；P1-D1独立 POST-GPIO 真机多条件 PASS，P1-D2 在包含 W25 初始化后仅修复后的松开+RESET单次记录；`main→BootMain` 容量成功与 `J BLOCKED` 均**没有证明**真实 App 跳转。[S1:160-180][S4:18-20][S5:44-76,214-225]

**正确结构**：`BootDecision` 可读无危险副作用，`JumpPrepare` 做只读校验与外设状态检查，`JumpCommit` 作为不可返回终端函数，不允许已关外设后普通返回 Boot；失败转受控恢复；**App 端只在业务健康达标后写 BKP_DR2 并软件复位；Boot 端读取请求、重新核对 Pending 与真实 Flash CRC 后才能提交 W25 Metadata 的 CONFIRMED**。这条现行设计更正了早期“App 直接修改 Metadata”的错误表述。

**自动检查**：统一向量测试：MSP 4/8对齐争议、SRAM 上/下界含 `0x20005000` 是否允许、PC Thumb/范围、非法向量/CRC、待发TX、DMA、SysTick/NVIC状态。**真机**：独立 `0x08004000` App、Reset/COLD键态、启动可见回执、故障后可SWD恢复；产品路径测试后再评估Trial/回滚。**禁止**：用测试 HEX 中 Boot 向量证明 App 有效；没有App镜像就强跳；未清DMA/NVIC换 MSP；将 C04-B 产品容量链接成功视为烧录许可证。

**证据**：S-R、P1 H-R 限定场景；真正 C07 Jump与健康确认 OPEN。

### 4.12 Keil/ST-Link 真机调试、安全实验和验收证据

**原则（MUST）**：每次硬件测试先证明固件“是谁”以及“会接触哪里”。保留构建清单、源码哈希、MAP/AXF/HEX完整校验、Flash地址范围、Keil选项、实际Load次数、复位方式、外设供电条件和原始串口/Watch快照。证据语义先校验，再算SHA归档。

**案例**：S4 同名输出把旧V1 8772B的静态检查误用来说明 V2 10992B，后以绝对路径/重建/哈希纠正；早期 COM4 不存在导致日志含 traceback 但照样有SHA，被纠正为先判断退出码和 PASS 字段再封存；S5 D1 实际连续四次 Keil Load，不能称“一次”；S2 Watch 初值 `EE` 测试须检查 request已消费、count与数据更新，单张截图可能是旧事务。HW04/05 `BLOCKED` 只证明保护。[S2:61-95][S4:78-90,156-205][S5:175-212]

**正确结构**：测试 runner 输出 `BEGIN→RUN→RESULT→VALIDATED→ARCHIVED`，每轮 seq/result/error/指纹与构建ID；独立 Board vs Product target；Download只用 `Erase Sectors+Program+Verify`，测试前有内部Flash/W25必要备份及恢复方案。

**自动检查**：HEX checksum/范围/重叠、目标地址白名单、危险函数 retained黑名单、失败快照绝不命名 VALID、串口枚举与固件H握手；失败烧录不自动重试。**真机**：NRST、断ST-Link/USB-TTL完全冷上电、按钮按下/松开、断电前后数据对照，验证回灌路径。**禁止**：Mass Erase、Option Bytes无授权变更、Debugger Watch打开危险gate、把 Verify OK 等同整个器件/RTC没有受到影响。

**证据**：H-R/L-R 限定范围；全产品试验仍 OPEN。

### 4.13 Codex 辅助开发、审查效率和防 AI 错误

**原则（MUST）**：Codex 的每条“已解决”“节省N字节”“硬件PASS”必须绑定可访问工件；缺输入或工具/硬件执行记录时必须报告 OPEN。AI不能猜测对齐规范、未提供的代码行、最新HEX产物、串口结果或未测失败原因。

**已发生的误判类型**：把 `L6407E 0x41c` 当超限值（S1）；把 Clang节省值当 AC5 收益（S3）；把 MAP XREF中的名称当最终执行区（S4/S5）；把 W25 init 修复前后单函数说为136→148B，但MAP实为128→138B、总体 Code+12B（S5）；将 HW03C 的当前 `armed=0` 推断为历史未写RTC，或把自带SHA的失败日志当有效BKP快照（S4）。[S1:247-260][S3:334-347][S4:286-302][S5:290-294,350-367]

**正确工作模式**：每次只变更一个主题，先读冻结规格/已有接口/构建工件，输出 `CHANGE PLAN + TARGETS + SAFETY DIFF + TEST MATRIX`，再生成最小补丁；要求编译和离线差分后才建议有限硬件实验。验证报告列 `SOURCE / OBSERVED / INFERRED / NOT_RUN` 并附真实命令/日志位置；一旦代码或工件不具可验证性，Codex必须降低结论等级。

**自动检查**：规则见 `AGENTS.md`；每次 PR 强制 `RULE_CHECKLIST`、Feature-Coverage和Evidence表；审查只要发现改变冻结分区/CRC/Phase顺序或跳过危险门禁即停止。**真机**：人工明确批准单个测试目标与允许的物理副作用，不得由 AI “为了看看”直接建议运行含危险BootMain的Link-only HEX。**禁止**：虚构工具输出；保留多版旧工件却不绑定哈希；隐瞒未完成功能；无规格授权移除安全路径。

**证据**：跨五份材料重复出现的具体误判及纠正；该经验的普适性来自可复现失败机制，不来自重复提及次数。

## 5. 可复用候选与复用条件（不要复制即使用）

| 历史候选/方法 | 可复用内容 | 必须先做的验证 | 禁止直接迁移内容 |
|---|---|---|---|
| C04 CRC/Header、C05 YModem | CRC16/CRC32候选、32B Header、帧状态和块序号 | 共享字段偏移、CRC独立向量、标准Sender和错误帧 | 旧格式假定、错误CRC16变体、V1续传 |
| `UARTSizeProbe` / G4D UART | 512B DMA位置消费、2KiB ring、TX TC、错误锁存 | 唯一消费者和真机HT/TC/IDLE/ORE/TE/溢出 | `C03\r\n` 自动输出、探针 Poll |
| `w25q64(8).c` 等 | CS、页拆分、范围保护、SPE修复与结果码 | 源码/硬件路径一致性、64KiB擦除+全区读回、异常和WIP | 允许Watch擦写的测试控制器、未经授予的分区写入 |
| C06 Metadata/Backup/Recovery | 72B journal候选、状态先提交、双CRC、Decide/Commit分层 | 格式冻结前审查、写裂/轮转/首次制造/掉电真机 | 假定末尾4B硬件原子性、无效记录自动CONFIRMED |
| G4D2 产品编排、B3 OPT1/2 | 长操作门禁、命令解析、日志读取链、经差分的优化候选 | 真实主入口覆盖、IWDG监督、功能回归、目标重链 | `G4D2_LinkOnly` 当生产入口、删除Feed仅省字节 |
| P1 PB12 sampler | 多轮、配置检查、失败保持恢复策略 | 全产品入口和所有复位键态、时序校准 | 用固定NOP循环宣称绝对毫秒去抖 |
| HW03C P/F/Q | 安全授权与只读状态/危险操作分开、快照有效性门禁 | RTC所有权/RSF-RTOFF/BDCR异常解决并重测 | 直接在产品中执行未经验证的F/BDRST |
| `factory/provision_meta.py` | 制造侧生成 Metadata 的架构思路 | 字段版控、授权、真实回读、重复预置拒绝 | 放进Boot、空NOR自行建立CONFIRMED |

## 6. 统一代码接口约束示例（**示意，不是现成产品代码**）

```c
/* Shared: 最终字段布局必须由冻结格式的同一头文件规定 */
typedef enum {
    IAP_OK = 0,
    IAP_ERR_RANGE,
    IAP_ERR_IO,
    IAP_ERR_CRC,
    IAP_ERR_UNTRUSTED
} IAP_Status;

/* BSP: 不改变持久介质。所有者提供明确的只读/写入契约。 */
IAP_Status BSP_W25_Read(uint32_t addr, void *dst, uint32_t len);
IAP_Status BSP_BKP_ReadRequest(uint16_t *value);

/* IAP: 可判定/可提交必须严格分开；Commit 有NOR写/擦副作用。 */
IAP_Status IAP_RecoveryDecide(/* const snapshot, */ /* decision out */);
IAP_Status IAP_RecoveryCommit(/* expected sequence, next state */);

/* Boot: 统一向量验证，不得在Recovery/Header/Jump各复制一份。 */
IAP_Status Boot_AppVectorValidate(uint32_t app_base, uint32_t app_size);
```

禁止以本示例里的参数/枚举覆盖正式规格；真正实现须给出完整可编译原型、单一错误码字典、精确读写范围、上下文/副作用、并发所有权与头文件依赖。纯决策函数可以 host mock；有副作用函数必须经区间检查、状态持久化和独立真机门禁。

## 7. 持续检查的证据模板

每个变更/测试提交一段记录，不填完不得将 OPEN 改为 PASS：

```text
Test ID / Phase / Requirement-ID:
Build ID / Git Commit / Source SHA256 / Compiler + Flags:
Target: [HOST | LINK_ONLY | BOARD_ISOLATED | PRODUCT]
Artifacts: full rebuild log, map, axf/hex sha256, source diff, test log
Final MAP: LR_IROM1, Code/RO/RW/ZI, removed/retained/reachable
Safety authorization: device, allowed addresses, prior backup, irreversible side effects
Procedure + raw observed result + expected result:
Evidence level: [HOST | BUILD | BOARD_ISOLATED | PRODUCT_HW]
Result: [PASS | FAIL | BLOCKED | NOT_RUN | OPEN]
Limits / remaining OPEN / failure recovery / reviewer:
```

**当前产品状态**：历史五份复盘的交叉整合已记录；随后 HW05C/HW05E 隔离 CPU→App 真机交接成功，**产品** C07、安全启动、LSI/IWDG、Journal 掉电恢复、SSD1306 FreeRTOS 全链路仍未验收。本文后续整合了原压缩包中的阶段门禁和风险表；不再要求保留另外三份独立规范。


---

## 8. 后期补充：HW05C / HW05E 真机交接与调试器经验（2026-10-11）

> 这一节取自**历史汇总所述来源对话中的用户串口输出、Keil 截图和脚本执行结果**；与 S1～S5 的历史复盘来源不同。可认定对应**隔离硬件路径**的事实，但不能认定完整正式 Boot 产品路径。

### JUMP-HW-01：最小 App 的受控烧写与写保护证据

- HW05C v3 测试 App 只占 `0x08004000..0x080044BB`、1212B；App 第一个 MSP 为 `0x20000698`，Thumb Reset 向量为 `0x0800416D`，链接基址 `0x08004000`。这是测试 App 而非最终 FreeRTOS 仪表盘。
- 用户在 STM32CubeProgrammer 首先备份完整 65536B 内部 Flash，旧 Boot MSP `0x20001260`、Reset PC（带 Thumb 位）`0x08000189`。App Pages16/17 烧写完成后，离线比对 `PASS app_image_matches_expected`、`PASS boot_16KiB_unchanged`、`PASS flash_outside_pages16_17_unchanged`、`PASS erased_tail_in_page17`。
- **经验**：编程工具报告 Verify OK 不是唯一保护证据；必须比较**全片烧前/烧后**，证明未触碰 Boot 及 App 目标页以外范围。真实 App 的代码区不等于 Boot 代码区；不得用未知 HEX 的默认下载设置。

### JUMP-HW-02：Reset Vector Catch 必须凭独立调试证据核验

- ST-Link Under Reset、HW RESET、Stop after Reset 组合下，Keil 曾先报告异常的 `PC=0x08001466/SP=0`，随后真正停在 `PC=0x08000188/MSP=0x20001260/xPSR=0x01000000`。
- 用户的 CoreSight 调试寄存器读回：`DHCSR=0x00030003`（CPU Halted）、`DEMCR=0x00000001`（复位捕获使能）、`DFSR=0x00000008`（vector catch 事件）。这些证据共同支持本次复位入口停机，不只是“源码窗口停在哪一行”。
- **经验**：不同调试会话、缓存、断点、实际复位方式会改变寄存器现场；标注捕获来源。**不能手工改 PC/SP 后再伪称成功停在复位入口。**

### JUMP-HW-03：W25 `/CS` 与 RCC 异常的正确证据边界

- 在不同调试/供电现场，W25 `/CS` 对 GND 曾测得 `1.3V`、正常上电时 `2.6V`，随后用户确认**复位入口停机的同一现场** `/CS=3.1V`（VCC 此前约 3.2V）。不同条件不能不标注地合并成一个常量。
- 先前 `_RDWORD` 与 Memory 窗均显示 `RCC_APB2RSTR=0x0000A1C0`、`RCC_APB1RSTR=0x20188038`；用户曾碰到接线。**不能因“碰到了导线”或“位对应未实现外设”的推断，就宣告这些读数的物理原因已确认。** 可以在限定且安全的测试条件下继续无关的 CPU 级实验，保留异常记录；如要基于这些寄存器实施危险动作，仍需满足对应前置条件。
- **经验**：W25 `/CS` 空闲高电平应当由硬件上拉/软件归属在复位和异常窗口保障；不同 Boot 运行态读数不能替代 Reset Halt 读数。正式工程按 DESIGN_SPEC 的 PA4 外部 10kΩ 上拉要求和 Phase0/6 实测门槛执行。

### JUMP-HW-04：SRAM 代码真实运行与产品 Boot 可信启动是两件事

- HW05E v2 使用 ARMCC5 编译的 SRAM-only AXF，`ENTRY=0x200032C5`（Thumb）、机器指令起址 `0x200032C4`、镜像 776B；用户上传的 Intel HEX 回读与 AXF 连续 776B **逐字节匹配**，SRAM 区间 `0x20003000..0x20003307`。
- 使用受控 Keil Debug 实验交接后，真机串口记录：

  ```text
  HW05C APP ENTER vtor_in=0x08004000 vtor_now=0x08004000 msp=0x20000698 cfgr=0x00000000 valid=1
  HW05C APP ALIVE
  ```

- 见证：HW05E 标识 `0x20004000=0x48303545`、READY `0x20004008=0x600D005E`、失败掩码 `0x2000400C=0`、结束标记 `0x20004070=0xC0DE05E0`；HW05C 标识 `0x20000000=0x48303543`、自检 `0x20000028=1`、心跳 `0x2000002C=125`（历史来源截图）。
- **可以正式说**：`HARDWARE-ISOLATED PASS` —— CPU 从经回读验证的 SRAM 桥接程序正确交接到已烧写的测试 App，App 见证和 UART 自检真实有效。
- **不能说**：产品 `BootMain→C07A_JumpConfirmed→App`、Metadata/安全信任链、FreeRTOS App、三次 Trial、回滚、IWDG **已通过**。调试窗口出现 `AGDI cannot access target while executing` 表明部分调试访问在 CPU 运行时失败，不否定已有独立串口/见证证据，但也不能据此声称截图中的 PC 是交接后真实 PC。

## 9. 新确定 App 方案的实现经验与预防原则

### APP-01：核心板事实与原理图边界

用户确定 **STM32F103C8T6 核心板**；板载 PC13 LED 已由用户真机程序测试成功、BOOT0 已有板级实现。不要因为局部原理图没有画 LED 就否定真机事实。`i2ctest.ioc` 记录了 PC13 GPIO 输出和 I2C1 PB6/PB7，是配置层面的测试参考，不是位于 `0x08004000` 的正式 App 工程；从 `.ioc` 只能证明配置，不可凭它声称 SSD1306 已成功显示。

### APP-02：SSD1306/I2C HAL 地址、刷新时序与资源

- **设计固定**：SSD1306 128×64、I2C1 PB6/PB7、100kHz、7-bit 地址 `0x3C`，传入 STM32 HAL 的地址是 `0x78`；指令 `0x00`/数据 `0x40` 是 I2C 控制字节，不是两个实际显示设备地址。
- 单一静态 `1024B` framebuffer，使用有界长度写入/6×8 字体 21 字符/行，OLED I2C 所有者只有 Business/Display 路径；所有 I2C 事务有实际可验证超时，遇设备 NACK 立即可退出并记录错误，**不得影响更高优先级 UART 和升级控制任务**。
- Boot 不初始化 SSD1306；Boot 在 YModem、NOR 安装、回滚期间不刷新 OLED。App 显示 `ENTER BOOT MODE` 后复位，OLED 此后保留上帧或熄灭；不要凭 UI 伪造进度。
- 若 F103 I2C BUSY/超时常见异常发生，先对照实际 MCU Rev、ST 勘误/驱动恢复处理，禁止在 ISR 中用 HAL_MAX_DELAY 等待 I2C。

### APP-03：ADC 只测片内结温估值，不冒充室温仪器

F103 的内部温度 CH16，需要 TSVREFE、长采样、实际 ADC 时钟和校准；不能照搬不同 STM32 系列的 `TS_CAL_1/TS_CAL_2` 地址。`Vsense`/V25/平均斜率得到的是**未逐片精度校准的温度趋势**；串口必须输出原始 ADC 与状态，计算溢出、采样失败、VDDA 假设都要作为误差界处理。

### APP-04：3秒/5次不是靠延时任务自增

- 老 V2 App 的“Business 每1s 一次，却要求 Trial 在3s内有5个业务成功周期”不自洽；当前设计改为 **Business 每500ms 有界真实进展**、ADC+OLED 每1s至少成功一次。
- `UpgradeCtrl` 的优先级是**中优先级(2)**，不能因业务逻辑长期阻塞导致 IWDG 接力延误。`UartRecv` 优先级高(3)、`Business` 低(1)；这与 RTOS IRQ 中断抢占优先级的数值关系不同。
- Trial 确认必须**同时满足**运行时间至少3000ms、真实业务序号+5、ADC/OLED 各成功至少一次、UART Ready、无致命错误。不能让 UpgradeCtrl 在自循环中伪造心跳、不能单纯 `vTaskDelay(3000)` 后确认。
- App **只写** `BKP_DR2=0x5AA5` 并软件复位。Boot 检查 Pending/Flash 后写 Metadata CONFIRMED。任何历史“App 直接写持久 Metadata”的示例是**已过时的设计**。

### APP-05：IWDG 与 RTOS 栈、堆的正确计算

`heap_4` 的 `configTOTAL_HEAP_SIZE=6144B` 是首轮候选，内含动态创建任务的 stack+TCB；若再对照三任务 256/192/192 words 单独加进总 SRAM，就会重复计数。需读真实 `map RW/ZI` 和任务高水位、空闲堆。IWDG 已被 Boot 启动时，App 调度器启动前有受控短时喂狗，而运行后只允许 UpgradeCtrl 根据真实业务进展刷新；不能无条件“每2秒喂一次”，否则业务卡死不触发复位。

## 10. 历史冲突裁决与现行方案覆盖表

| 主题 | 曾有争议/错误 | 现行决定 | 未关闭的实验证据 |
|---|---|---|---|
| Flash Metadata | 旧草案内部末页 | **W25 A/B**；完整 48KB 内部 App | Journal 掉电/一致性仍 OPEN |
| YModem CRC | CRC16 0x8005/其他 init | **CRC16/XMODEM 0x1021、init0** | 标准 Sender 与完整硬件协议尚 OPEN |
| 断点续传 | 老教程提议续传 | **V1 标准 Sender 不续传** | 不以未来扩展代替当前异常测试 |
| Boot/Main 架构 | 复制诊断 main，或直接烧 C04-B | **新干净双工程**，历史代码受控迁移 | 新正式 Boot 的 Release 产品容量仍 OPEN |
| MAP 字节 | Clang -151B 或 S3 不同构建的 ROM 做加减 | **同一 AC5/O3 真实入口**的 Code/RO/RW/LR 判定 | 完整最终模块重链 OPEN |
| PB12 | 稳态 K 通过 vs 启动瞬态读错 | 不矛盾；**以 P1 原源码/时序迁移候选并实测** | 物理根因及正式 Boot 全场景 OPEN |
| W25 SPI | 测试 W25 成功 vs 正式入口 SPE=0 | 不同路径；BSP 初始化必须核对硬件 SPE | 正式产品同入口复验 OPEN |
| RTC/BDCR | 旧 V2 与 V3 模型、BDCR 0→8200 | 不认定已修复；谨慎保持现场 | 真实测频、变更主体仍 OPEN |
| Boot→App | 早期所有静态/阻断试验 | **HW05E 独立硬件 PASS；C07 产品路径 NOT RUN** | 产品 C07/Phase0/7 真实通过 OPEN |
| 向量 MSP | 老 C06C 4B，C07A 8B | **现行设计明确 8B、MSP 在 `(RAM_BASE,RAM_TOP]`** | 正式单一谓词与板上跳转测试 OPEN |
| Metadata ABI | 历史 72B 只是候选 | V3.1 **72B 18u32LE/9态/56槽**确立为新的实现 ABI | NOR 真实原子性和旧格式迁移 OPEN |
| App 确认归属 | 旧经验称 App 直写 Metadata | **App DR2 请求+复位；Boot 才 Commit W25** | BKP跨软件复位/Trial end-to-end OPEN |
| FreeRTOS 调度 | 旧 App V2 UpgradeCtrl 最低、Business1s | UARTRecv3/UpgradeCtrl2/Business1，Business500ms | App完整 48KB/20KB、IWDG调度 OPEN |
| OLED 型号/地址 | “兼容OLED”及0x78误作7位地址 | **SSD1306 128×64，7bit 0x3C，HAL 0x78，100kHz** | 实物ACK/首屏/异常恢复 OPEN |
| 温度校准 | 引用外系 TS_CAL_1/2 | F103 CH16 结温粗估，不称室温精度 | 真实读数/错误注入 OPEN |
| LED/BOOT0 | 图局部缺图推断无板载LED | **用户真机确认 PC13 LED、板上 BOOT0 实现** | App正式集成需回归，但不用外接 LED |
| VBAT/USB | 无关施工阻塞开发 | USB D+ 不参与USART1；VBAT无额外施工，BKP使用前功能读回 | BKP有效性与整板断电元数据分离 |

## 11. 未关闭风险与安全操作阻断（融合原风险表）

| Risk ID / 优先 | 状态、问题、发生/可能性证据 | 影响阶段 | 关闭条件（最小要求） | 现在禁止的动作 |
|---|---|---|---|---|
| **R-01 / P0** | 真实完整Boot 16KiB容量及功能覆盖不确定。C04-B 15,596B、P1 15,756B、G4-D 16,116B非同一目标条件；最新SPE修复尚未加到实Boot入口。[S3:44-55][S4:239-254][S5:276-310] | Pre-0，Phase0～9 | 同一最新源码，真实`main→BootMain` AC5 O3 full rebuild，MAP `LR`≤0x4000、所需符号和安全功能全保留；动态RAM/栈另测。 | 用小诊断HEX余量、旧MAP或`LinkOnly`作为最终产品预算/烧录许可。 |
| **R-02 / P0** | `BDCR 0x00000000→0x00008200`来源、写入时刻未知；L/P阻断了当前F，但未恢复时间线。[S4:143-154,277-284] | Pre-0；Phase0、7、8 | 不写备份域的原始BDCR/CSR/DR1-DR10/OptionBytes/复位上下文采集；对启动代码/调试器/工装写访问做可复查归因；安全路径被审批后重新测。 | BDRST、静默修改RTCSEL、清除现场或将`armed=0`解释为历史全无写操作。 |
| **R-03 / P0** | LSI RTC测频真机NOT RUN，V2 RSF/RTOFF差异、V3仅静态修正；RTCSEL可能与App RTC占用冲突。[S3:196-209][S4:117-154] | Pre-0，Phase8前 | 先解决R-02；读写权限与RTC时钟所有权判明；经审批的3×500ms真机测量有有效前后快照/频率误差边界/超时恢复。 | 声称标称40kHz已校准，强制RTC选择或隐式损伤BKP。 |
| **R-04 / P0** | IWDG约4s实际复位、Feed健康时序、长擦写监督、Boot/App交接未实板闭环。C04-C被记为Feed引用容量探针；S3 Feed被removed；HW04 W BLOCKED。[S3:211-225][S4:182-193] | Pre-0风险核验，Phase8/9验收 | Option Bytes/LSI测量、RLR/PSC/启动条件；独立看门狗重置和错误路径、长操作期限、App确认接力原始真机日志。 | 把LongOpSafe/函数指针/阻断输出当IWDG保护，通过硬件启动IWDG后执行无期限擦写。 |
| **R-05 / P0** | C07**产品路径** Jump/VTOR/MSP/NVIC/USART TC未通过；此前4B/8B代码差异现由设计文件统一为8B，真实回归待测。[S1:160-180][S5:214-225] | Pre-0，Phase0与8/9 | 单一向量谓词经ABI核对；独立有效App、负例、真实清理和跳转、NRST/冷启回执/可恢复性。 | 运行历史危险BootMain HEX、未经验证的手写Jump直接切栈。 |
| **R-06 / P1** | PB12首次采样松开误判已观察到；P1-D1条件较广，D2B实际POST-W25仅松开RESET一次成功；偶发物理根因不明。[S4:39-50][S5:44-64,84-121,393-401] | Pre-0，Phase0 | 对最终Boot初始化次序完整测试松开/按住×NRST/真正冷上电，记录GPIO配置、原始采样、失败处理及电源条件；必要时电压/示波器定位。 | 把不稳定读数视为释放/允许Jump；把循环NOP当绝对ms去抖。 |
| **R-07 / P1** | W25 SPI SPE修复仅隔离D2B窄范围PASS，最新完整BootMain链接/真机尚无记录；Mode/CS/Busy恢复可能随入口而变。[S4:52-76][S5:123-147,393-401] | Pre-0，Phase0/6 | 产品BSP显式SPE/CS/Handle状态与SR1/JEDEC读回，O3同版真实入口重链；误初始化/超时/冷上电失败分支回归。 | 从单次D2B `W25 result=0`推导正式Boot已修复且足以开始擦写。 |
| **R-08 / P1** | W25 64KiB BlockErase/全区FF检查OPEN；早期E3/E4是测试授权/地址错误码。[S2:85-95] | Phase6～8 | 严格Download授权、原始备份/指纹→BlockErase→全64KiB地址读回→一致计数+首坏地址→完全断电复验。 | 擦Backup/Metadata/Log，或用底层OK替代全区验收。 |
| **R-09 / P1** | DMA/IDLE真机证据跨窗口不完整；Single consumer、DMA超越/overflow、TX TC主要是候选模型。[S1:94-103][S3:166-179][S4:11-22] | Phase1～4/9 | 同版硬件115200 8N1：HT/TC/IDLE任意交错、ORE/TE、2048B overflow、重发和TC；标准Sender YModem全链。 | 声称CubeMX配置或HW02历史一句PASS证明正式协议可靠。 |
| **R-10 / P0** | Metadata A/B写裂、72B候选后写CRC的真实NOR掉电原子性未证实。[S1:116-125] | Phase7～9 | 模型每写/擦点注入及真机受控掉电，验证旧有效记录、序号/CRC、满槽轮转和清理失败后的再扫描。 | 把CRC最后写4B当硬件原子提交，先擦旧有效扇区。 |
| **R-11 / P0** | Backup真实48KiB复制/双CRC/断电/自动Rollback尚无完整板上验收；旧镜像身份必须独立。[S1:127-136] | Phase6～9 | 有短/长/非块整倍数图像，W25完整48KiB备份/读回，安装失败后恢复旧版本并验证CRC与健康状态。 | 在缺备份/不可信Meta的产品路径擦内部App。 |
| **R-12 / P1** | `RECEIVING→DOWNLOADED`断电重验缺Block0历史文件长度持久信息，不能从Header倒推出原Sender声明。[S1:149-158] | Phase4、7 | 明确各状态可用历史证据/下载长度，并对所有断电点验证保守拒收或完整重验；不改变YModem V1语义。 | 把未保存的历史协议字段想当然判为匹配、添加未授权续传。 |
| **R-13 / P1** | 制造首启根信任/Meta A/B预置工装只有脚本候选，未验真机。[S3:226-239] | Phase6～8 | 独立制造授权与已验App哈希、双扇区写回读、重复预置拒绝、异常NOR恢复计划。 | 空白/损坏NOR自动CONFIRMED。 |
| **R-14 / P1** | 长操作看门狗策略未定案：长擦写前“暂不启动”与“已运行IWDG健康Feed”的适用条件不同；硬件OptionBytes未知。[S3:211-225] | Phase5～8 | 建立各操作最长时间、IWDG启停前提和复位后的恢复策略；实板证明，不改变冻结功能。 | 把一个软件门禁当全部IWDG安全承诺或把Feed裁掉。 |
| **R-15 / P1** | 最坏SRAM/Boot stack/ISR高水位无闭环；某静态账本6392B，测试1024B缓存不可算产品占用。[S1:84-92][S2:121-131][S4:235-254] | Pre-0、全Phase | 静态各对象账本+真实运行栈填充/高水位、极端DMA中断+NOR操作与App任务各自验证。 | 以`RW+ZI < 20KB`作为唯一内存安全证明。 |
| **R-16 / P1** | HAL DeInit依赖成本和安全寄存器特化尚未被真实同功能版本量化。[S1:62-72] | Phase0、优化/9 | 同源A/B MAP、保留函数和VTOR/DMA/IRQ/TC等价测试、最终真机Jump。 | 直接删HAL清理以省ROM。 |
| **R-17 / P1** | Header/CRC部分PC模拟报告缺随批原始日志；C04的CRC表虽链接，但规范字段偏移/大小必须统一。[S1:14-34,225-227] | Phase4～9 | 共享Header精确字段/字节序与32B静态断言；标准已知向量、独立CRC参考及真实Sender。 | 错误CRC16初值、私改Header布局、V1续传。 |
| **R-18 / P1** | 对真机实验的构建和数据链出现错配、旧MAP/HEX、COM失败却SHA成功、重复Load；可追溯性风险有实案。[S4:78-90,156-205][S5:175-199] | 全Phase | 唯一build manifest、绝对路径、同次hash、成功语义验证、下载次数与扇区范围复核。 | 用旧工件PASS、哈希空错误日志当正常快照、自动重试危险命令。 |
| **R-19 / P1** | Release是否保留了完整日志查询、健康Feed、安全回滚路径依版本有差异：B1 log read removed，B2 linked；G4 Feed remains removed。[S3:106-119,211-225] | Phase8～9 | 同一完整Boot MAP检查append/latest/readslot/Feed/rollback真实调用链、主机和板上读回/复位。 | “源码文件存在”即宣告功能完整。 |
| **R-20 / P1** | App健康确认（DR2请求→Boot提交Metadata）、FreeRTOS三Task优先级/并发以及BKP一次性请求到Meta持久确认无产品闭环。[S3:371-379] | Phase9 | Boot+FreeRTOS App真机升级请求→重启→升级→Trial→健康确认→三次失败回滚和日志闭环。 | 仅写BKP_DR3代替Metadata健康确认或单一模拟任务当真实App。 |
| **R-21 / P2** | 统一诊断`main.c`过大，T1～D2多副本，接口差异/遗留探针增审计成本。[S4:208-225][S5:227-261] | 全Phase | 薄入口、模块分层、Git+manifest、测试符号黑名单和小步可回滚补丁。 | 无限复制工程；在产品main.c沿用临时P/F/Q、Watch逻辑。 |
| **R-22 / P2** | ST-Link/USB-TTL回灌、暂停与真实冷上电区别容易被混合；Watch和HAL tick的语义不总明确。[S2:198-205][S5:201-212,340-346] | 全硬件Phase | 真正完全断电/独立供电/Debugger连接方式记录；测试seq/done、精度与状态码字典。 | 单张Watch、同一tick=0或VerifyOK当全部硬件行为。 |



**风险状态更新规则**：本风险表复用历史 `R-01..R-22` 编号，结论先按经验包的二级证据保守维持 OPEN；后续发生的 HW05E 隔离真机跳转，只降低“Cortex-M3 机械交接是否可行”的不确定性，**不关闭产品 R-05**。现行的向量判定规则/Metadata ABI/App 任务已经有明确设计选择，但这些选择没有硬件验收，不能以“设计冲突裁定”替代真实试验。

## 12. Phase 0～9 经验预防门禁（浓缩原阶段清单）

**三层门禁**：`BUILD`（实际 AC5 full rebuild/MAP/HEX），`OFFLINE`（主机模型/负例/故障注入），`HARDWARE`（用户操作且有原始真机证据）。本表是错误预防提示，**正式范围和退出标准始终以 `DESIGN_SPEC.md` §14–15 为准**。

| Phase | BUILD/静态不得遗漏 | OFFLINE 必须覆盖 | HARDWARE 退出证据重点 | 安全 STOP |
|---|---|---|---|---|
| 0 双工程/Jump | 两工程原点、向量、SCB->VTOR、真入口可达、Boot≤16KB | MSP上下界、Thumb PC、CRC/非法向量、PB12优先序、尾段不可返回 | 正式 Phase0 Boot→独立最小 App、PB12按/松、NRST/冷启、UART TTL、坏像阻断 | 不能烧历史 C04-B；不得以HW05E替代 |
| 1 UART/Ring | USART1/2KiB Ring、单消费者、可见错误码 | 空满/255..2048边界、拆分、溢出与指针绕回 | 115200 8N1 长短随机输入/恢复和TC | 禁止两个消费者及探针自动TX |
| 2 DMA/IDLE | RX CH5/512 circular + TX CH4、句柄唯一定义、真实IRQ组 | HT/TC/IDLE任意交错、整圈覆盖、ORE/TE/tx buffer lifetime | 连续字节流与完整错误注入 | HAL和裸寄存器不可重复清同一标志 |
| 3 ASCII/Router | 128B CRLF 行缓冲、明确 Mode owner | 超长、粘包、半包、错误命令、模式切换 | `@IAP:` 与 `@APP:` 的归属和串口响应 | 禁止将IDLE当行结束 |
| 4 YModem/Header | CRC/32B字段、正式格式、发布符号保留 | 123456789向量、Block0/尾块/重复包/NAK/EOT/断电 | 标准 Sender、完整长度/CRC 回读 | 不支持V1续传；EOT≠下载可信 |
| 5 内部Flash MVP | 只有 App 页16..63，半字与越界检查 | 地址/长度溢出、部分安装和非法向量 | 独立批准的受控页操作、读回、Boot未变 | 未备份或危险探针进入 Release 必须 STOP |
| 6 Download/Backup | W25唯一SPE/CS状态、两级范围、48KiB双CRC | 256B页/4KiB sector、64KiB擦、WIP/WEL/超时、旧CRC | Download全区、Backup完整48KiB受控回读 | 不能把 W25 result=OK 当Backup可信 |
| 7 Journal/Recovery | 72B ABI、双 A/B、单点Persist owner、正式策略调用链 | 所有危险状态每次写裂、脏槽、扇区轮转、Provisioning空NOR | 真正切区断电、上电唯一最新可信记录 | 不得先擦旧唯一有效副本 |
| 8 Trial/IWDG/Log | Feed/rollback/log读取真实保留；无LinkOnly | attempts=3、BKP DR2资格、超时、日志损坏、长操作复位 | LSI约4s实测、三次Trial、自动回滚与冷启 | W BLOCKED/Feed地址探针不算真狗PASS |
| 9 FreeRTOS OLED | 三任务优先级/heap高水位、App≤48KB、Boot最终≤16KB | 500ms心跳/3s健康、I2C超时、ADC错误、UART并发、端到端回归 | SSD1306 0x3C显示、温度结温、V1→V2确认/失败三次回滚 | App直接写W25、忽略OLED健康、随意喂狗均STOP |

### 12.1 每轮最小变更与报告格式

```text
PHASE / SPEC SECTION / LESSON IDs:
FILES_CHANGED / SOURCE OWNER / SIDE EFFECTS:
REAL TOOLCHAIN / COMMIT / BUILD ID:
FULL_REBUILD: 0 errors; 0 warnings or reviewed exceptions; actual log path
MAP: Boot Code/RO/RW/ZI, LR_IROM1, RAM static + runtime stack budget
REAL ENTRY: retained/removed functions; test-only paths excluded
HOST TESTS: cases, expected, actual (or NOT RUN)
HARDWARE: device, image hash, allowed addresses, raw output (or NOT RUN)
RISKS: R-XX still OPEN / newly closed with evidence
NEXT ACTION: Codex executable work vs CubeMX/user hardware step
```

**不要把模板当作脚本已经运行的证据。** 按现有工程工具可访问性执行；AC5 或 ST-Link 不可访问时报告 `NOT RUN`，不得生成假的 Rebuild/MAP/串口结果。

---

## 13. 总结：保留的核心工程教训

- **安全不变量是底线，代码路径是证据**：Release 必须通过真实 AC5 O3 目标构建、入口可达性/危险调用路径审查与正式 Phase 真机验收。
- **设计已统一≠硬件已验收**：72B NOR 提交、BKP 软件复位保持、IWDG/LSI、SSD1306 和完整 W25/Flash 事务必须逐阶段关闭。
- **每个硬件资源一个所有者**：HAL 与寄存器访问互相一致，UART 事件只传进度，W25 CS 必须连续/失败释放，内核跳转是一条不可返回的尾链。
- **验证记录不能混淆**：C04-B静态容量、HW05E隔离交接、Phase0产品 Boot Jump、Phase9升级回滚属于四种不等价证据。
- **三份文档即可运行**：产品**是什么**看 `DESIGN_SPEC.md`，历史**为什么出错**看本文件，Codex **必须怎样工作**看 `AGENTS.md`。遇新案例更新这里；不得继续并列维护已经被融合的四份原压缩包文档。

## 14. 正式配置准备的新增审查事实（2026-10-12）

本次没有产品代码迁移、烧录、主机模块测试或新目标AC5构建。完整配置与实施契约唯一放在 `doc/DESIGN_SPEC.md` §19，本节仅记录失效假设与预防依据。

- 实际根为 `IAP_PROJECT/`，AGENTS 已位于根，无需从doc移动；修正其doc路径。当前不是Git仓库，不伪造diff/status/commit；本次清理重复提取、旧快照和临时差异文件，维护结论于现有文档，Git 初始化由用户执行。
- 本机CubeMX6.17.0/FW_F1V1.8.7；HAL1.1.10、FreeRTOS10.3.1、RVDS/ARM_CM3真实存在。CubeMX的F1 Mode XML中Native模式被注释，看到Native模板不等于GUI可选。采用CMSIS_V1取件、User Native API唯一启动，生成task骨架与运行注册不得同时生效。
- CMSIS_V1 `makeFreeRtosPriority` 实际为 `priority-osPriorityIdle`；产品3/2/1对应Normal/BelowNormal/Low，不是High/Normal/Low的直接文字对应。检查生成task栈单位，最终xTaskCreate用words。heap_4包含动态Idle与用户任务栈/TCB，禁重复计算。
- HAL_IWDG_Init源码立即执行__HAL_IWDG_START；只在USER CODE里写“稍后启用”而保留自动MX_IWDG_Init无法保护Phase0。必须在Advanced Settings禁止生成调用，App禁止Start，实际启动仅Phase8受控owner。LSI开启与IWDG启动是不同副作用。
- HAL1.1.10已实现ReceiveToIdle_DMA及HT/TC/IDLE回调，HAL IRQ独占标志清除；不要再叠加手写IDLE/两套DMA回调。同抢占优先级6避免生产者嵌套，但不证明Flash阻塞/超圈不会丢数；此项仍须Phase2错误模型和真机验收。
- P候选.ioc无72MHz树，main明确要求HSI8MHz；SPI Mode3但分频8。正式HSE72MHz、SPI分频16产生4.5MHz，应验证而非称“与已测速度相同”。PB12循环4000/64/8000没有绝对毫秒含义；P1-D2B窄证据不能关闭全部复位/冷启组合。
- 候选04B工作目录当前MAP显示ROM5760B；同目录归档C04_B_REAL_BOOT.map显示15596B，不能依据文件夹名混用。历史Build日志支持AC5 build960，本轮沙箱内armcc --vsn发生Windows DLL重定位错误，获许可后沙箱外真实输出确认MDK5.37/AC5 update7 build960；未构建新目标，不推定新产品容量或零Warning。
- BKP无需RTC日历初始化。F103C8没有TIM5，不采用其他密度的LSI→TIM5CH4重映射方案；Phase8 RTC测频必须HSE参考、RSF/RTOFF有限等待、RTCSEL保守检查，绝不自动BDRST。App不配置RTC/BDCR。
- 本地DS5319 Rev18表50传感器启动max10µs、采样17.1µs带脚注；12MHz/239.5cycles采用更长19.958µs窗口，仍须应用验证，不是逐片温度精度证明。PC13输出速度不超2MHz。W25BV手册时序不可授予未知子型号写擦权限。
- 已读Header候选没有firmware_version0拒绝和输入缓冲长度参数，不能直接部署；完整函数级迁移审查/已知向量/ARMCC5回归仍属于对应模块工作。

此前引用的i2ctest与独立HW05C/E原始工程在本树未定位到；保留历史来源，不以不存在的当前工件重新签PASS。所有风险表OPEN不因本次配置设计完成而自动关闭。

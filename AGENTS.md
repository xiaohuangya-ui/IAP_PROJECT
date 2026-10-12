# STM32F103C8T6 IAP — Codex 常驻工程约束

> **身份**：正式仓库根目录唯一 `AGENTS.md`。**产品依据**：`doc/DESIGN_SPEC.md`（V3.1）；**问题与依据**：`doc/ENGINEERING_LESSONS.md`。仅此三份核心管理文档。若本文件与设计规格冲突，立即以 `doc/DESIGN_SPEC.md` 为准并报告差异；不得在没有用户批准的情况下改设计。
>
> **当前阶段**：检查点 A（架构与一次性 CubeMX 配置设计）；用户生成两个新工程后，Codex 集中审核，只有 `CUBEMX_BASELINE_REVIEW = PASS` 才进入 Phase 0 代码开发；正式产品 Phase 0～9 尚未签署验收。历史 HW05E→HW05C 独立 SRAM→App 通过，**产品 Boot Policy/C07 实际跳转未通过**。

## 1. 工作边界与优先级

1. **MUST** 先阅读 `doc/DESIGN_SPEC.md` 中当前 Phase 的相关章节和本文件，再按模块查阅 `doc/ENGINEERING_LESSONS.md`。所需原始历史工程/芯片资料不存在时记录 `OPEN`，不可伪称已经审查。
2. **MUST** 按 **Phase 0→9** 的既定顺序开发与验收；一个 Phase 可批量完成本阶段的离线代码、构建和测试，但未经对应硬件退出证据，不得进入下一 Phase 的正式实现或声称 ACCEPTED。跨阶段离线预研必须单独标记。
3. **MUST NOT** 自行缩减已确定功能：标准 YModem、DMA/IDLE、W25 Download/Backup、双扇区 Metadata、日志、CRC、持久状态机、掉电恢复、三次 Trial、IWDG、回滚、FreeRTOS 三任务和 SSD1306/片内温度业务。
4. **MUST** 遇到手册与现行设计的硬件可行性冲突，保存文档/章节、实测事实和拟议方案，请用户确认；不得悄悄修改引脚、存储 ABI 或时序。
5. **用户所有权**：用户操作 CubeMX `.ioc`、接线、ST-Link、真机烧录、硬件验收。Codex 可以审查 `.ioc/.uvprojx/scatter`，输出精确配置表，编写和审查代码/脚本/主机测试；**不得擅自修改 `.ioc`、连接实机、烧录或改变 Option Bytes**。
6. **当前设计事实**：固定 STM32F103C8T6 核心板；已确认板载 PC13 LED、板上 BOOT0；SSD1306 128×64、I2C1 PB6/PB7；Boot 独占 W25 SPI1。**不要要求外接 PC13 LED、重做 BOOT0 电路或增加不相关的 USB D+ 检查**。VBAT 不额外施工，但 BKP 跨软件复位能力须真机验收。

## 2. 不可越过的硬边界

| 资源 | 必须保持 |
|---|---|
| MCU | STM32F103C8T6，**仅 64KiB Flash、20KiB SRAM**；不可利用非标扩容 |
| Boot | `0x08000000..0x08003FFF`，`0x4000`（16384B），任何 IAP 写/擦均禁止 |
| App | `0x08004000..0x0800FFFF`，`0xC000`（49152B）；唯一内部可升级槽 |
| RAM | `0x20000000..0x20004FFF`，`0x5000`（20480B）；Boot/App 分别核算 |
| W25 | Download `000000..00FFFF`；Backup `010000..01FFFF`；Meta A `020000..020FFF`；B `021000..021FFF`；Log `022000..025FFF`；其余保留，**默认禁止写** |
| 工具链 | Keil MDK 5.37 / **ARM Compiler 5.06 update 7 build 960**；O3 是历史容量比较基准；不得使用 ARM Compiler 6 的 `-Oz/-Os` |
| 运行 | Boot 裸机；App FreeRTOS；App `Task_UartRecv` 高(3)、`Task_UpgradeCtrl` 中(2)、`Task_Business` 低(1) |

**版本协议唯一来源是 `doc/DESIGN_SPEC.md`**：32B Header（8×LE u32）、magic `0x46574D47`、目标 `0x08004000`、CRC16/XMODEM `0x1021/init=0`、CRC32/IEEE 反射 `0xEDB88320/init=xor=0xFFFFFFFF`；USART1 115200 8N1、DMA RX CH5/TX CH4、512B circular/2KiB Ring、CRLF `@APP:/@IAP:`、单文件标准 YModem V1 无续传。**不得沿用历史错误 CRC、旧内部 Metadata 页、私有续传或旧 Header 格式。**

## 3. 文件所有权与实际编译

1. **新正式双工程**，历史 `C07_*NO_JUMP*` 只作配置参考，`G4D_*Candidate04B*` 只作容量/算法参考，HW05C/HW05E 只作隔离硬件证据；历史工程只读。**禁止将历史 C04-B/LinkOnly/测试 HEX 作为可烧录产品固件**。
2. `Core/` 是 CubeMX 生成入口和 USER CODE 挂钩；ST `Drivers/` 不编辑；用户代码放 `Bootloader/User/{BSP,Drivers/W25Q64,Protocol,IAP}`、`Application/User/{BSP,Drivers/SSD1306,App}`，纯算法放 `Shared/`；测试放隔离 Test Target。保持 `main.c` 薄入口。
3. **唯一物理实现**：W25 驱动只在 Boot 一份；SSD1306 驱动只在 App 一份；共享 CRC/Header 不能有双份实现。UART/DMA、SPI/CS、I2C/OLED、BKP、IWDG、Flash、Jump 各自唯一 owner；HAL 句柄只定义一次，其他模块用正确声明。
4. 每添加/移动 `.c`，**必须同时核对 Windows 物理路径、Keil Source Group/Exclude from Build、Include Paths**；头文件可找到不等于实现参与链接。遇 `L6218E` 优先核对真实参与编译的 `.c`、定义、`static`、签名和条件编译。
5. `Read/Scan/Decide/Verify` 接口不得暗含介质写入；`Commit/Erase/Program/Jump` 明示副作用、权限、错误处理。Protocol 不直接调用 NOR/内部 Flash；IAP FSM 拥有事务状态，Store 管 NOR 分区权限，设备驱动管物理边界。
6. 当前不预建 Bootloader、Application、Shared、Tests、Tools、Evidence 等目录；Bootloader/Application 由用户 CubeMX 首次生成，其余目录仅在对应模块实施时按需建立。CubeMX 重新生成后审查初始化顺序、NVIC/DMA、代码保留性与 Keil 工程编译组；不得修改会被再生成覆盖的非 USER CODE 区域。
7. Release Target **禁止** LinkOnly/size-probe、Watch 危险开关、Host mock、诊断自发串口、工装 Provisioning、Phase0 测试白名单和任何自动擦写自检。Phase0 专用测试构建必须以名字和源码隔离；它不等于 Release。

## 4. 编码、HAL + 寄存器与中断原则

- 默认使用 STM32F1 HAL 完成初始化、常规 GPIO/SPI/I2C/ADC/USART；仅在 DMA CNDTR、IDLE 标志、VTOR/MSP、BKP/RTC/IWDG 等确有必要时采用 **CMSIS/寄存器**，并在所属 BSP/Jump 记录访问顺序、副作用、超时和 HAL 状态一致性。
- ISR 必须有界且轻量；USART IDLE/HT/TC 是**字节流进度**而非帧界。DMA RX **单一生产者**搬运，Ring **单一前台消费者**，溢出/ORE/FE/NE/TE fail-closed；不得在多个 HAL/手写 ISR 中重复清标志。
- DMA TX 缓冲生命周期必须覆盖 DMA 完成**和 USART 硬件 TC**；在 `@APP:UPDATE` 写 DR1/复位前等待 TC。禁止 DMA 引用已退出作用域的局部变量。
- W25 必须保证复位/空闲 `/CS` 高、SPI 确实 SPE=1；命令与数据片选连续，失败释放 CS，WREN 校验 WEL，WIP 有限超时，跨 256B Page 拆分；HAL_OK≠介质写入已验证。4KB/64KB 擦除需要明确业务授权。
- App 仅配置 `ADC1 CH16`（F103 片内**结温估算**）、`SSD1306`（7-bit `0x3C`，HAL `0x78`）、PC13 板载 LED、USART1/BKP/IWDG；**不得初始化 SPI1/W25、不得写 Metadata**。OLED I2C 100kHz、有界超时、单一 1024B framebuffer，不在 ISR 或 Boot 工作期间刷屏。
- App 健康确认：只有 `Business` 的真实成功进展（≥5 次）、经过 ≥3000ms、ADC/OLED 至少成功一次、UART Ready、无致命错误，`UpgradeCtrl` 才写 **BKP_DR2=0x5AA5 → 软件复位**；**由 Boot 复核 Pending/CRC 并写 W25 CONFIRMED**。旧经验中的“App 直接写 Metadata”是过时描述，禁止采用。
- IWDG 约 4s 依赖**实际 LSI**和最短超时时间，Boot 启动、App 接力**有条件喂狗**，长擦写不允许无期限循环。不能把软件安全门禁/链接保留探针称作真实看门狗测试。

## 5. 事务和恢复的不变量

1. W25 **72B A/B Journal** 是唯一掉电持久权威；末 CRC32 后写+回读只是提交设计，**不能宣称 NOR 原子性已硬件证明**。完整的记录布局、9 个持久状态编码、版本和 32B Log ABI 以 `doc/DESIGN_SPEC.md` 为唯一依据。
2. 先持久化 `RECEIVING` 再接收；完整 YModem 后重新 W25 回读/Header/CRC/向量验证再标 `download_valid=1`；备份前 `BACKING_UP + backup_valid=0`；**只有 Download 和 48KB Backup 物理+有效双 CRC 均可信、且 `INSTALLING` 已提交回读成功，才允许擦内部 App**。
3. 只有完整重装/回读 CRC/向量合格才进入 `PENDING_CONFIRM`；每次 Trial 跳转前持久 attempts+1（≤3）；第三次成功确认允许提交；否则从独立 CRC 可信的旧 48KB Backup 完整恢复并记录；没有安全恢复路径必须停 Boot。
4. `BKP_DR1=0x55AA` 是 App 升级请求，`DR2=0x5AA5` 是 App 的**一次性确认请求**，`DR3=0xA33A` 由 Boot 标识 Trial。BKP 不替代 W25 持久状态，不保证整板断电保持。所有权限/清除时机在 Boot/App 间一致。
5. `Recovery/unfinished install/rollback` 优先于 PB12 与普通升级；有效确认 App 没有升级请求时**尽快正常跳转**；无有效 Metadata/冷启出厂未 Provision 不得凭向量自动宣布 CONFIRMED。
6. Flash/NOR 地址、加法、页界、长度、回绕都要无溢出验证；异常、脏记录/重复序号、单副本损坏、日志损坏、部分擦写和掉电都应有确定性失败或恢复路径，不编造成功。

## 6. 16KB 资源与 Warning 治理

- 每个重要提交保存 **同一构建的**完整 Rebuild 日志、Keil Target/Compiler Flags、AXF/MAP/HEX 指纹，审查 `.map` 的 Code/RO/RW/ZI、`Code+RO+RW`、`LR_IROM1`、移除/保留 Sections、**真实 main→BootMain 可达性**及正式配置下的功能覆盖；不可单凭 XREF/源码出现函数名就判定功能已入镜像。
- Boot ROM≤16384B，App ROM≤49152B，静态 RAM `RW+ZI` 不得越界，还要核算堆、任务栈、ISR/主栈峰值。**App heap_4 已包含动态任务栈/TCB，禁止二次计数**。
- Boot 14KB 是优化预警，不是释放安全功能的授权；历史 15596/15768 等均为**不同候选静态构建**，不能成为正式产品大小承诺；禁止靠放宽 IROM、丢回滚/IWDG/Journal、用 Clang 数值充当 AC5 净收益。
- **目标零 Warning**。完整 Rebuild 中的新 Warning 默认阻断合入；如 ARMASM `A1581W` 这类已审查的对齐提示，在代码/机器码/链接布局确认且**逐条记录例外**后可作为已知例外，不得全局关闭警告或把增量 0 Warning 当完全通过。
- O0 与 O3 行为不同，先查 UB、生命周期、编译器选项、SPI SPE、DMA/ISR 并发和真实反汇编；不能仅加 `volatile`、把 O3 降到 O0 或武断归罪编译器。

## 7. 操作安全、证据与 STOP

- **Codex 发起的实验/调试/部署动作**若涉及内部 Flash 擦写、W25 Backup/Metadata/Log 擦写、Option Bytes、BKP/RTC/BDCR、IWDG 强制启动、产品 BootMain 运行或实机跳转，必须取得用户对该次操作和目标范围的明确授权；物理操作由用户亲自执行。**正式已授权产品固件内部**的 Download/Backup/Metadata/Log/安装事务则按照设计状态机和持久安全授权**自动执行**，不要求每一页都人工批准。只读检查优先，任何危险操作都需明确镜像身份、范围、保护与可恢复性。
- ST-Link 受控写入时禁止 Mass Erase、错误页号或自动重复危险操作；烧录前后审 HEX 覆盖地址，独立读取 Flash 比对 Boot 不变；不使用 Watch 值设置危险门。
- 区分 `DESIGN` / `STATIC(BUILD,HOST)` / `HARDWARE-ISOLATED` / `PRODUCT-HW` / `ACCEPTED`。`BLOCKED` 证明守卫有效，不证明危险动作或后续功能已经成功。每个 PASS 附固件身份、环境、范围、原始日志或审计产物；缺输入写 `NOT RUN/OPEN`。
- 历史仍 OPEN：产品 C07 真实跳转、最终Boot容量/栈、PB12全场景、W25全区擦写/完整 Backup、72B Journal 掉电写裂、LSI/IWDG 约4s接力、BKP写读跨软复位、FreeRTOS+SSD1306 完整资源及端到端升级。可继续不涉及危险动作的离线开发；**不得为赶进度把 OPEN 改 PASS**。

## 8. Codex 单轮执行契约

1. 开始：说明 `PHASE / 目标 / 要读的设计章节和经验案例 / 文件所有权 / 安全副作用 / 离线测试`；若用户已给出完整要求，**直接执行而非反复询问**。CubeMX 待用户操作的配置集中一次列出。
2. 开发：优先最小变更，真实编辑代码、补齐边界/错误/host tests，按可用工具完整构建与审查。若工具链或硬件不可达，不许伪造日志/PASS；保留可执行命令供用户完成。
3. 输出：`文件差异；BUILD/TEST 的真实结果；MAP 容量与 Warning；安全影响与 STOP；用户必须做的 CubeMX/实机步骤；该 Phase 未关闭项`。避免逐函数无效报告；**不自动推进下一 Phase**。
4. 本三份核心管理文档以外，构建产物、证据日志、源码、厂商手册可以正常存放，但**不新增并行的设计规范、风险台账、Agent 规则文件**。新增经验直接归并 `doc/ENGINEERING_LESSONS.md`，设计变更仅在用户确认后更新 `doc/DESIGN_SPEC.md`。

## 9. 参考代码与 V1/V2 产物使用门禁

- **MUST** 修改模块前按需查阅 `doc/REFERENCE_REUSE_AND_V1_V2.md`；外部 H747 文件只有路径/摘要、尚未读取源码时明确 `NOT REVIEWED`。优先参考已验证的 F103 候选；H747 只能以经过审查的算法/架构思想迁移，不能照搬 H7 寄存器、双核、双 Bank/Slot、EEPROM、HTTP/Resume 或 FLASHWORD。
- **MUST** 对每个迁移模块列出 `原文件→目标文件→可用思想/接口→禁止内容→Host/ARMCC5/真机验证`，只实现当前 Phase 相关功能，不能整库复制。旧教程示例不具备产品验收地位。
- **MUST** Phase 9 从一套 App 源码构建 `APP_V1_RELEASE` 与 `APP_V2_RELEASE` 两个真实镜像，遵守 `doc/DESIGN_SPEC.md` 的版本/CRC/健康确认要求。V2 必须有真实可见 UI 差异；测试专用 V2 健康失败镜像与 Release 隔离。三套测试场景（正常升级、从独立 V1 基线回滚、掉电恢复）分别保存硬件证据。
- 本辅助指南**不是第四份核心管理规范**。如与设计不符，先遵守 `doc/DESIGN_SPEC.md` 并报告冲突。

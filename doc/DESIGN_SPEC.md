# STM32F103C8T6 可靠 UART IAP Bootloader + SSD1306 智能温度仪表盘

**统一产品设计与逐阶段实施规格 V3.1｜2026-10-12（板卡实测校订）**  
**用途：** 正式产品的唯一设计依据；配套 `doc/ENGINEERING_LESSONS.md` 记录实测经验，仓库根目录 `AGENTS.md` 约束 Codex；技术参考导航见 `doc/REFERENCE_REUSE_AND_V1_V2.md`。  
**硬件基座已确定：** 用户实际使用的 STM32F103C8T6 核心板（U2=LQFP48，8MHz HSE、32.768kHz LSE、5V→3.3V 电源、双 20pin 排针；**板载 PC13 LED 已由用户真机测试成功，BOOT0 启动电路由核心板提供**）；外挂设备为 W25Q64 和**确定为 SSD1306 的 128×64 I2C OLED**。所附核心板原理图为局部图，不得以图上未画出某电路否定用户已经实测的板级事实。不得擅自改用其他 MCU、OLED 控制器或改变核心板。  
**开发分工：** 用户亲自配置 STM32CubeMX、接线、烧录与真机验收；Codex 编写、审查与测试代码，提出精确配置清单和可重现构建结果。  
**规范语言：** MUST=必须；MUST NOT=禁止；PASS=证据满足；OPEN=尚无证据、对应危险操作停止。本文所列 *配置值* 是本轮明确确定的实现基准，不等于全部硬件参数已经在板上实测。  
**版本边界：** 既有功能范围（Boot 16KB、App 48KB、W25 固定分区、YModem、Metadata、掉电恢复、回滚、IWDG、FreeRTOS 三任务与 Phase 0～9）不缩减。本次新增/合并的 App 业务：SSD1306 仪表盘、F103 内部结温估算、`@APP:TEMP`。此前 R4 的候选验证**不能冒充产品验收**。

---

## 0. 设计决策和不可改变的边界

### 0.1 最终系统功能

- **Bootloader（裸机）**：安全上电与 PB12 手工恢复，USART1 115200 8N1，512B Circular RX DMA、HT/TC/IDLE、2KB RingBuffer、TX DMA，ASCII 命令与单文件标准 YModem，固件 32B Header + CRC16/XMODEM + CRC32/IEEE，W25 Download/Backup/Metadata A/B/升级日志，App 区 IAP 安装、危险状态持久化、断电恢复、最多三次试运行、健康确认与旧版回滚、IWDG 接力、安全跳转。
- **Application（FreeRTOS）**：`Task_UartRecv` 高优先级、`Task_UpgradeCtrl` 中优先级、`Task_Business` 低优先级；SSD1306 128×64 OLED 显示固件版本、片内温度趋势、运行时间和健康状态；USART1 `@APP:VERSION/@APP:TEMP/@APP:UPDATE`；BKP 升级请求/确认；IWDG 健康监督。
- **PC 端**：`fw_pack.py` 打包 `header32+app.bin`；使用现成标准 YModem Sender 和普通串口终端；不开发专用 PC GUI。
- **明确不做**：内部 Flash 双固件槽、App 直接写 W25、基于 W25 Reserved 的业务日志、HTTP/云 OTA、加密/签名、V1 YModem 断点续传、文件系统。**CRC32 是差错检测，不是来源鉴权。**

### 0.2 权威及证据等级

1. 本文经用户确认后的最新固定设计约束优先于旧草稿；官方器件手册决定硬件能否实现；真实实验结果证明*实际发生的行为*，不能自动改变产品功能要求。
2. `DESIGN`：规定应如何实现。`STATIC`：源码/单测/AXF/MAP/反汇编有证据。`HARDWARE`：对应镜像与板卡实测通过。`ACCEPTED`：Phase 全部退出条件与安全证据齐全。
3. **现有真实证据**：C04-B 16KB 静态链接候选一次约 15,596B（余 788B）；另一次 P1/P1-D2 相关候选约 15,768B（余 616B）；HW05C App 受控写入 `0x08004000` 且 Boot 16KB 回读不变；HW05E v2 776B SRAM 回读吻合，独立 SRAM→App 真机串口 `valid=1`、`APP ALIVE`，SRAM 见证匹配。**正式 C07 产品路径、完整可靠升级、完整 SSD1306 App 未因此 PASS。**
4. R4 的某些存储编码源自候选/容量实验（例如 72B Journal）。本版定义新的**产品线协议基线**供正式实现；如已有同用途的持久数据，不能直接套用并声称兼容，须做版本/格式迁移或重新受控初始化。设计基线可执行≠硬件/产品已验收。

### 0.3 本轮纠正旧方案的问题

| 旧方案歧义或错误 | 本文的唯一可执行决定 |
|---|---|
| 把芯片内部 Flash 最后一页留作 Metadata | 16KB Boot + 完整 48KB App；Metadata **仅**在 W25 A/B |
| CRC16 的 `0x8005` 实现混入 YModem | YModem 固定 CRC16/XMODEM `poly=0x1021 init=0` |
| V1 断点续传 | 单文件标准 YModem，不续传 |
| “双槽/双备份”混指 | 内部仅 **1 个 App 槽**，W25 **1 个旧 App 物理备份**，2 个 Metadata 扇区 |
| 对 SSD1306 类型或尺寸仍用“类 OLED” | **SSD1306 128×64 I2C** 固定，不得替换 |
| 局部原理图未画 LED，被误判为必须外接 LED | **用户确认 PC13 板载 LED 已存在且真机代码测试成功；正式 App 直接使用板载 LED，不另接灯或限流电阻** |
| App 高/中/低优先级与历史固定要求颠倒 | UARTRecv=高、UpgradeCtrl=中、Business=低 |
| 每 1s 业务却在 3s 内要 5 个心跳 | Business 每 500ms 产生真实进展，ADC/OLED 每 1s |
| UpgradeCtrl 自己加 heartbeat | 只采用 Business 成功提交的序号 |
| App 在 Boot/YModem 时实时显示 OLED | Boot 独占串口/升级，**不驱动 OLED**，App 只在运行期间更新屏幕 |
| F103 使用其他系列 `TS_CAL_1/TS_CAL_2` | 用 F103 的 Vsense、VDDA 与典型 V25/斜率估计芯片结温 |
| 6KB FreeRTOS heap 与动态任务栈重复相加 | 动态任务栈/TCB 已在 `heap_4` 堆中分配，不重复记账 |
| IWDG 无条件每 2 秒喂 | 依据实测 LSI 的最短窗口和业务进展有条件喂狗 |
| “USART DMA TX 完成即可复位” | 必须再等 USART 硬件 `TC=1` |
| 将 HW05E 成功判为完整 C07 PASS | 只认独立 CPU 交接真机通过，产品 C07 仍需 Phase 验收 |

---

## 1. 真实核心板、电气连接和固定外设配置

### 1.1 核心板原理图所能确认的事实

用户确认上传图为**本项目固定使用的核心板**，故以下不再作为“换板候选”：U2 是 STM32F103C8T6 LQFP48；Y2 是 8MHz HSE；Y3 是 32.768kHz LSE；核心板有 3.3V 稳压供电和 VCC3V3/GND 排针；PA4、PA5/6/7、PA9/10、PB6/7、PB12、PC13、NRST、VBAT、BOOT0、SWD 等信号按图引出。**这是核心板的局部电路图，不是全板网络的完整证明；用户实测结果优先用于确认板载 LED 和 BOOT0 已实现，W25、SSD1306、PB12 按键仍按下文固定接线。**

**已确认的板卡事实：** PC13 板载 LED 已存在、用户已通过测试代码验证；BOOT0 的正常启动电路已在板上实现，不需要再外接 10kΩ 下拉或新增跳线。App 中 PC13 按已测的低电平亮/高电平灭方式使用，Boot 不依赖 LED。**板载 USB D+/D− 不用于本项目的数据传输**：升级和调试只使用 USART1 外接 3.3V USB-TTL，因此不把 USB D+ 电路排查列为 Phase 前置任务。**VBAT 不设独立电池或专门施工任务**：BKP 仅承担 App→Boot 软件复位时的一次性请求/确认；后续必须通过读写回环的实际测试确认复位后可读，整板断电恢复仅依赖 W25 Journal。若该 BKP 跨软件复位试验失败，才查明 VBAT/备份域供电原因，不在本轮无故修改板子电路。

### 1.2 最终接线表（外设和排针）

| 设备/信号 | STM32 接口 | 外部连线、元件与所有权 |
|---|---|---|
| W25Q64 `/CS` | `PA4` | Boot GPIO 推挽输出，低有效；**外部 10kΩ 上拉至 3.3V**，MCU 复位/高阻时仍应保持高电平 |
| W25Q64 `SCK/MISO/MOSI` | `PA5/PA6/PA7` | Boot SPI1 Master，3.3V，SPI Mode 3；W25 GND/VCC 分别接公共地/3.3V |
| USART1 TX/RX | `PA9/PA10` | 3.3V USB-TTL：PA9→适配器 RX，PA10←适配器 TX；共地；**禁止连接 5V TTL 电平** |
| SSD1306 OLED `SCL/SDA` | `PB6/PB7` | App I2C1 AF Open-Drain；SSD1306 4-pin 模块 GND、3.3V、SCL、SDA；SCL/SDA 各 4.7kΩ 至 3.3V（若模块已有上拉，测量有效并联阻值，禁止重复导致过强负载） |
| OLED 模块 | 固定 `SSD1306` | **128×64、I2C、7 位地址 `0x3C`**；若实物 SA0 地址不是 `0x3C`，按器件地址选择配置修正/更换接法后再验收，**不允许代码悄悄自动接受未知地址** |
| Recovery 开关 | `PB12` | 外部 10kΩ 上拉至 3.3V，按键按下短接 GND；启动采样低有效 |
| 状态 LED | `PC13` | **使用板载 LED（用户已实测）**；GPIO 输出、低亮高灭；不添加外接 LED、限流电阻或额外接线 |
| SWD | `PA13/PA14`, `NRST` | ST-Link SWDIO/SWCLK/NRST/GND 正确对应；支持 Under Reset；SWD 固定保留 |
| 正常启动 | `BOOT0` | **核心板已实现所需启动电路**，不另外接下拉电阻或跳线；正常复位应从内部 Flash 启动，Phase 0 用启动行为确认 |
| BKP/备份域 | `VBAT` | **不作为单独板卡改线任务**，不新增电池；BKP DR1/DR2/DR3 必须经过上电供电持续下的写入→软件复位→Boot 读取实测才能用于可靠升级控制。若失败才排查板上备份域供电；整板断电的状态恢复始终依赖 W25 Journal |
| 供电 | VCC3V3/GND | 外设统一 3.3V，单一受控供电，所有外部模块共地；避免 ST-Link、USB-TTL、板载 USB 造成反向灌电 |

**与本项目实际有关的上电检查：** ①核心板 3.3V 供电正常；②W25 `/CS` 上电及复位入口停机稳定为高电平；③核心板按已实现的 BOOT0 电路正常从内部 Flash 启动；④OLED 的 `0x3C` 应答正确、I2C 上拉不接 5V；⑤PB12 按下低/松开高；⑥ST-Link NRST 接线正确；⑦**在首次使用 BKP 请求/确认功能前**完成“写 DR1/DR2→软件复位→Boot 读回/清除”的功能验收。板载 LED 不需新接线；VBAT 不提前列额外跳线任务；原生 USB D+ 不在本项目检查范围。**危险操作的相关前置条件不满足时 NO-GO，但允许继续离线开发。**

### 1.3 CubeMX 固定时钟和外设参数

| 配置 | Bootloader | Application |
|---|---|---|
| RCC | HSE Crystal/Ceramic Resonator 8MHz；PLL ×9；SYSCLK 72MHz | 同 Boot；Application 在启动代码中重新初始化为同一时钟树 |
| 总线 | HCLK 72MHz；APB1 36MHz；APB2 72MHz；ADC 时钟 PCLK2/6 = **12MHz** | 同 Boot |
| 时基 | HAL SysTick 1ms，裸机服务主循环 | FreeRTOS SysTick 1ms；**HAL Timebase=TIM2，TIM2 NVIC 抢占优先级 15**，禁止 HAL Tick 与 RTOS Tick 重复占用同一 SysTick |
| USART1 | PA9 TX / PA10 RX，115200、8N1、无流控 | 相同；仅在当前镜像运行时拥有串口 |
| RX DMA | DMA1_CH5 Circular，8-bit，Memory Increment，512B，HT/TC/TE IRQ | 相同；单独分配自身 512B 缓冲 |
| TX DMA | DMA1_CH4 Normal，8-bit，Memory Increment，TC/TE IRQ | 相同；最后等待 USART `TC` |
| USART1 IRQ | enable IDLE/错误中断；HT/TC/CNDTR 只由一个收集器负责 | NVIC Priority Group=4（4 位抢占）；USART1、DMA1_CH4/5 的预占优先级定为 6；FreeRTOS `configPRIO_BITS=4`、`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5`，ISR 使用 `FromISR` 必须满足优先级数值≥5 |
| SPI1 | Master Full Duplex、8bit、MSB、**CPOL High / CPHA 2Edge (Mode 3)**、软件 NSS，预分频 16（SCK=4.5MHz） | **不启用** SPI1、W25 驱动 |
| W25 `/CS` | PA4 GPIO output Push-Pull，初始化前先保持外部 10k 上拉；GPIO 输出先置高再配置 SPI 通信 | 不初始化 PA4 |
| PB12 | GPIO 输入，上拉，低有效，启动窗口固定采样 | 不抢占 PB12 |
| PC13 | Boot 不依赖该灯 | GPIO 输出 Push-Pull，低有效板载 LED；初始输出 SET（灭），需复用已测试的电平逻辑 |
| I2C1 | **不启用** | PB6/PB7，7-bit addressing，**100kHz**，AF Open-Drain；没有实测必要不切 400kHz |
| ADC1 | **不启用** | 通道 `ADC_CHANNEL_TEMPSENSOR`（内部 CH16），`TSVREFE=1`，12MHz ADC Clock，239.5 cycles sampling，软件触发，ADC calibration；每秒定长 16 次取平均 |
| FreeRTOS | **不启用** | Cortex-M3，使用 FreeRTOS Native API `xTaskCreate`/`vTaskNotifyGiveFromISR`，`heap_4`，三个自定义任务，**禁止多生成并运行一个 defaultTask**；抢占式调度，Tick 1000Hz；见第 11 章 |
| BKP/PWR | Boot 读取/清除 DR1/DR2/DR3 和 Metadata 关联状态 | App 只写规定的一次性 DR1/DR2；只读 DR3 |
| IWDG | **完整产品 Phase 8 才启用**，最终启动后负责喂狗直至跳 App | 不重复改变超时配置，只作接力刷新；产品构建在 Phase 8 后使用 |
| SWD/JTAG | SWD 保留，禁止占用 PA13/PA14 | 同 |

**已提供的 LED/I2C 配置参考（只读，不直接充当正式 App）：** 用户提供的 `i2ctest.ioc` 和 `i2ctest.uvprojx`。`.ioc` 明确包含 `PC13-TAMPER-RTC.Signal=GPIO_Output`、`PinState=GPIO_PIN_SET`，`PB6=I2C1_SCL`、`PB7=I2C1_SDA`、`I2C1.I2C_Mode=I2C_Fast`；CubeMX 6.17.0、STM32Cube FW_F1 V1.8.7。该实验 `.ioc` 的时钟记录为 **8MHz**；其 Keil Target IROM 覆盖 `0x08000000–0x0800FFFF`，**不是**正式 App `0x08004000–0x0800FFFF`。`i2ctest.uvprojx` 包含 `main.c` 和 HAL I2C/GPIO 实现，但本轮未上传 `main.c`，不能从工程配置推定 OLED 已经通过显示验收，也不能从 `.ioc` 的 I2C Fast 推定正式 72MHz/100kHz 配置已上板验证。正式工程仍以本表参数为准并单独构建/验收；**LED 板载存在与已点亮这一事实来自用户的真实测试说明**。

**时钟约束**：SYSCLK 72MHz 必须在 8MHz 晶振工作正常且 HSE/PLL Ready 之后才能使用；失败时保持安全停留，不能照 72MHz 计时却实际跑 HSI。启动选择和恢复路径必须覆盖 HSE 不就绪。HSE 固定使用板上 8MHz，LSE 晶振存在但**产品时间显示只采用系统 uptime，不依赖 LSE/RTC 日历**。LSI 用于 IWDG，必须做实测校准，不能用 LSE 存在替代 LSI 测试。

---

## 2. 地址空间、分区和持久数据所有权

### 2.1 内部 Flash 与 SRAM（固定）

| 区域 | 起始—末地址（闭区间） | 大小 | 约束 |
|---|---|---:|---|
| Boot Flash | `0x08000000–0x08003FFF` | `0x4000`=16,384B | **IAP 永远不得擦写**；Boot Vector at `0x08000000` |
| App Flash | `0x08004000–0x0800FFFF` | `0xC000`=49,152B | 唯一安装区；App Vector at `0x08004000` |
| SRAM | `0x20000000–0x20004FFF` | `0x5000`=20,480B | Boot/App 各自独立使用，不能把 20KB 当 32KB |

STM32F103C8 中容量内部 Flash 1KB/页，半字 16-bit 编程。App 正好占页 16～63；Boot 占页 0～15。内部不设置 Metadata 页，不使用 `0x0800FC00` 等旧方案。App 长度奇数时最后半字未使用的高字节补 `0xFF`，CRC 只取 `image_size` 有效字节。

**最终 Keil 链接配置**：Boot IROM1=`0x08000000,0x4000`；App IROM1=`0x08004000,0xC000`；两工程 IRAM1=`0x20000000,0x5000`；各自启用 startup vector、scatter/RO/RW/ZI 正确定位，并记录 MAP 实际最大 RAM 占用。必须在 App 启动早期及 Boot 跳转尾段分别设置 `SCB->VTOR=0x08004000`。

### 2.2 W25Q64（8,388,608 字节）固定分区

| 分区 | 起始—末地址 | 大小 | 写入者与用途 |
|---|---|---:|---|
| Download | `0x000000–0x00FFFF` | 64KB | Boot 下载 `32B header + app.bin` |
| Backup | `0x010000–0x01FFFF` | 64KB | Boot 存放旧版已确认 App **整 48KB 物理镜像** |
| Metadata A | `0x020000–0x020FFF` | 4KB | Boot 追加式 72B 状态记录 |
| Metadata B | `0x021000–0x021FFF` | 4KB | Boot 冗余、切区 |
| Upgrade Log | `0x022000–0x025FFF` | 16KB | Boot 追加重要升级和错误日志，不替代 Metadata |
| Reserved | `0x026000–0x7FFFFF` | 其余 | **所有产品模块禁止擦写** |

最大固件文件 `32+49152=49184B`，可以放入 Download 64KB。Backup 总共 64KB，但物理镜像**固定复制内部 App 的 49,152B**；CRC 必须同时覆盖 48KB 物理镜像和旧 App 的有效 `active_size`。`Metadata A/B` 每区 `floor(4096/72)=56` 条完整记录，尾部 64B 不用于记录。App **绝不初始化 SPI1、绝不写 W25**。Boot Store 负责 W25 **业务分区边界**，W25 原始驱动负责设备 **物理边界**；两重都要检查。

### 2.3 持久化信任顺序

- **唯一掉电持久权威**：W25 Metadata A/B 内通过 CRC/序列/语义审查的最新记录。
- **App 升级和确认请求**：BKP DR1/DR2 是一次性软件复位信号，不是掉电日志；DR3 是 Boot 发给 App 的 Trial 提示。
- **RAM/Debug Watch 值**、OLED 内容、普通串口打印、Upgrade Log 都不能替代受保护的 Metadata 状态提交。
- 无有效 Metadata 且没有明确授权的首次 Provisioning：**留 Boot 维护**，不得仅凭合法 App 向量自动起跳。

---

## 3. 固件格式、版本、CRC 和镜像判定

### 3.1 唯一 YModem 文件格式（32B + App）

按 **8 个小端 `uint32_t`** 序列化；禁止假定编译器结构体打包方式：

| 偏移 | 字段 | 强制规则 |
|---:|---|---|
| `0x00` | `magic` | `0x46574D47` |
| `0x04` | `header_version` | `1` |
| `0x08` | `firmware_version` | 非零，`uint32_t` 数值直接比较；**同版本可重刷，正常模式禁止低版本** |
| `0x0C` | `image_size` | 传输格式允许 `1…49152`；**可启动镜像必须 `≥8` 字节**以提供 MSP/PC，其他值拒绝 |
| `0x10` | `image_crc32` | App 有效字节 CRC32/IEEE |
| `0x14` | `target_addr` | 严格为 `0x08004000` |
| `0x18` | `reserved` | 必须 `0` |
| `0x1C` | `header_crc32` | 对前 28B 做 CRC32/IEEE |

`fw_pack.py` 输入为**App 的 `fromelf` 导出的 `.bin`**，必须确认镜像从 `0x08004000` 开始、文件真实长度不超过 48KB；若 `.bin` 内有 RO load 填充，CRC 按实际生成二进制文件字节计算。YModem Block0 十进制大小必须**严格等于** `32+image_size`；SOH/STX 最后一包的尾部填充不算有效文件字节，也不得写入镜像 CRC 范围。

**版本编码（本版统一约束）**：`firmware_version = (major << 16) | minor`，major/minor 为 `uint16_t`；演示 V1=`0x00010000`，V2=`0x00020000`。按无符号 32 位比较，`0` 非法；同版本重刷允许，回退版本只在人工 Recovery 的显式确认程序中授权。该编码是**本版新明确的协议决定**，不是宣称历史测试包已采用此编码。

### 3.2 CRC 算法

| 用途 | 参数 | 校验向量 |
|---|---|---|
| YModem Packet | CRC16/XMODEM：poly=`0x1021`，init=`0x0000`，RefIn/RefOut=false，xorout=`0`，高字节先发送 | `123456789 → 0x31C3` |
| Header、Image、Metadata、Log | CRC32/IEEE：reflected poly=`0xEDB88320`，init=`0xFFFFFFFF`，xorout=`0xFFFFFFFF`，RefIn/RefOut=true | `123456789 → 0xCBF43926` |

不能把 MODBUS CRC16、STM32 硬件 CRC 外设默认参数或其他家族硬件 CRC32 当成这些算法。

### 3.3 Image 验证顺序（必须完整）

1. 下载长度、Header 32B 的 magic/version/reserved/address/image_size/version/header_crc32 检查。
2. 对 W25 Download 的全部**有效文件字节**进行真实回读；验证 App 区 image_crc32。
3. 解析 App 第 0/4 字节：MSP 必须位于 `(0x20000000,0x20005000]` 且满足 8B 对齐；Reset 向量最低 bit 必须 1，`reset & ~1` 必须在 `[0x08004000,0x08004000+image_size)`；跳转地址不能落在未下载区域。
4. 版本准入通过，目标 Flash 区合法，既有可信 Active/Backup 安全策略满足。
5. **一次性持久提交** pending_version、pending_size、pending_crc32 与 `download_valid=1`；提交前不得开始旧 App 擦写。

---

## 4. USART1 DMA/IDLE、RingBuffer 和命令模式

### 4.1 数据通路和所有权

`PA10 USART1_RX → DMA1_CH5 Circular[512] → HT/TC/IDLE 进度快照 → RingBuffer[2048] → Mode Router → ASCII CRLF 或 YModem Parser`；TX DMA1_CH4 Normal。Ring 使用 16 位 head/tail、一格空区分空满，**有效容量 2047B**。中断/生产者是 DMA 进度收集器，消费端为 Boot 主循环或 App Task_UartRecv；禁止在两个 ISR 中独立更新 head 导致竞态。

- IDLE、HT、TC 都表示**有新字节的机会**，不表示“协议一帧结束”。每次从同一个 DMA `CNDTR` 位置计算增量；同一生产者串行消费并维护上次位置，重复事件不重复入 Ring。
- 在 115200 8N1 下，512B DMA 一整圈的理想时间约 44.4ms；若收集器来不及消费、无法排除已覆盖，置 `UART_RX_OVERRUN`，**废弃当前协议包/会话并显式回复错误**，不能静默当完整包 ACK。
- Ring 满、USART ORE/FE/NE、DMA TE 必须记录错误码，调用协议层明确重试/断开规则；ISR 不做全量 CRC、W25 擦写、OLED、长字符串处理。
- USART1 TX DMA TC 后，仍要等待 USART 硬件 `TC=1`，特别是执行 `@APP:UPDATE` 的软复位前。
- HAL IRQ 和寄存器级 IDLE/CNDTR 处理**只能存在一条完整路径**。相关寄存器访问封装在 BSP，不能让多个文件对同一标志重复清除。

### 4.2 ASCII 命令（全部 CRLF 结束、区分大小写）

| 当前运行镜像 | 固定输入 | 必须行为 |
|---|---|---|
| App | `@APP:VERSION\r\n` | 返回 `APP VER=<major>.<minor> MODE=NORMAL\|TRIAL\r\n` |
| App | `@APP:TEMP\r\n` | 返回 `APP TJ_EST_X10=<有符号整数> RAW=<0..4095> STATUS=OK\|ERR\r\n`，测量的是结温估值不是室温 |
| App | `@APP:UPDATE\r\n` | **仅已确认 NORMAL** 可投递 UpgradeCtrl；TRIAL 或升级/复位已进行中返回 `ERR BUSY\r\n`；受控 TX TC→写 DR1→软件复位；不直接接收 YModem |
| Boot | `@IAP:VER\r\n` | 输出 Boot 标识、已确认 active_version 或 UNKNOWN |
| Boot | `@IAP:STATUS\r\n` | 输出九状态、valid、attempts、错误码及关键版本 |
| Boot | `@IAP:ENTER\r\n` | 仅允许维护/可接收状态启动 YModem，先安全创建 RECEIVING 事务 |
| Boot | `@IAP:LOG\r\n` | 只读枚举有 CRC 的升级事件 |

**实现限制**：ASCII 行缓冲 **128B**（含 CRLF 和终止符）；超过 127 字节清空本行并回复 `ERR LINE_TOO_LONG\r\n`。识别合法完整 CRLF 后再分发；不允许用 IDLE 当行末；禁止无界 `strcpy` / `sprintf`，推荐有限 `snprintf` 或轻量固定字段编码。Boot 接收 YModem 期间忽略 ASCII 解析；YModem DONE/CANCEL/ERROR 后先复位 Parser/Router，再重新开放 CRLF。

---

## 5. 标准 YModem 业务协议（Boot Only）

**范围：单文件、标准 Sender、无断点续传。** 支持 Block0 文件名和十进制文件大小，SOH 128B / STX 1024B、block 序号及其反码、CRC16、`C`、ACK/NAK、重复包幂等、双 EOT 和终止空 Block0。Session FSM 与 Packet FSM 分离，不能用“接收一次 UART IDLE 就是一帧”的写法。

| 场景 | 行为 |
|---|---|
| Block0 | 只接受一个非空文件名（限制长度）与十进制合法文件大小；字段层必须在 `33…49184`B 范围且与后续 Header+App 严格一致；有效可启动 App 必须 `≥8B`，因此最终有效升级文件最低为 `40B` |
| 数据块合法 | 解析整块、验证编号/反码/CRC16；只将文件声明范围内的有效字节写入 Download；WIP 清除且写入错误条件已检查后才 ACK |
| 已 ACK 的重复块 | **可再次 ACK，但不得增加长度、不得重复推进存储偏移** |
| 未来编号/错误编号 | NAK 或有界失败；不接受跳号写入 |
| 错 CRC | 不写有效数据，不增长字节，不标 Download valid；NAK 重传 |
| DMA/Ring 丢数 | 当前包及必要的会话状态失效；不能假设后续字节仍正确对齐 |
| 双 EOT/空 Block0 | 完成**串口协议会话**，接下来仍必须 W25 全量回读、Header/CRC/向量验证，才能允许下载标记有效 |
| sender 取消/超时 | 退出接收，原已确认 App 不变；新 Download 标记无效，下次从零开始传输 |

本版的**通信默认参数**：单包等待 1000ms，连续异常包最多 10 次，启动握手最长 30s；所有超时记录为整数毫秒配置常量，必须由主机异常测试证明标准 Sender 兼容。超时值是实施默认值而非已有真机测量结论。擦写 W25 的时间包含在受控落盘流程中，不得因为 NOR 正在正常 WIP 就用 1s 包超时误判；正常块 ACK 必须等待本地落盘完成，然后才进入**下一个**等待窗口。

---

## 6. W25Q64 驱动、危险操作授权与日志

### 6.1 设备驱动 API 职责

| API | 执行契约 |
|---|---|
| `W25_Init / W25_ReadJedec` | 读取 JEDEC；历史记录 `EF 40 17`，但仍必须核对实物完整子型号、SR/保护位和对应官方时序 |
| `W25_Read / W25_ReadStatus` | 物理 8MB 防溢出范围校验；WIP/WEL 解码；任何退出路径 `/CS` 释放为高 |
| `W25_WriteEnable` | WREN 后验证 WEL=1；无 WEL 禁止编程/擦除 |
| `W25_PageProgram` | 每次 1～256B、不能跨 256B 物理页；验证前置擦除状态，WIP 限时轮询 |
| `W25_Write` | 合法缓冲拆成跨页物理操作；**不擅自帮业务层擦除陌生 sector** |
| `W25_SectorErase` | 必须 4KB 对齐、调用者已授权区域，WIP 轮询有上限，不允许无界 HAL_MAX_DELAY |
| `W25_Verify` | 关键写入后的回读字节/CRC；`HAL_OK` 不等于数据正确落盘 |

所有 **Flash/NOR** 擦写只能在显式用户批准的真机测试条件下执行。任何硬件写擦必须通过 BSP 物理范围 + Store 业务分区授权；错误码保留阶段和地址上下文。禁止从 Boot 正常启动路径自动触发破坏性自检。

### 6.2 W25 日志（16KB）

正式产品保留 `0x022000–0x025FFF` 的 16KB Upgrade Log。**升级日志只能追加可恢复的短记录，绝不修改 Metadata 决策**。为避免 ABI 含糊，本版规定日志记录为 **32B，8×u32LE**：`magic=0x4C4F4731, sequence, event_code, state, version, error_code, detail, crc32(first28B)`。每 4KB 日志扇区可容纳 128 条；4 扇区循环，写入日志 CRC 最后提交、回读验证。日志写入失败不得把已经成功的 Metadata 提交回滚成“未提交”，但应把可观察故障写入错误状态或串口；**为日志让位不得擦掉唯一可信 Metadata 或 Backup**。新的 32B 日志记录格式是本版确定的实现选择，非历史实验固有格式；日志格式变更需先处理旧记录的版本兼容或受控清区。

事件编码在代码中建立固定枚举，至少覆盖：START_DOWNLOAD、DOWNLOAD_VALID、BACKUP_VALID、INSTALL_START、INSTALL_OK、TRIAL_START、CONFIRM、ROLLBACK_START、ROLLBACK_OK、ERROR、POWERLOSS_RECOVERY。日志对外 `@IAP:LOG` 只读，遇损坏记录跳过并报告 `LOG_CORRUPT`，不凭日志推导有效镜像。

---

## 7. 72 字节 Metadata A/B Journal：单一可信状态源

### 7.1 持久记录的精确字节结构

**72B = 18 个 `uint32_t`，全部按 little-endian 显式编码**。协议兼容性不得依赖 C 结构体可能的 Padding。

| u32 索引 | 字段 | 规则 |
|---:|---|---|
| 0 | `magic` | `0x4D455432` |
| 1 | `sequence` | 有效记录单调递增；不以普通 `>` 处理回绕 |
| 2 | `state` | 下方数值枚举 |
| 3–5 | `active_version, active_size, active_crc32` | 已确认旧 App 的有效长度和 CRC |
| 6–8 | `pending_version, pending_size, pending_crc32` | 新候选 App 的有效长度和 CRC |
| 9–12 | `backup_version, backup_crc32, backup_size, backup_image_crc32` | Backup 48KB 物理 CRC、原镜像有效长度和 CRC |
| 13 | `download_valid` | 只能 `0`/`1`；与 Pending 三元组同一次提交 |
| 14 | `backup_valid` | 只能 `0`/`1`；与 Backup 四元组同一次提交 |
| 15 | `boot_attempts` | `0..3`，每次真实 Trial 放行之前先持久 `+1` |
| 16 | `last_error` | 稳定可追溯错误码，0 表示无错误 |
| 17 | `crc32` | 对前 68B 计算 CRC32/IEEE，**最后写入** |

本版固定九个持久状态的 on-flash 编码（与早期 `c06_metadata.h` 的本地 preflight 编码一致，但原文件曾明确声明**不属于已冻结 ABI**；现经本版设计规范固定为新产品 ABI）：

`CONFIRMED=1; RECEIVING=2; DOWNLOADED=3; VERIFYING=4; BACKING_UP=5; INSTALLING=6; PENDING_CONFIRM=7; ROLLBACK=8; FAILED=9`。`IDLE` 为运行时接收器状态，不写为持久第十态。

### 7.2 原子提交、扫描和切区

1. 未写记录槽必须**全 72B 为 `0xFF`**；存在非 FF 但校验不合法者标 DIRTY，禁止覆盖为新记录。
2. 写新记录先写 68B 业务字段（跨 256B 页正确拆分），最后写 CRC32 的 4B；读回 72B，magic/CRC/字段语义全部一致才可称 `COMMITTED`。**NAND 风格“写同一位两次修补”绝不能用于 NOR**。
3. A/B 全区扫描，比较具备有效 CRC、合法 state/size/valid/attempts 的记录；一次仅采用**唯一最新合法序列**。同序列内容不一致、版本跳变不可信或无法决定谁新→`FAILED`/维护，不自动猜测。
4. 满区时先擦备用区（擦前必须确认旧区至少含一条可信记录），在备用区写入新基线并完整回读，然后才可擦旧区。中途断电，至少旧区或新区应保留可恢复可信状态。
5. 为避免“接近回绕时随意比较”，本版规定：`sequence >= 0xFFFFFFFE` 时**停止继续提交并进入维护/受控恢复**，不允许在产品 V1 中自动回绕。这是明确 fail-closed 策略，不改变记录的 sequence 字段长度。
6. 所有事务必须**先提交可恢复的危险状态**，成功读回后才允许执行擦内部 App、擦 Backup 旧数据、切 Journal 区等风险动作。
7. 任一字段组合不自洽（例如 `PENDING_CONFIRM` 却 `download_valid=0`、`INSTALLING` 却无安全备份）不得按正常 App 启动处理；由恢复策略判定重装/回滚/留维护。

### 7.3 首次出厂 Provisioning（不能省略）

**问题**：W25 Metadata 空白时，仅仅烧入 `app.bin` 不构成可信启动。  
**唯一解决路径**：`FACTORY_PROVISION` 是**维护态受控工具流程**，由用户明确批准后：①用 ST-Link 受控烧录已审查的正式初始 App；②取得真实版本、size、CRC，完整读回 App；③将 W25 Journal 初始化为 `CONFIRMED(active_version,active_size,active_crc32)` 并验证；④记录 Boot/App/W25 备份校验与版本证据；⑤冷启动验证。首次 Provision 不在 Boot 正常开机中自动执行。它是首次部署动作，**不是内部 Metadata 页、也不是新增持久升级状态**。

Phase 0 尚未实现 Journal 的过渡方法：仅在名为 `PHASE0_TEST_ONLY` 的专用非量产构建中，对获批准的最小 App 固定 `image_size/image_crc32/vector` 白名单验收后做真实 Boot→App；该白名单不得进入正式 Phase 7/Release。**不能因为 HW05E 独立 SRAM 跳转已 PASS，就跳过产品 C07 真机验证。**

---

## 8. 九状态持久 IAP 状态机和确定性掉电恢复

### 8.1 状态含义/复位处理（唯一行为表）

| `state` | 必须保持的不变量 | 复位后的动作 |
|---|---|---|
| `CONFIRMED` | Active 有效；App CRC/向量受信 | 核对 Active 与实像后正常起跳，否则维护/必要恢复 |
| `RECEIVING` | Download 不可被标 valid；内部旧 App 未改 | 丢弃不完整传输，允许完整重下；旧 App 仍可可信启动但本次手工维护需有明确退出动作 |
| `DOWNLOADED` | 仅 YModem 会话结束 | 重新全包验证，不能因“下载完”直接安装 |
| `VERIFYING` | Header/Download 全量检查中 | 从 W25 重新检查 CRC/Header；未提交下载 valid 不可信 |
| `BACKING_UP` | 旧 App 必须仍在内部且有效；backup_valid=0 | 重新擦写备份允许区域、再完整备份+验证，绝不跳部分备份 |
| `INSTALLING` | 必须有可信 Download 和可信 Backup | **不跳内部 App**；先核实两个有效数据，再重新安装，或者根据恢复策略恢复 Backup；两者都无效留 FAILED |
| `PENDING_CONFIRM` | 已安装候选 App、有效镜像 CRC、0..3 Attempts | 首先审查符合条件的 DR2 确认；否则若 attempts<3 且镜像有效，再持久增加 Attempts 后 Trial；次数耗尽回滚 |
| `ROLLBACK` | 有可信 Backup 48KB 物理 CRC | 重新恢复全 48KB；完整回读 CRC 与向量后提交旧版 CONFIRMED；中途断电继续 Rollback |
| `FAILED` | 无可证明安全自动路径 | 保留 Boot，明确错误码，人工维护；不能“猜测 App 还能启动” |

`IDLE` 属于协议/运行期状态，不新增持久状态。**发生任何危险态复位时，恢复判断先于人工升级命令和 App 跳转。**

### 8.2 标准升级事务：顺序必须如下

```text
正常 App 接受 @APP:UPDATE
→ 协调业务、安全发送响应并等待 USART TC
→ 写 BKP_DR1 = 0x55AA；SYSRESETREQ
→ Boot 读取 Journal / 消费 DR1；进入受控维护
→ @IAP:ENTER；提交 RECEIVING + download_valid=0
→ YModem → W25 Download（保存 Header32 + image）
→ 提交 DOWNLOADED → 提交 VERIFYING
→ Header/版本/CRC/向量/W25 全包回读 PASS
→ 一条 Journal 原子提交 pending 三元组 + download_valid=1
→ 提交 BACKING_UP + backup_valid=0
→ 拷贝内部旧已确认 App 的全部 48KB 到 W25 Backup
→ 物理 48KB CRC 和有效像 active_size CRC 回读 PASS
→ 一条 Journal 原子提交 backup 四元组 + backup_valid=1
→ 提交 INSTALLING；回读有效后才动内部 Flash
→ 仅擦写页 16..63（按镜像所需页），安装 Pending image
→ 读回有效长度 CRC、向量/版本再次确认
→ 提交 PENDING_CONFIRM(boot_attempts=0)
→ 每次 Trial 放行之前持久 +1（最大三次）
→ Boot 设置 BKP_DR3 Trial 标识，净化外设/中断/时钟上下文
→ 跳 App @0x08004000
→ App 健康时间≥3000ms；Business 真正成功心跳≥5；ADC/OLED 至少成功一次
→ App 写 BKP_DR2=0x5AA5 并 SYSRESETREQ
→ Boot 校验 DR2 + PENDING_CONFIRM + Pending 与 Flash 全量 CRC
→ 追加 CONFIRMED，新 Active=Pending，清理请求，允许正常 App 启动
```

**新 App 安装不因 Trial 失败直接标记 Active。** Confirm 只有 Boot 在 W25 Metadata 内完成。若 Pending 无效/连续三次试运行未 Confirm，先进入 `ROLLBACK`，只从 CRC 可信的 W25 Backup 完整恢复原 48KB，再提交旧版本 `CONFIRMED`。任何阶段绝不把未完成安装的 App 放行。

### 8.3 下载和安装冲突消除

- 下载期间覆盖 Download 不触碰内部旧 App；如果下载中断，旧 App 仍可运行。恢复逻辑可由人工选择结束维护并重启旧 CONFIRMED App，但必须保留/恢复与 Journal 一致的已确认状态，不悄悄把 `RECEIVING` 当作 `CONFIRMED`。
- `BACKING_UP` 中**不擦内部 App**；Backup 直到双 CRC 回读正确后才设 valid。
- `INSTALLING` 中断：先看 Download/Backup 实际回读 CRC 决定重新安装或回滚；“App 看起来能跳”不是安全依据。
- `ROLLBACK` 中断：幂等地从 Backup 再写整 48KB，成功后才信任旧 App。
- `PENDING_CONFIRM` 内发生复位：先判断 DR2 是否为有效候选确认，其次处理尝试次数；第三次启动后完成确认仍必须允许成功。
- Journal 切区断电：新老扇区扫描、CRC/序列/语义必须能证明唯一权威；不允许因新扇区头几个字节像 magic 就认 valid。

---

## 9. Boot 启动策略、PB12、可信验证和跳转

### 9.1 启动优先级（完成 Phase 7/8 的产品逻辑）

1. 在可用的最小安全时钟下读取并记录 RCC Reset Flags；初始化 Recovery PB12 采样和 W25 非破坏性访问；不做任何自动擦 NOR 自检。
2. 检查 Journal 唯一最新有效记录；若安装/回滚等危险事务未完成，**立即按第 8 节进行恢复**，优先级高于 PB12 与常规升级命令。
3. 读取 PB12：按键在复位/上电后固定窗口稳定低有效 → 留 Boot 手动维护。人工 Recovery 也不得绕过危险恢复。
4. BKP_DR2 确认仅在 `PENDING_CONFIRM` 且 pending/version/CRC/attempts 自洽时有效；单次消费后清除；若有效，提交 CONFIRMED；DR1 则是 App 请求进入 Boot，消费后清除。
5. `PENDING_CONFIRM` 按 attempts 门槛尝试/回滚；只有严格允许时才跳 Trial。
6. `CONFIRMED` 先检查 active_version/size/CRC/向量，若有效则**正常直接跳 App，不等待升级超时**。
7. 无可信元数据、W25 不可用、App 不匹配或无法恢复→停留 Boot 的可诊断维护模式；不按未经授权的向量起跳。

**PB12 采样策略**：迁移并审查历史 P1/P1-D2 候选采样实现（隔离通过记录：D1 覆盖较广，D2B 仅有修复后松开+RESET 单次记录；不据此认定整体 4/4 PASS），以该实现的输入极性、时序、采样稳定条件作为正式代码基线；迁移时必须保留原始测试代码/串口证据和精确采样参数，不能在未核查源码时另造一个“20ms 全低”算法。若历史候选源码/配置暂不可访问，Phase 0 的 PB12 条目保持 NO-GO，不由 Codex 擅自填入时间常量。

### 9.2 Boot 跳转尾段的精确边界

调用前须由 Boot Policy 验证**可信** App 的版本、有效长度、CRC、向量，不由跳转函数仅依 MSP 合法决定信任。机械交接尾段必须：停止 DMA UART/SPI、释放 W25 CS、停止 SysTick、清/禁 NVIC 外部 IRQ（包括 pending）、处理系统异常挂起、确保没有活动异常上下文、恢复受控时钟/外设状态；配置 `SCB->VTOR=0x08004000`，DSB/ISB；恢复目标线程可接受的中断屏蔽状态；装载 App 初始 MSP，跳到 Thumb Reset_Handler，绝不返回 Boot 栈。不得在 `PRIMASK=1` 或 `FAULTMASK=1` 的状态启动依赖 FreeRTOS 的 App，也不能在 Hand-off 的尾段执行依赖已失效栈/旧外设的普通 C 逻辑。

**真实验收必须分三层记录**：①“向量非法可安全拒绝”PASS；②“HW05E 独立 CPU SRAM→App”已有 HARDWARE PASS；③“正式 Boot Policy→C07A_JumpConfirmed→App”仍 NOT RUN，只有 Phase 0 最小授权测试路径或 Phase 7 产品 Metadata 路径真实上板后才能分别验收。不允许烧写历史仅用于静态容量实验的真实 C04-B BootMain HEX。

---

## 10. BKP、Trial、健康确认、IWDG 与回滚

### 10.1 一次性 BKP 协议

| 寄存器 | 16-bit 值 | 单一写入者 | 规则 |
|---|---|---|---|
| `BKP_DR1` | `0x55AA` | App | UPDATE_REQ；先完成 UART TC 后写入并软件复位；Boot 消费并清除 |
| `BKP_DR2` | `0x5AA5` | App Trial | CONFIRM_REQ；**不等于已持久 CONFIRMED**；Boot 审查后清除/提交 |
| `BKP_DR3` | `0xA33A` | Boot | TRIAL_MARK；仅在 Pending Trial 放行前设置，正常 Confirmed 启动清除 |

读写须启用 PWR/BKP 时钟并按芯片手册管理 DBP 写权限。**不要求新增 VBAT 电池或跳线**；第一次实现时必须用当前核心板完成 DR1/DR2/DR3 写入、软件复位、Boot 读回及消费清除的真实测试。只在通过此门槛后，才将 BKP 用作可靠的跨软件复位握手；不依赖其在整板断电后保留任何值。掉电恢复与确认最终以 W25 持久 Journal 为准。

### 10.2 Trial 判定：统一 `≥3s` 与 `≥5` 次真实业务心跳

- Trial 启动时，App 在 FreeRTOS 调度器工作正常后建立 `start_tick` 和读取 DR3 的本次上下文，**Boot 在跳 App 前已经持久累计 attempts**。
- `Task_Business` 用 `vTaskDelayUntil(500ms)` 每周期执行有界进展（包括业务状态更新和板载 PC13 LED 翻转）；成功后对**单调递增**的 `business_ok_seq` 加一；不能单纯系统 Tick 增量或 UpgradeCtrl 自增。ADC 与 SSD1306 OLED 每两个周期（约 1s）完成**真实成功**一次，供健康状态观察。
- `Task_UpgradeCtrl` 在 Trial 中需同时满足 `elapsed_ms ≥ 3000`、`business_ok_seq_delta ≥ 5`、`ADC_ok_count ≥ 1`、`OLED_ok_count ≥ 1`、UART RX 子系统 Ready、无致命错误、看门狗接力正常，才能发完必要的串口确认消息并等待 USART `TC`，随后向 BKP_DR2 写 `0x5AA5` 并软复位。
- OLED 不 ACK、ADC 永久超时、Business 停止提交进展、串口致命错误等不得写 DR2；Task_UpgradeCtrl 不应因为自身还在正常调度就认为 App 健康。正常模式 OLED 轻微错误可以记录并显示可行的降级诊断，但**Trial 必需健康项失败必须拒绝确认**。
- 3s 是**最小观察时间**，不是 3s 到点必须硬确认。允许更晚地确认，但每次 Trial 放行、超时/复位都受 Boot attempts≤3 约束。

### 10.3 IWDG 真机约束

- 最终产品要求约 4s IWDG；由 Boot **一次启动**并交给 App 接力。具体 WDG_SW Option Bytes 和 LSI 实际频率以真机测量为准；**不能为了省测试直接假定 LSI 恰好 40kHz**。
- 产品算法采用 IWDG Prescaler=64，装载 `reload = clamp(round(F_LSI_measured*4/64)-1,0,4095)`；编译期禁止仅因为数值落在范围内就认定 4s 真机通过。启动、最短超时裕量和异常路径必须实测；不写 Option Bytes 来强制适配。
- Boot 长 W25/Flash 操作必须每段有界并依据真实进度更新喂狗资格；到达错误上限后明确 FAIL，而不是无限刷新掩盖卡死。
- App 在 FreeRTOS 启动前先进行**短时安全喂狗交接**（不可依赖尚未调度的任务）；运行后仅 `Task_UpgradeCtrl` 在主要任务真实进展正常时刷新。IWDG 开始后不能再停止；App 不重新改变其配置。
- “每 2s 喂一次”仅为应用调度**最大期望周期**；真正刷新周期必须在**实测最短 IWDG 超时**内留有充分裕量，`Task_UpgradeCtrl` 可用 250ms 周期监测健康/刷新但不能无条件喂狗。业务健康标准不能要求空闲 UART 非得收到字节。
- 当前尚未完成产品 IWDG/LSI 验收；**Phase 0～7 的测试构建不得伪装成已完成最终看门狗闭环，Phase 8 统一启用并完成真机接力**。

### 10.4 回滚条件

一次 Trial 放行前先持久 attempts+1，最多 3 次。收到有效 DR2 后 Boot 必须先验证 Journal Pending 和内部 App 全有效镜像 CRC，满足后才提交新 Active=Pending 的 `CONFIRMED`。无确认且 `attempts==3`，或 Pending 镜像被破坏时，必须先验证 Backup 48KB 物理 CRC/旧有效像 CRC，写入 `ROLLBACK` 危险状态并从备份恢复**全部 48KB**；读回验证并提交旧 `CONFIRMED` 后才允许再次正常跳 App。若 Backup 也失效→`FAILED` 停 Boot。

---

## 11. FreeRTOS Application：SSD1306 智能温度仪表盘

### 11.1 三个任务的最终职责和优先级

| Task | 优先级 | 初始栈（32bit words） | 必须完成的工作 |
|---|---|---:|---|
| `Task_UartRecv` | High=3 | 256 | 2KB Ring 消费、128B CRLF Shell、`@APP:VERSION/TEMP/UPDATE`、向 UpgradeCtrl 通知，不得直接进行复位 |
| `Task_UpgradeCtrl` | Medium=2 | 192 | 处理升级请求，独占 DR1/DR2 写与受控 SYSRESETREQ；Trial 健康进度监测与有条件 IWDG 接力 |
| `Task_Business` | Low=1 | 192 | 每 500ms 业务进展与 PC13 板载 LED；每 1s ADC1 片内结温采样、SSD1306 OLED 帧刷新、uptime/health 统计 |

`configTICK_RATE_HZ=1000`；`configTOTAL_HEAP_SIZE=6144B`（**首轮编译候选，RAM 不足时须通过真实高水位审计调整，不能删功能**）；`heap_4`；默认 `xTaskCreate` 动态任务创建。**上述三任务的栈/TCB 已从 FreeRTOS heap 分配，不得在 SRAM 预算中再加一遍。** 暂定 API 优先级 3/2/1 为 FreeRTOS 任务逻辑优先级（不是 Cortex-M NVIC IRQ 优先级）；ISR `FromISR` 的真实中断优先级值必须按实际 FreeRTOSConfig.h 验证，不能混用。

### 11.2 ADC1 通道 16 温度采集协议

1. 使能内部 `TSVREFE`、按器件文档等待传感器稳定；`HAL_ADCEx_Calibration_Start(&hadc1)`（以生成工程中真实可链接的 STM32F1 HAL API 原型核对后调用）校准 ADC；ADC 时钟 12MHz、239.5 cycles → 采样窗≈19.96µs，满足 F103 温度通道所需采样要求。
2. 任务每 1000ms 通过**16 次有限超时的软件触发 ADC 采样**取整型平均值 `raw`，`0…4095`；转换超时、HAL 错误、明显超界/失效必须设置 `TEMP_ERR`，不伪造有效温度。
3. 产品约定先按板卡 VDDA≈3.3V 估算：`Vsense = raw*3.3/4095`，`Tj_C≈25+(1.43−Vsense)/0.0043`，对显示统一转换 `int32_t temp_x10`，小心单位、符号和溢出；**这是芯片结温估算，不是精密室温计**。
4. 不使用其他系列 STM32 的 `TS_CAL_1/TS_CAL_2` 地址；不私自把校准结果存进 W25 Reserved 或内部 App Flash。App `@APP:TEMP` 同时显示原始 ADC、估算和状态，便于解释测量偏差。

### 11.3 SSD1306 128×64 固定显示协议

| 项目 | 唯一设计值 |
|---|---|
| 控制器/接口/尺寸 | **SSD1306 / I2C / 128×64（1bit）** |
| I2C 控制器和引脚 | I2C1；SCL=PB6、SDA=PB7 |
| 设备地址 | **7-bit=0x3C**，STM32 HAL `DevAddress=0x3C<<1=0x78`；`0x78` 不是 7-bit 地址 |
| 总线频率 | **100kHz**，上拉至 3.3V，禁止 5V 上拉 |
| 控制字节 | `0x00`=命令流，`0x40`=显示数据流；不得把它们当 SSD1306 寄存器地址概念混淆 |
| Framebuffer | 固定 `uint8_t frame[1024]`，8 pages×128 columns，**只分配一份** |
| 寻址 | Horizontal addressing，列 `0..127`，页 `0..7`；初始化及每帧必要时发送 `0x20/0x21/0x22` 对应窗口命令 |
| 发送策略 | `HAL_I2C_Mem_Write(&hi2c1,0x78,0x40,I2C_MEMADD_SIZE_8BIT,...)`，**每次 32B 显示数据**，1024B 分 32 次，使用 horizontal addressing 连续推进列/页；初始化命令可同法使用 0x00 控制字节 |
| 周期 | 正常/Trial 1Hz，有界事务超时；只由 Business/Display 单一所有者操作，其他任务通过共享状态请求显示 |
| 故障处理 | 每次 I2C 事务最多 **20ms**，失败立即结束本次刷新并置 OLED_ERR；严禁卡在 `HAL_MAX_DELAY`，需验证 F1 I2C BUSY 勘误恢复逻辑 |

SSD1306 选定了；**不再把 SH1106 等其他芯片当候选**。显示实现要保证 6×8 ASCII 字模水平最多 `floor(128/6)=21` 字符；V1 文字仪表盘四行的顶点使用 y=0/16/32/48；V2 在同一 framebuffer 中按 §14.2 分配文字与趋势图区，绘图不覆盖必需的版本/温度/健康信息；超长文字显式截断，不能越界写 framebuffer。Boot 不编译任何 OLED 驱动，不能在 Boot 下载/回滚中实时更新屏幕。

**V1 文字布局与 Trial/升级提示示例（版本文字由构建常量产生；V2 正常页另须满足 §14.2 的真实趋势显示）：**

```text
NORMAL（App 正常）       TRIAL（新 App 首次试运行）
APP V2 NORMAL          APP V2 TRIAL
MCU Tj~31.5C           MCU Tj~32.0C
UP 00:12:34            HEALTH 5/5  3.0s
RX 123 IWDG OK         WAIT BOOT CONFIRM

升级前（仍由 App 显示）  Boot 工作期间（不刷新 OLED）
ENTER BOOT MODE        OLED 保留末帧或熄屏
UART TX FLUSH          通过串口 @IAP:STATUS/@IAP:LOG
RESTARTING             查看下载、安装、回滚进度
```

回滚后的旧 App 只显示确实知道的旧版本、NORMAL，除非已经有**可信的跨镜像事件记录**，不能伪造“刚刚发生回滚”的原因或时间。

### 11.4 App 上电/升级/确认顺序

```text
Reset_Handler → SystemInit → App VTOR=0x08004000
→ HAL/72MHz 时钟及必要 BSP 初始化
→ 如 IWDG 已启动，则在调度器前及时刷新（不重新配置超时）
→ 初始化 UART RX DMA/Ring、BKP（读取 DR3）、ADC1、I2C1/SSD1306
→ 创建 3 Tasks、通知/队列，启动 FreeRTOS 调度器
→ Business 真正运行并提交 heartbeat；1Hz 结温+OLED
→ 若非 TRIAL：显示 NORMAL，串口接受 VERSION/TEMP/UPDATE
→ 若 TRIAL：3s/5成功业务心跳/ADC/OLED/UART/狗健康全达标
→ UpgradeCtrl 等待必要串口 TC → 写 DR2=0x5AA5 → SYSRESETREQ
→ Boot 复核 Pending 和 CRC，Journal 按提交与回读契约持久化 CONFIRMED（不宣称 NOR 硬件原子性已验证）
```

**升级请求仅允许 `NORMAL` 且当前没有未决升级事务；TRIAL 时拒绝 `@APP:UPDATE`**。UARTRecv 只解析命令并通知 UpgradeCtrl；UpgradeCtrl 请求 Business/Display 单一 OLED 所有者显示 `ENTER BOOT MODE` 并有界等待结果（不直接调用 I2C/SSD1306），限制新请求、等待日志/TX 完成，检查系统状态，写 `BKP_DR1=0x55AA`，执行软复位；更新会话真正发生在 Boot，而不在 FreeRTOS App 中运行 YModem。

---

## 12. SRAM、Flash、编译选项、质量门禁

### 12.1 硬件不可逾越的大小

| 目标 | Flash 绝对上限 | SRAM 绝对上限 | 审核口径 |
|---|---:|---:|---|
| Boot | **16,384B** | **20,480B** | `.axf` 实际 Load image / RW initial load + `.map` 及 MSP 栈安全预算 |
| App | **49,152B** | **20,480B** | 从 `0x08004000` 起的真实链接装载范围 + FreeRTOS 动态堆/最坏任务/中断栈 |

历史 C04-B 15,596B、另一候选约 15,768B 是**静态容量结果**，并不是已实现全部正式产品的保证，严禁把它们作为 Release 固定体积。Boot 14KB 是主动优化预警阈值，不是允许删回滚/看门狗/Metadata 的理由。

### 12.2 App SRAM 预算（避免重计）

| 组成 | 首轮预算 | 实际归属 |
|---|---:|---|
| RX DMA | 512B | 静态 .bss |
| Ring | 2048B | 静态 .bss |
| SSD1306 framebuffer | 1024B | 静态 .bss |
| ASCII Shell | 128B + 有界 TX 缓冲 | 静态 .bss |
| ADC 状态 | 数十 B | 静态 .bss |
| FreeRTOS heap_4 | **6144B** | 静态 heap 数组，**包括 xTaskCreate 的三个任务栈/TCB** |
| HAL 句柄、队列、全局业务状态 | MAP 实测 | `.data/.bss` 或 FreeRTOS heap 内（按真实创建方式） |
| 中断/主栈 MSP | 候选 **1024B** | linker/startup 保留并审查最大调用链 |

任务初始 256+192+192=**640 words=2560B**，包含于上面的 6144B heap_4（不是另加 2560B）；高水位 `uxTaskGetStackHighWaterMark`、`xPortGetMinimumEverFreeHeapSize` 等用于最终调优。由于 Keil/FreeRTOS HAL 状态、调试库及额外缓冲不可仅凭本表得出终值，**只有真实 MAP+运行时证据同时证明 `<20,480B` 才算 PASS**。

### 12.3 编译器、Warning 和代码隔离

- Keil MDK **V5.37**，**ARM Compiler 5.06 update 7 build 960**，优先使用经真实审查的 O3/Size 构建选项；**禁止 ARM Compiler 6 专属 `-Oz/-Os`**。
- `0 Errors` 是硬门槛；所有 Warning 逐条登记/修复或评审接受。对于 ARMASM 自动加 2B padding 的 A1581W，须核对实际入口/向量/对齐机器码，不允许假装没有 Warning。
- O0 正常/O3 异常先查 UB、buffer 生命周期、HAL handle 状态、中断竞态、链接器/反汇编和内存越界；不能仅改 `volatile` 或永久降 O0。
- `L6218E Undefined symbol` 首查 `.c` 是否**真实加入 Keil Target Source Group**、Exclude from Build、函数签名、`static` 及链接条件；`#include` 路径能找到头文件不代表实现已参与构建。
- CubeMX `Core/` 和 ST 官方 `Drivers/` 属于生成/官方代码，Codex 不重写非 USER CODE 区；项目驱动、协议、策略进入 `User/`；禁止把实验擦写、容量 probe 代码带入最终镜像。
- 每 Phase 真实保存 Build log、`.axf/.map`、Flash/RAM 账本、Warning 清单、主机测试结果、Git Commit 和真机验收证据，失败记 FAILED/OPEN，不伪装 PASS。

---

## 13. 正式仓库结构：只有三份核心文档

```text
IAP_PROJECT/
├── AGENTS.md                  # 唯一根开发约束
├── doc/
│   ├── DESIGN_SPEC.md         # 唯一产品规格，含完整 CubeMX/Keil 基线
│   ├── ENGINEERING_LESSONS.md # 历史经验与风险
│   ├── REFERENCE_REUSE_AND_V1_V2.md # 辅助导航与演示方法
│   └── 厂商手册及硬件资料
├── IAP_REFERENCE/             # 只读历史工程，不参与正式链接
├── Bootloader/                # 用户 CubeMX 生成；当前不预建
│   ├── Bootloader.ioc
│   ├── Core/{Inc,Src}/
│   ├── Drivers/               # 官方 HAL/CMSIS
│   ├── MDK-ARM/
│   └── User/{BSP,Drivers/W25Q64,Protocol,IAP}/
├── Application/               # 用户 CubeMX 生成；当前不预建
│   ├── Application.ioc
│   ├── Core/{Inc,Src}/
│   ├── Drivers/               # 官方 HAL/CMSIS
│   ├── Middlewares/           # 官方 FreeRTOS
│   ├── MDK-ARM/
│   └── User/{BSP,Drivers/SSD1306,App}/
├── Shared/                    # 共享 CRC/Header/Ring 等纯算法
├── Tests/{host,hardware}/     # 隔离测试，不进入 Release
├── Tools/                     # 打包、主机测试、容量审计
└── Evidence/                  # 有实际构建/验收产物后按需建立
```

以上为目标结构，不表示目录已创建；本轮只维护既有文档和 Git 忽略配置。Boot/App 由用户 CubeMX 创建，其余模块目录在对应 Phase 实施时按需创建。

- 历史 `C07_PB12_P1_D2_W25_READONLY_NO_JUMP_TRIAL`：用于核对 `.ioc`、HAL/SPI、PB12 和系统引脚配置，**不复制诊断 main.c 做正式入口**。
- 历史 `G4D_OPT2B3_OPT3_Candidate 04B_TRIAL`：仅作容量/策略/Journal/算法参考；**真实 BootMain 未完成硬件运行验证，禁止直接烧录历史实验 HEX**。
- `HW05C/HW05E`：CPU/VTOR/MSP 的历史真机证据；不是新的产品 Boot/Main，也不能替代 FreeRTOS App。
- 新正式双工程由用户用 CubeMX 分别生成，Codex 先审查 `.ioc/.uvprojx/scatter`、给出一份确切的配置表，检查点 B 一次集中审核通过后再加入当前 Phase 用户源文件；`main.c` 保持薄入口。
- W25 设备驱动**只有一个权威 `.c/.h` 实现**；App 不加载；抽出共享 CRC 时不得在 Boot 和 Shared 分别留下两份可链接实现。

---

## 14. Phase 0～9：锁定顺序、实施任务和退出门槛

> **重要区分**：Phase 0 使用最小测试 App 和隔离的编译期 CRC 准入，只为了真实验证 Boot→App 和基础引脚；第 7 阶段实现持久 Metadata 产品信任链；第 9 阶段整合真正的 SSD1306/ADC/FreeRTOS 仪表盘。不得将这些阶段合并或提前宣告成功。

| Phase | Codex 必须完成的代码/离线工作 | 用户负责的 CubeMX/真机验收证据 | 退出判定 |
|---:|---|---|---|
| **0** | 创建干净 Boot+App 工程、固定 scatter/VTOR，受控 `PHASE0_TEST_ONLY` 的 CRC/向量白名单，最小 Jump/Recovery | 用户配置 `.ioc`；Flash Boot/App 分区；PB12+NRST、外接 TTL、BKP 跨软件复位功能验证、W25 JEDEC 初检；**真实 Boot→App** | Boot 实测跳 App、坏向量阻断、按键留 Boot、Boot 16KB/App 48KB/MAP 可查；HW05E 不能代替 |
| **1** | USART1 普通串口 + 2KB RingBuffer；错误和溢出处理 | 外接 USART1，随机长短输入/溢出和 TX TC | 无静默丢数，有错误状态与自动恢复 |
| **2** | RX 512B Circular DMA HT/TC/IDLE 增量收集 + TX DMA CH4 | 10,000 次主机模型随机边界与真机高速长流/错误注入 | 不重复/遗漏/误判 IDLE，覆盖风险可观测 |
| **3** | `@APP` / `@IAP` CRLF 命令 + Router | 半包、粘包、超长、模式独占、无错误跨模式消费 | ASCII/YModem 互斥，无命令注入与越界 |
| **4** | 标准 YModem Block0/双 EOT、CRC16、CRC32、Header32、PC fw_pack.py | 标准 Sender 正常/错 CRC/长度/序号/重传/超时/取消 | 一致的 32B Header、CRC 向量、完整接收校验 |
| **5** | 内部 Flash IAP MVP、App 区独占写入、回读 CRC | 用户受控烧录/擦页；越界写 Boot 必须被拒绝 | 永不擦写页 0..15，半写不盲跳；**不冒充有备份的最终产品** |
| **6** | W25 Download+Backup、物理 48KB CRC+旧有效像 CRC | W25 跨页满包、故障注入、双 valid 提交门槛 | 旧镜像备份正确，所有越界/受保护区操作拒绝 |
| **7** | 72B Journal A/B、首次可信 Provisioning、完整 Boot Policy/掉电恢复 | DIRTY、切区、并发断电、所有危险状态复位、真实 C07 可信放行 | 任意中断不得信任半写镜像或丢失唯一可证实记录 |
| **8** | Trial 3次/Confirm、IWDG 约4s 实测接力、Rollback、32B 日志 | 第三次成功、失败三次回滚、看门狗停喂/LSI 真机、日志及掉电 | CRC 可信才确认/回滚；长操作可喂、异常可恢复 |
| **9** | FreeRTOS 三任务、ADC 结温、**SSD1306**、App Shell、健康监督、端到端压测 | OLED 100kHz/0x3C/刷新、`@APP:TEMP/UPDATE`、V1→V2→确认或回滚、HSE/ADC/OLED 失败注入 | 48KB App/20KB SRAM/Boot 16KB 真链接；全链路证据齐全 |

**阶段流程**：先完成 §19 检查点 A，由用户一次配置并生成两个 CubeMX 工程；检查点 B 集中审核通过后锁定配置。每个 Phase 只实施一个有边界的模块组，按“设计审查→代码实施→主机测试→ARMCC5 构建→Warning/MAP 审计→用户真机验收→阶段确认”执行；后续不按模块零散增加外设。没有全部退出证据，下一 Phase 只可进行明确标记为实验性的*离线预研*，不能进入正式验收。原 App 端 A～F 业务包归入 Phase 9 的实施子步骤，不创建另一套会混淆名称的正式 Phase。

### 14.1 Phase 0 必须执行的细化顺序

1. **先归档**用户确认的历史 C07 诊断工程、C04-B 候选源码和 HW05C/HW05E 证据；不在原目录继续开发，不烧 C04-B HEX。
2. Codex 核对用户已有 CubeMX/HAL 版本、引脚和 Keil 编译器配置；输出 Boot `.ioc`/App `.ioc` 两份精确配置表并指出需用户操作部分。
3. 用户在新的 `Bootloader/`、`Application/` 各自创建 CubeMX `.ioc`，分别生成 Keil MDK5 项目，核对 SysTick 与 App TIM2 HAL 时基、Core、Drivers、scatter、App VTOR。
4. 先按 §19 检查点 B 完成两工程集中配置审核、必要启动/链接基线集成；审核 PASS 后 Codex 实现薄入口、独立启动/向量检查、跳转尾段、PB12 采样、只读 W25 JEDEC、串口最小输出，所有危险路径默认不执行；将 `PHASE0_TEST_ONLY` 与 Release Target 物理隔离。
5. Codex 完整 ARMCC5 Rebuild，证明 Boot ≤16KB、最小 App ≤48KB、RAM 未越界；检查 AXF/MAP 的入口地址、测试开关和所有 Warning。
6. **先备份**板上全 64KB Flash 与安全相关状态，按用户批准的页面范围分别烧入 Phase0 Boot 与最小 App；App 通过编译时已知 size/CRC 和向量才允许受控真实 Boot→App，坏镜像不能误跳。
7. 用户验收 PB12+NRST、正常冷启动直跳、无/坏 App 留 Boot、USART1 USB-TTL，留有完整串口证据、MAP、回读哈希与测试版本；未符合任一项不宣布 Phase 0 PASS。

---


### 14.2 Phase 9 两个 Release App 固件及独立故障演示（新增交付约束）

- **`APP_V1_RELEASE`**：`firmware_version=0x00010000`，完整 FreeRTOS 三任务、SSD1306/ADC 结温仪表盘、PC13 LED、USART Shell、BKP Trial/Upgrade/IWDG 接力；UI 明确显示 `APP V1 NORMAL`。
- **`APP_V2_RELEASE`**：`firmware_version=0x00020000`，使用**同一套 App 基础驱动和协议**；相比 V1 必须实现一项真正的业务变化：以业务任务每秒获取的片内结温估值维护**最多 60 个 `int16_t temp_x10` 历史采样**（120B 静态上限，异常采样不写入有效趋势），在 SSD1306 128×64 上绘制有界的温度趋势折线，并同时保留版本/温度/健康状态显示。UI/串口/Header 版本一致；不能只改文字或假日志冒充升级。
- 两份正式 App 分别输出从 `0x08004000` 开始的 AXF/MAP/`fromelf` BIN、Header32 + BIN 标准 YModem 文件、版本/长度/CRC32/SHA 和构建记录；均独立满足 48KB App/20KB RAM 限制。固件 Header 的版本编码和所有安全/健康条件仍按第 3、8、10、11、12 章，**不新增协议或改变 ABI**。
- 为证明失败回滚，允许在**独立 Test Target** 产生 `APP_V2_HEALTH_FAIL_TEST`（采用当前 V2 源码、故障注入阻止真实业务健康条件达成）。禁止进入 Release，禁止在未经备份回读及用户授权时用于真机。它是测试变体，不是第三个正式产品版本。
- **正常升级场景**：V1 经过受控 Provisioning 处于 CONFIRMED → `@APP:UPDATE` → Boot 接收 V2 → Download/Backup/Install → V2 Trial/BKP_DR2 → Boot 提交 CONFIRMED → OLED 显示 V2、串口版本和 Flash CRC 均与 V2 一致。
- **故障回滚场景**：必须从**独立恢复的 V1 已确认基线**开始，安装 V2 健康失败 Test 镜像 → 三次 Trial 限额 → W25 物理 Backup 恢复 V1 48KB/CRC/Metadata；不能在 V2 已正式确认后仍声称其 Backup 必须是 V1。
- **掉电恢复场景**：从可信 V1 开始分别在 Download、Backup、Install、Trial、Rollback 注入中断，按 Journal 和真实 CRC 检验恢复；所有危险真机操作由用户执行。
- 以上均为 **Phase 9 交付/验收子项**，不改变 Phase 0～9 顺序。Phase 0 的最小测试 App 不是 V1 Release。模块参考导航见 `doc/REFERENCE_REUSE_AND_V1_V2.md`，它不替代本文。

## 15. 必备安全/故障验收清单（跨所有阶段）

**镜像完整性**：Header 魔数/版本/长度/CRC/保留字段/目标地址；MSP/PC 范围、向量 Thumb；大小最大 49152；偶数/奇数；同版本重刷；未授权降级失败。  
**DMA/协议**：2KB Ring 满/越界、HT/TC/IDLE 重叠、DMA 512B wrap、串口 ORE、YModem 错序号/重复 ACK、标准双 EOT/Block0/取消、CRLF 半包/粘包。  
**W25/Journaling**：JEDEC 与保护位、片选复位高、WREN/WIP、跨 256B page/4KB sector、72B 跨页最后 CRC 提交、双 A/B 切区/CRC/序列冲突、RDP/保护边界。  
**事务安全**：下载时断电（旧 App 安全）、备份中断（valid=0）、安装首次页擦后断电、半写、首 Trial 掉电、确认请求后复位、回滚半写，均不得误跳半写 App。  
**Trial**：早于 3s/少于 5 业务进展/ADC 或 OLED 未成功/FreeRTOS 卡死不能确认；第 1～3 次 Trial 均计数，第三次有健康确认可通过；三次后恢复旧 48KB。**BKP**：首次使用前证明上电持续条件下跨软件复位请求可读/可消费；不得将 BKP 当掉电持久存储。  
**IWDG**：真实测量 LSI、启动与 App 接力、长事务不意外过期、故意不喂狗触发复位，Boot 正确判断复位原因且不误伤已经 CONFIRMED 的固件。  
**显示业务**：SSD1306 7bit 0x3C 和 HAL 0x78 分开，128×64 framebuffer 有界，100k I2C 无无限等待；温度仅结温估算，不误称精确室温计，串口 `@APP:TEMP` 与屏一致。  
**构建约束**：ARM Compiler 5 O3 真机回归，Boot 16KB/App 48KB/SRAM20KB 边界，FreeRTOS heap 动态任务栈不重计，Release 不含实验写擦/测试白名单，全部 Warning 可追溯。  
**证据链**：源码提交哈希、CubeMX `.ioc`、AXF/MAP、编译日志、主机单测、上板前后备份/地址校验、串口/OLED 现场与断电恢复日志。

---

## 16. 不能凭已有材料宣称已经通过的事实：明确关口

| ID | 用户已明确的条件/历史事实 | 必须在何时怎样关闭 | 对当前开发的影响 |
|---|---|---|---|
| H01 | STM32F103C8T6 核心板固定；SSD1306 OLED 固定；PC13 板载 LED 和 BOOT0 板级实现已由用户确认 | Phase 0 仅完成 PB12/W25 和复位等实际需要的连接；Phase 9 OLED I2C ACK `0x3C` 与显示实测 | 不更换核心板、不另接 LED、不增加 BOOT0 下拉 |
| H02 | PC13 板载 LED 已存在且用户提供通过测试的工程配置；`.ioc` 将 PC13 配置为 GPIO_Output，初始高电平 | 正式 App 再次进行 LED 低亮高灭及 500ms 心跳显示回归 | 历史实测视为板卡有效事实；不将未提供的测试 `main.c` 内容或波形编造成证据 |
| H03 | 板载 USB D+/D− 不承担本项目 UART IAP 数据通信 | **无本项目开发检查任务**；USART1 PA9/PA10 走外接 3.3V USB-TTL | 不引入 USB CDC、USB GPIO 操作或无关 D+ 测试 |
| H04 | W25 `/CS` 曾在复位停机时 1.3V，后来同状态记录 3.1V；不能由一次数字证明永远可靠 | Phase 0/6 根据外拉电阻及重复复位/冷启动测高、测 W25 JEDEC | 未达标时禁止 W25 危险写擦 |
| H05 | BOOT0 已在核心板实现；VBAT 的板上供电结构不单独设设计任务 | Phase 0/首次 BKP 功能使用前，用 DR1/DR2 跨软件复位读回验证；仅失败时调查备份域供电 | 不增加 BOOT0 硬件、不安排无关 VBAT 改线；整板掉电依赖 W25 |
| H06 | W25 仅已知 JEDEC `EF4017`，具体包装后缀/保护位尚需真品核对 | Phase 0/6 写擦前核对实际芯片型号手册并只读确认 SR | 不修改分区，未知器件禁止写擦 |
| H07 | 产品 `C07A_JumpConfirmed()` 未真实走通 | Phase 0 白名单测试路径、Phase 7 正式 Metadata 路径分别验收 | HW05E 已 PASS 仅证明 CPU 级交接 |
| H08 | IWDG 目标 4s 依赖 LSI 测频/Option Bytes；实际产品接力仍未闭环 | Phase 8 按第 10 节和指定最坏操作注入测试 | 不把候选窗口写成已测的绝对定时 |
| H09 | App 三任务 FreeRTOS+SSD1306+ADC 的真实 Flash/SRAM 尚无最终 AXF/MAP | Phase 9 真实 Release 链接、栈高水位、OLED 断线和温度误差验证 | 不宣称 27KB App 只是历史估算已通过 |
| H10 | 本版固定了新版日志 ABI、版本编码、ADC/OLED 参数以及 Journal numeric ABI | 首次正式构建使用本版测试向量；若已有旧版持久数据先受控清理/迁移，不盲目按新格式读取 | 属于实施规格选择，**不是历史测试已经验证** |

这些不是“让 Codex 自己猜”的设计选项：**本文明确选定硬件/协议/接口值，并规定实际器件或电气条件与选择不符时停止相应的上板操作，回到用户确认后调整**。仅剩需要真实证据关闭的关口，而不是一堆未决软件设计。

---

## 17. 官方技术依据和代码参考的使用规则

本项目长期仅保留三份核心管理文档：**`doc/DESIGN_SPEC.md`（本文）、`doc/ENGINEERING_LESSONS.md`、仓库根目录 `AGENTS.md`**。技术参考导航 `doc/REFERENCE_REUSE_AND_V1_V2.md` 只提供可复用思想/源码线索，不是第四份权威规格；器件手册、HAL、FreeRTOS 与源码同样不是附加的项目规格。

- STM32F103C8T6：ST **DS5319**（电气/温度特性与引脚）、**RM0008**（RCC、GPIO、USART、DMA、SPI、I2C、ADC、BKP、IWDG）、**PM0056**（Cortex-M3 异常/VTOR/MSP）、**PM0075**（内部 Flash）、适用中容量 Errata；按官方文档版本和实物修订查阅。
- W25Q64：按实际丝印确定子型号的 Winbond 官方手册核对 JEDEC、WIP/WEL、BP/TB 等保护位、256B Program、4KB Sector、复位电气和最大 Busy 时间；不要仅以 `EF4017` 推定完整型号后缀。
- **SSD1306**：使用 SSD1306 控制器手册固定 128×64 的寻址、初始化、0x00/0x40 I2C Control Byte；**不引入 SH1106 等兼容驱动**，第三方字体/驱动代码需核对许可和 Flash 容量。
- STM32F1 HAL / CMSIS：以两个正式 CubeMX 生成项目实际版本为准，特殊寄存器访问必须符合 ST 文档和 HAL ownership；不得从 F4/H7 的 HAL 接口/Flash 缓存实例直接复制。
- FreeRTOS：核对本项目真实的 CMSIS wrapper 版本、任务栈单位、`heap_4`、NVIC `FromISR` 优先级与线程安全。历史样例是结构参考，不能覆盖本版协议边界。

当官方手册和本版实施值出现真实硬件冲突时，记录地址/章节/测量证据并暂停对应危险动作，让用户决定是否改连线或发布设计修订；Codex **不得擅自修改固定的 16KB/48KB、W25 布局、可靠性功能或 OLED 型号**。

---

## 18. 唯一交付标准

产品最终必须同时满足：**16KB Boot + 48KB App、20KB SRAM、SSD1306 真实可视业务、USART1 DMA/IDLE、完整标准 YModem、32B Header、CRC16/CRC32、W25 下载与可靠旧版备份、72B 双扇区 Journal、确定性掉电恢复、App→Boot 请求、最多三次 Trial、健康确认、IWDG 接力、自动回滚、固定 Phase 0～9 全部实测验收**。不得以更换芯片、扩大 Boot 分区、删除失败路径、把 App 业务迁入 Boot，或以独立 HW05E 静态/真机测试代替产品 C07 证据来实现“表面达标”。

**项目目前状态**：检查点 A 配置设计准备；用户用 CubeMX 生成两个新正式工程后，由 Codex 执行 §19 检查点 B。只有 `CUBEMX_BASELINE_REVIEW = PASS` 才开始 Phase 0 代码开发；需要用户实际接线/测量/授权/上板的关口按本文阶段退出表逐项完成。不存在“全部产品已完成”的声明。

---

## 19. 正式双工程一次性 CubeMX / Keil 基线（检查点 A，2026-10-12）

本节是原规格的实施细化，不是第二套 Phase，不改变 Header、CRC、Metadata、分区、任务职责或安全策略。**尚未生成工程，`CUBEMX_BASELINE_REVIEW = NOT RUN`；本轮不实施产品模块。**

### 19.1 实际输入与版本证据

- Codex 工作根：`F:/Embedded_Study/Templates/RTOS_Project_Template`。正式项目根：其下 `IAP_PROJECT/`；不要把外层重命名或再创建一层 `IAP_PROJECT`。本文后续相对路径均从正式项目根计算。
- 初始真实文件只有根 `AGENTS.md`、`doc/` 和 `IAP_REFERENCE/`；根约束已在正确位置，未移动，未创建第二份 AGENTS。工作根、项目根及其父链的 `git rev-parse --show-toplevel` 均未找到 Git 仓库；Git diff/status/commit **不可提供真实结果**，不自行 `git init`。
- 三份核心管理文档与辅助指南已复核；本轮不保留预建工程空骨架。Core/Drivers/MDK-ARM/Middlewares 由用户 CubeMX 生成，User/Shared/Tests/Tools/Evidence 后续按模块需要创建。
- 本机 CubeMX 安装：`F:/Embedded_Study/Software/IDE_Tools/CubeMX_work`；安装元数据 APP_VER=**6.17.0**，历史 `.ioc` 同版，数据库 DB.6.0.170。菜单依据本机数据库/模板和随附 UM1718 Rev50 核查，未代替用户操作 GUI。若用户实际启动其他安装，先按 Help/About 核对；不要静默迁移版本。
- 固定复制 **STM32Cube FW_F1 V1.8.7**（本机 Repository 已存在）；其 HAL 源码版本 **1.1.10**；FreeRTOS 源码 **10.3.1**；有 `portable/RVDS/ARM_CM3/port.c` 与 `portmacro.h`，适配 AC5。CMSIS 的设备头、core 头及版本宏在检查点 B 连同文件哈希记录，不能把 FW 包版本当作 CMSIS 版本。
- Keil `UV4.exe` 文件版本 **5.37.0.0**，ARMCC 可执行文件存在。历史真实 Build log 写明 **5.06 update7 build960**；本轮沙箱内 `armcc --vsn` 发生 Windows DLL 重定位错误；获用户许可后在沙箱外执行成功，真实输出确认 **MDK Professional5.37 / ARM Compiler5.06 update7 build960**（2026-10-12 只读执行的版本输出；此处保留结论，临时输出文件已清理）。未构建新工程；后续构建需采用可实际执行的环境，版本确认不能替代 full rebuild。
- DS5319 本地 **Rev18**：64KB/20KB、表50温度特性与启动最长10µs、PC13输出限速2MHz；RM0008 本地 **Rev17**：RCC/DMA/USART/SPI/ADC/BKP/IWDG。ADC 239.5/12MHz=19.958µs；DS5319 表50列示17.1µs且带“最短时间须应用迭代确定”的脚注，不能改称逐片实测精度保证。
- W25Q64BV 手册为 **2010-07-08**，SPI Mode0/3、256B页、4KB扇区；其 tPP最大3ms、tSE最大200/400ms、64KB擦最大1000ms只适用于该手册条件，**不作为未知实物子型号的最终超时常量**。芯片完整丝印/保护位仍须 Phase0/6确认；本轮不写擦。
- SSD1306 本地手册 §8.1.5：SA0选择0x3C/0x3D、控制字节区分命令/数据。正式选择仍固定0x3C，Phase9只读ACK/显示验收，不能自动接受未知地址。
- 历史 `i2ctest.ioc/.uvprojx`、独立 HW05C/HW05E 原始工程/串口附件在当前文件树未找到；旧文档中对此的描述仅保留为来源记录，不能宣称本轮重验。
- 定向检查两候选的 `.ioc/.uvprojx`、HAL UART、W25、PB12、CRC/Header、LSI 和相关真实 MAP。`G4D_OPT2B3_OPT3_Candidate 04B_TRIAL/MDK-ARM/...map` 实际ROM为**5760B**，归档 `C02_Records/WithProbe_O3_add/C04_B_REAL_BOOT.map` 才是**15596B**（LR=0x3CEC）。目录名不证明构建身份，不能混用为当前产品容量。

启动/Flash外部补充依据：[PM0056 Rev7 §4.4.4](https://www.st.com/resource/en/programming_manual/pm0056-stm32f10xxx20xxx21xxxl1xxxx-cortexm3-programming-manual-stmicroelectronics.pdf) 的VTOR要求与 [PM0075](https://www.st.com/content/ccc/resource/technical/document/programming_manual/10/98/e8/d4/2b/51/4b/f5/CD00283419.pdf/files/CD00283419.pdf/jcr:content/translations/en.CD00283419.pdf) 的中容量1KB页/半字编程；AC5用户启动挂钩以 [armlink v5.06 §6.7](https://www.keil.com/support/man/docs/armlink/default.htm) 的符号替换机制为依据，后续必须实际链接/反汇编确认。

厂商 PDF 原件保留在 doc/；重复提取文本、旧快照与临时审查输出已清理，配置结论仅维护在本节。外部核对：[ST UM1718](https://www.st.com/resource/en/user_manual/um1718-stm32cubemx-for-stm32-configuration-and-initialization-c-code-generation-stmicroelectronics.pdf) 的 Advanced Settings 支持取消初始化调用；[FreeRTOS Cortex-M 优先级说明](https://www.freertos.org/RTOS-Cortex-M3-M4.html)说明 FromISR 与中断阈值规则。配置以本机生成结果为最终审查对象。

### 19.2 唯一模块责任与接口依赖

下表是接口设计契约，不是已实现函数原型。接口后续在所属 Phase 给出完整参数、上下文、错误码和测试。

| 正式模块/位置 | 唯一责任与接口方向 | 明确禁止/用途 |
|---|---|---|
| BSP `uart_dma`（每镜像一个运行 owner） | 初始化关联唯一 huart1/hdma句柄；StartRx、TxSubmit、TxIdle/FlushTC、有界错误恢复；HAL回调进入同一个 RX 收集器 | 无ASCII/YModem/存储决策；Phase1普通UART、Phase2启用DMA，App Phase9接力 |
| Shared `ring_buffer` | 纯2KB SPSC容器；Push由收集器独占、Pop由Boot循环/App UART任务独占；一格空有效2047B | 不触寄存器，不自行Poll，不启动第二消费者 |
| BSP `spi_bus` | SPI1/SPE与PA4 CS的物理所有者，事务开始/交换/结束/异常释放 | W25驱动只能通过此端口访问，不另建寄存器传输路径 |
| Boot Drivers/W25Q64 | 唯一W25设备实现：JEDEC/SR、WEL/WIP、读/跨页写/对齐擦/物理8MB边界/超时 | 不理解Metadata/FSM，不隐式擦陌生区；Phase0只读，Phase6起受控写 |
| BSP `internal_flash` | 内部半字编程、页擦、锁管理、硬件错误/忙超时；硬保护App页16..63 | 不判定新旧版本，不改OptionBytes，不擦Boot；Phase5 |
| IAP `store` | W25 Download/Backup/Meta/Log分区权限与范围检查、授权操作；调用唯一W25 | 协议不可绕过Store；Reserved禁写 |
| Shared `crc16/crc32/header/vector/boot_app_contract` | 唯一纯算法/显式LE布局/统一8B MSP谓词/DRx值；同一源文件按需链接两目标 | 不重复维护算法；不含HAL、介质副作用或状态机；Phase0/4等 |
| Protocol `ascii/router/ymodem` | CRLF/互斥模式/标准包序号CRC和有界会话；输出数据与事件给IAP服务接口 | 不直接调W25/Flash/Metadata；写入落盘结果由FSM返回，成功后才能ACK；Phase3/4 |
| IAP `metadata` | 唯一72B编码、A/B扫描/语义检查/追加/切区及回读；Scan只读、Commit有副作用 | 不隐式安装；不能凭日志判可信，不把CRC末写称硬件原子；Phase7 |
| IAP `image_validate/backup/install/rollback` | 对可信快照作全量验证，按FSM授权执行复制、双CRC、安装与完整48KB恢复 | 驱动不参与升级状态判断；Phase4～8 |
| IAP `fsm/recovery` | 唯一事务协调者，九态、危险操作前持久提交、超时/失败/复位恢复；Decide纯判定、Execute显式IO | 无多处同时提交状态，不让命令绕过恢复；Phase6～8 |
| IAP `boot_policy` | 收集Journal、镜像验证、PB12、BKP上下文后决定维护/恢复/Confirmed/Trial；恢复优先；Trial持久计次由FSM执行 | Decide不写，不凭向量建立信任；Phase0隔离白名单，Phase7产品策略 |
| IAP `jump` + BSP `jump_platform` | Policy授予可信目标后Prepare可失败；Commit净化UART/DMA/SPI/CS、Tick/NVIC/system pending、VTOR/屏蔽状态/MSP并不可返回 | 不决定版本/Metadata、不从ISR跳、不恢复到已拆环境；Phase0/7/8 |
| BSP `backup_domain`（每镜像一个） | PWR/BKP时钟、DBP权限保存恢复、DR1/DR2/DR3读写回读和消费；Boot清请求，App UpgradeCtrl写DR1/2 | App不写DR3；禁BDRST/自动RTC配置；Phase0/8/9 |
| Boot BSP `lsi_measure` | Phase8按RTC计数对HSE参考1ms窗口测LSI，有界RSF/RTOFF/权限检查，保留现场 | F103C8没有TIM5，不套用RM中高容量TIM5方案；RTCSEL不兼容则停，不自动重置备份域 |
| BSP `iwdg` | Boot唯一StartMeasured（Phase8）；Boot按真实操作进度刷新；App仅EarlyHandoffFeed/HealthyFeed | 无第二启动/重配、无无条件主循环喂狗；App的任务级喂狗仅UpgradeCtrl |
| Application BSP/Drivers/App | ADCTemp单点校准采样、I2C端口、唯一SSD1306 framebuffer；Business拥有OLED/ADC，UART任务解析，UpgradeCtrl控制请求/确认/复位 | 不编译W25/Store/Metadata，PB12不占用；完整业务Phase9 |
| Tests/Tools/Evidence | 主机负例/故障注入、隔离最小App/工装、打包与产物审计、原始验收证据 | Release组不引用历史探针、工装或测试白名单 |

依赖方向：`Core薄入口 → BootMain/Phase入口 → Policy/FSM → Validate/Store/Metadata/Install → BSP/设备驱动 → HAL/CMSIS`；`Protocol → IAP service`只交换请求/结果；`App入口 → 三Task → AppHealth/Shell/Display → App BSP/SSD1306`；双方只引用同一Shared源码。W25下层不反向依赖FSM。

**IRQ/回调所有权冻结**：USART1 IRQ只调用 `HAL_UART_IRQHandler(&huart1)`；CH4/CH5 IRQ只各调用对应 `HAL_DMA_IRQHandler`。使用已查源码的 `HAL_UARTEx_ReceiveToIdle_DMA(...,512)`（Phase2/9启动），HAL独占IDLE/错误/HT/TC/TE标志清除；唯一 `HAL_UARTEx_RxEventCallback`转发到收集器、唯一 `HAL_UART_ErrorCallback`锁存故障、唯一TX完成回调归TX owner。**不同时再手工SR/DR清IDLE，不注册第二套DMA Xfer回调，也不混用RxHalf/RxCplt另行入Ring。** 收集器读取受控CNDTR快照统一去重；三IRQ同抢占优先级6、sub=0，避免相互嵌套更新head。HT不能被关闭。HAL Normal TX DMA完成会启用USART TCIE，最终回调仍须审真实HAL和硬件TC；复位/跳转前再次确认TC。

所有ISR只做有界搬运/锁错/通知；App仅优先级6的回调可调用FromISR和portYIELD_FROM_ISR。禁止HAL阻塞API在ISR内等待Tick15；阻塞/临界区跨度必须远小于半圈22.2ms并审最坏Flash阻塞，不能仅凭CNDTR相同断言未丢一圈。Phase2须用覆盖/延迟证据与fail-closed守卫关闭此风险。

### 19.3 创建两个新工程和通用生成选项

从 **New Project → MCU Selector → STM32F103C8T6 (LQFP48)**分别新建，不能Copy历史工程。

| 页面/设置 | Bootloader准确值 | Application准确值 | 理由/是否需确认/Phase |
|---|---|---|---|
| Project Manager → Project → Project Name | `Bootloader` | `Application` | 固定正式名；无需重新确认；0～9 |
| Project Location（父目录） | `F:/Embedded_Study/Templates/RTOS_Project_Template/IAP_PROJECT` | 同左 | 生成后的.ioc必须分别落`Bootloader/Bootloader.ioc`、`Application/Application.ioc`；避免双重同名目录；0 |
| Toolchain / IDE | **MDK-ARM**，若下拉显示版本用`MDK-ARM V5.32` | 同左 | 标签表示生成模板兼容性，不是要求安装Keil5.32；实际MDK5.37/AC5；无需确认 |
| Firmware Package | **STM32Cube FW_F1 V1.8.7**；取消使用最新包/自动迁移 | 同左 | 固定本机已核对包；其他版本需先复核；0～9 |
| Code Generator → STM32Cube Firmware Library Package | Copy only the necessary library files | 同左 | 官方库复制进各工程；不引用历史相对路径 |
| Code Generator | Generate peripheral initialization as a pair of .c/.h files=勾选；Keep User Code when re-generating=勾选 | 同左 | gpio/dma/usart等独立；用户实现放User；0～9 |
| Code Generator | Backup previously generated files=勾选；Delete previously generated files when not re-generated=不勾选 | 同左 | 再生成仍须审删除/遗留Source Group；0～9 |
| Project → Heap / Stack Size | C库Heap=`0x200`、MSP Stack=`0x400` | C库Heap=`0x200`、MSP Stack=`0x400` | 初始候选非验收；RTOS heap6144另设，不能填到C heap；0/9 |
| Advanced Settings → Driver Selector | 所有本项目外设 **HAL** | 同左 | 寄存器特化在唯一BSP内审查，不生成另一LL owner |
| Advanced Settings → Generated Function Calls | 常规顺序`SystemClock_Config → GPIO → DMA → SPI1 → USART1` | `SystemClock_Config → GPIO → DMA → USART1`；ADC/I2C调用抑制 | DMA时钟必须先于USART MSP；后续初始化由Phase入口控制 |
| Advanced Settings → IWDG行 | 保留生成函数；勾选 **Not generate Function call / Not to be generated**；Visibility(Static)=取消 | IWDG不生成初始化 | 两个名称为本机UM1718记载的措辞；核实是取消“调用”而非删掉函数；Phase8 |
| Advanced Settings → ADC1/I2C1行 | 不存在 | 两行勾选取消自动调用，取消Static使User可调用生成函数 | Phase0最小App不做OLED/ADC业务；Phase9受控启用，无需改.ioc |
| RTOS自动启动 | 不存在 | 生成骨架暂不得烧录运行；Phase0/9入口由User挂钩接管 | CubeMX的RTOS自动调度不是Phase0测试程序；检查点B后Codex集成 |

生成后只保存/关闭，不连接设备、不点Download/Debug。用户可以配置Keil选项，但Codex负责检查点B后的Source Group/scatter/VTOR/Phase入口集成。

### 19.4 Bootloader 完整 CubeMX 操作表

下表“确认”中的“否”表示设计已定；“实测门槛”只阻止对应硬件动作，不阻止生成工程。

| CubeMX页面/项 | 应选准确值 | 与App差异/配置理由 | 确认 | Phase用途 |
|---|---|---|---|---|
| System Core → SYS → Debug | **Serial Wire**；PA13 SWDIO、PA14 SWCLK | 两者相同，保留SWD；不选No Debug/JTAG占针 | 否 | 0～9 |
| SYS → Timebase Source | **SysTick** | Boot裸机HAL 1ms；App选TIM2 | 否 | 全部 |
| System Core → RCC → HSE | **Crystal/Ceramic Resonator** | 板上8MHz，不选Bypass | 否 | 全部 |
| RCC → LSE | **Disable** | 不依赖RTC日历，不配置PC14/15，避免备份域副作用 | 否 | 全部 |
| RCC → LSI | **Enabled**（IWDG选择导致自动勾选也保留） | 仅开启振荡器不会启动IWDG；Phase8读LSIRDY并测频 | 实测LSI在8 | 8/9 |
| Clock Configuration → HSE Input | **8 MHz** | 不复制历史HSI8MHz工作树 | 否 | 全部 |
| PLL Source / HSE divider / PLL multiplier | **HSE /1 / ×9**；System Clock Mux=PLLCLK | PLL=72MHz | 否 | 全部 |
| AHB / APB1 / APB2 prescaler | **/1、/2、/1** | HCLK72、PCLK1=36、PCLK2=72MHz；APB1 timer=72MHz | 否 | 全部 |
| ADC prescaler（若Clock页显示） | **PCLK2/6 = 12MHz** | Boot不启用ADC；不把此时钟值当传感器启动 | 否 | 统一时钟树 |
| Flash Latency/Prefetch（生成结果） | **FLASH_LATENCY_2**；Prefetch启用 | 不是独立虚构GUI项；查SystemClock/HAL设置2WS | 否 | 全部 |
| RCC CSS / USB | CSS不启用；USB外设Disabled | 48MHz框若自动显示不等于启用USB；无USB初始化 | 否 | 全部 |
| Connectivity → USART1 → Mode | **Asynchronous**；TX/RX；不Remap；PA9/PA10 | 两者相同 | 否 | 0～4/9 |
| USART1 → Parameter Settings | **115200 Bits/s；Word Length 8 Bits；Parity None；Stop Bits 1；Hardware Flow Control None；Over Sampling 16** | 8N1；不使用同步Clock/CTS/RTS | 否 | 全部 |
| USART1 → GPIO Settings | PA9 AF Push-Pull、High(50MHz)、No pull；PA10 Input、Pull-up（如该页仅Input无pull选项，GPIO页设置） | F1 RX无AF号选择；空闲高 | 否 | 0～4 |
| USART1 → DMA Settings → Add RX | **USART1_RX / DMA1 Channel5 / Peripheral to Memory / Circular / Priority High** | 两者相同；优先级是DMA仲裁，不是NVIC值 | 否 | 2～9 |
| RX DMA increment/alignment | Peripheral Increment=Disabled；Memory Increment=Enabled；Peripheral/Memory Data Width=Byte/Byte | **512B在BSP StartRx参数指定，不是CubeMX DMA长度菜单** | 否 | 2～9 |
| USART1 → DMA Settings → Add TX | **USART1_TX / DMA1 Channel4 / Memory to Peripheral / Normal / Priority Medium** | 不使用Circular TX | 否 | 2～9 |
| TX DMA increment/alignment | Peripheral Increment=Disabled；Memory Increment=Enabled；Width=Byte/Byte | 缓冲保留到USART TC | 否 | 2～9 |
| System Core → NVIC → Priority Group | **NVIC_PRIORITYGROUP_4**（4 bits preemption / 0 sub） | 与App一致，单收集器串行 | 否 | 0～9 |
| NVIC USART1 global interrupt | **Enable；Preemption 6；Sub 0** | UART错误、IDLE和TX最终TC共用IRQ | 否 | 1/2～9 |
| NVIC DMA1 CH4 / CH5 global interrupt | **两者Enable；Preemption 6；Sub 0** | 不照搬历史RX5/TX6；不相互抢占 | 否 | 2～9 |
| NVIC SysTick | **15 / 0** | 1ms裸机HAL Tick；不在高优先级ISR HAL_Delay | 否 | 全部 |
| DMA HT/TC/TE 与USART IDLE/Error | IRQ入口生成；运行期由HAL启动ReceiveToIdle配置HT/TC/TE、IDLEIE、EIE/PEIE | **不是CubeMX独立HT/IDLE复选框**；遵循§19.2回调唯一所有权 | 否 | 2 |
| Connectivity → SPI1 → Mode | **Full-Duplex Master**；PA5/6/7；无Remap | Boot独占；AppDisabled | 否 | 0/6～8 |
| SPI1 → Parameter Settings | Data Size=8 Bits；First Bit=MSB First；Clock Polarity=High；Clock Phase=2 Edge | Mode3与历史已核源码一致 | 否 | 0/6～8 |
| SPI1 NSS / Baud Rate / CRC / TI | **Software NSS；Prescaler16；CRC Calculation Disabled；TI Mode Disabled** | PCLK2/16=4.5MHz；历史/8在8MHz仅1MHz，不宣称同速真机通过 | 否 | 0/6～8 |
| SPI1 → GPIO | PA5/PA7 AF Push-Pull、High50MHz、No pull；PA6 Input No pull | 无SPI IRQ、无SPI DMA | 否 | 0/6～8 |
| Pinout PA4 → GPIO_Output；GPIO页 | Label=W25_CS；Output level=**High/SET**；Output Push-Pull；No pull；Speed Low2MHz | GPIO写SET必须先于模式配置；外部10k上拉保障复位高阻 | 实测CS/子型号在0 | 0/6～8 |
| Pinout PB12 → GPIO_Input；GPIO页 | Label=RECOVERY_KEY；**Pull-up**；无EXTI | Boot专用；原P1采样需适配时钟/入口 | 否 | 0/7/8 |
| PC13 / PB6 / PB7 | 不配置业务GPIO/I2C | Boot不依赖LED、不驱动OLED | 否 | 全部 |
| System Core → IWDG | **Activated；Prescaler64；Reload4095（只作生成占位）** | 4095绝不是4s批准值；函数自动调用按§19.3禁止 | 实测LSI/WDG_SW在8 | 8 |
| RTC / PWR / BKP | **RTC Disabled**，无RTC/LSE/Tamper/Alarm IRQ；PWR HAL支持保留 | BKP无需RTC运行；无独立BKP初始化GUI要求；BSP按需开PWREN/BKPEN、DBP | BKP真机在0 | 0/8/9 |
| 其他外设 | ADC/I2C/USB/CRC硬件/FreeRTOS/WWDG/TIM业务/无用EXTI均Disabled | 全部Phase需求已由列出的资源覆盖 | 否 | 全部 |
| HAL模块/启动文件（生成审核） | HAL RCC/RCC_EX、GPIO、CORTEX、DMA、UART、SPI、PWR、FLASH/FLASH_EX、IWDG；`startup_stm32f103xb.s`，STM32F103xB/USE_HAL_DRIVER | FLASH是HAL core依赖且Phase5需要；只保留必要实现，不改官方源码 | 否 | 全部 |

IWDG安全启用：生成`MX_IWDG_Init`但没有自动调用；Phase0～7任何入口不可调用它或写KR=0xCCCC。Phase8唯一iwdg owner测得LSI后按§10.3计算RLR，受控设置并启动；生成的占位函数不作为默认Start。启用后不可能软件关闭。App只写刷新key，不调用HAL_IWDG_Init。WDG_SW若实物为硬件启动，禁止试运行Phase0；只读记录Option Bytes后由用户决定，Codex不改OB。

RTC/LSI最终方案：Boot保留RTC寄存器测频服务（不是日历），以HSE72MHz参考SysTick计时，按实际RTCSEL检查并有限等待RSF/RTOFF。RTCSEL为0时首次选择LSI属于Phase8明确授权动作；已为LSI则不BDRST；其他非零选择或现场无法解释则STOP。App不读写RTC/BDCR。**CubeMX始终RTC Disabled**，BKP/IWDG所需资源通过固定BSP按Phase开启，不会后续再补一个RTC外设配置。RTC测频完成后的恢复范围、窗口误差与IWDG已运行情况下的时间预算在Phase8审查，不把旧HSI500ms测量结果称作新HSE测量通过。

### 19.5 Application 完整 CubeMX 操作表

| CubeMX页面/项 | 应选准确值 | 与Boot差异/配置理由 | 确认 | Phase用途 |
|---|---|---|---|---|
| MCU / SYS Debug / HSE / PLL / buses | STM32F103C8T6、Serial Wire；HSE晶振8MHz、HSE/1×9、PLLCLK；AHB/1、APB1/2、APB2/1 | SYSCLK/HCLK72、APB1=36、APB2=72；Flash2WS/prefetch同Boot | 否 | 0/9 |
| RCC LSE / LSI / RTC | LSE Disabled、RTC Disabled；不新增LSI重新配置（由Boot已运行的IWDG维持） | App时钟初始化只针对HSE/PLL，不关闭继承的LSI，不改BDCR | 否 | 0/8/9 |
| SYS → Timebase Source | **TIM2** | **不要再在TIM2页面配置另一个计数器/PWM**；HAL模板自动1kHz | 否 | 0/9 |
| NVIC TIM2 | **Enable，Preemption15，Sub0** | HAL timebase更新中断只HAL_IncTick，不调用RTOS FromISR | 否 | 0/9 |
| USART1 mode / baud / pins | Asynchronous TX+RX；PA9/PA10、115200、8Bits、None parity、1Stop、None flow、Oversampling16；TX AF PP50MHz、RX Input Pull-up | 与Boot完全同参数，无Remap | 否 | 0/9 |
| USART1 RX DMA | CH5、Peripheral→Memory、Circular、High；PInc off、MInc on、Byte/Byte | BSP512B、Ring2048B；不是GUI长度项 | 否 | 9 |
| USART1 TX DMA | CH4、Memory→Peripheral、Normal、Medium；PInc off、MInc on、Byte/Byte | TX owner直到最终TC | 否 | 9 |
| NVIC Group / USART / DMA4/5 | **Group4；三IRQ均Enable，6/0** | FromISR合法；同优先级串行收集器；使用HAL IRQ单一路径 | 否 | 9 |
| NVIC SysTick / PendSV / SVC | SysTick15/0、PendSV15/0；SVC由RVDS移植层管理（通常0） | SVC不是UART FromISR调用者，不强制把SVC改6；只留一个实现/向量 | 否 | 9 |
| Connectivity → I2C1 | **I2C**；PB6 SCL、PB7 SDA，不Remap | App独占，Boot不启用 | 否 | 9 |
| I2C1 → Parameter Settings | **I2C Speed Mode Standard；Clock Speed100000Hz；Duty Cycle2；Addressing7-bit** | Duty在Standard不影响固定标准时序；部分界面Duty不可编辑则保持默认 | 否 | 9 |
| I2C1 Own Address / Dual / General Call / No Stretch | Own Address1=0；Dual Address Disabled；General Call Disabled；No Stretch Disabled（允许stretch） | 主机自身地址不是SSD1306地址；如OwnAddress2隐藏无需填 | 否 | 9 |
| I2C1 GPIO / DMA / NVIC | PB6/7 AF Open-Drain、No internal pull、High50MHz；无DMA；EV/ER IRQ不启用 | 外部3.3V上拉；驱动有界轮询，单一Business owner | 实物上拉/0x3C在9 | 9 |
| SSD1306地址（代码接口） | **7-bit0x3C；HAL DevAddress0x78** | CubeMX没有“OLED slave address”配置项；不可填到OwnAddress1 | 否 | 9 |
| Analog → ADC1 → Mode | **Temperature Sensor**内部通道16；Independent；不选外部通道/注入组 | 不占外部ADC引脚 | 否 | 9 |
| Clock Configuration ADC | **PCLK2/6=12MHz**（上限14MHz） | 不照搬APB2=72直接驱ADC | 否 | 9 |
| ADC1 → Parameter Settings | Scan Disabled；Continuous Disabled；Discontinuous Disabled；Data Alignment Right；Number of Conversions1；External Trigger Software Start | F1固定12bit，不编造Oversampling/Resolution选择 | 否 | 9 |
| ADC1 Regular Rank1 | **Temperature Sensor / CH16；Sampling Time239.5 cycles** | 采样窗19.958µs，单转换约21µs；TSVREFE/稳定最长10µs核对HAL | 否 | 9 |
| ADC DMA/NVIC/校准 | 不启DMA/ADC IRQ；首次使用前`HAL_ADCEx_Calibration_Start(&hadc1)`并检查返回；16次采样各有限超时 | 校准不是CubeMX一个可伪造的自动选项，归ADCTemp owner | 实测在9 | 9 |
| Pinout PC13 → GPIO_Output | **Output Push-Pull；High/SET（灭）；No pull；Low2MHz**；Label=STATUS_LED | 板载低电平亮；遵守DS限速/灌电流，不外接新LED | 否 | 9 |
| SPI1 / PA4 / PA5/6/7 / PB12 | **不启SPI1；PA4/5/6/7不配置；PB12保留不配置** | App不编译W25、不抢恢复键、不得用CubeMX“所有空闲针Analog”全局覆盖它们 | 否 | 全部 |
| IWDG | **Disabled（不生成MX_IWDG_Init）** | 接力刷新用唯一App BSP寄存器key/HAL支持接口；禁重启重配 | 否 | 8测试/9 |
| PWR / BKP / RTC | PWR HAL支持；RTC保持Disabled；BSP按需开PWR/BKP、写权限回读后恢复 | App只读DR3，UpgradeCtrl独占DR1/2，绝不触W25 | BKP真机在0 | 0/9 |
| Middleware → FREERTOS → Interface | **CMSIS_V1** | 本机F1 Native Mode在数据库被注释，**不能选Native GUI**；最终用户代码仍用Native API | 否（同版已查） | 9 |
| FREERTOS → Config parameters → Kernel | USE_PREEMPTION=Enabled；USE_TIME_SLICING=Enabled；TICK_RATE_HZ=1000；MAX_PRIORITIES=7；MINIMAL_STACK_SIZE=128 words；USE_16_BIT_TICKS=Disabled；IDLE_SHOULD_YIELD=Enabled | 7兼容CMSIS_V1映射，正式只用3/2/1；Idle=0 | 否 | 9 |
| FREERTOS → Memory management | **heap_4；TOTAL_HEAP_SIZE6144；Dynamic allocation Enabled；Static allocation Disabled**；Use FW pack heap file Enabled | 仅一heap_4；Idle栈/TCB也在heap中；无静态Idle重复分配 | 否 | 9 |
| FREERTOS notifications/mutex | USE_TASK_NOTIFICATIONS Enabled；USE_MUTEXES Enabled；USE_RECURSIVE_MUTEXES Disabled；USE_COUNTING_SEMAPHORES Disabled | UART通知用Native API；队列/锁由用户模块需要时创建，不新增任务 | 否 | 9 |
| FREERTOS timers/hooks | USE_TIMERS Disabled；USE_IDLE_HOOK Disabled；USE_TICK_HOOK Disabled；CHECK_FOR_STACK_OVERFLOW=2；USE_MALLOC_FAILED_HOOK Enabled | 无Timer Service第4业务任务；失败hook由User提供，不能静默空函数 | 否 | 9 |
| FREERTOS optimization/debug | USE_TICKLESS_IDLE Disabled；USE_NEWLIB_REENTRANT Disabled；GENERATE_RUN_TIME_STATS Disabled；USE_TRACE_FACILITY Enabled；USE_STATS_FORMATTING_FUNCTIONS Disabled | 无额外统计定时器，不引printf浮点；TRACE便于任务审计 | 否 | 9 |
| FREERTOS → Include parameters | INCLUDE_vTaskDelay=1；INCLUDE_vTaskDelayUntil=1；INCLUDE_vTaskSuspend=1；INCLUDE_xTaskGetSchedulerState=1；INCLUDE_uxTaskGetStackHighWaterMark=1 | 调度/栈高水位；其他非需API按默认不创建额外对象 | 否 | 9 |
| FREERTOS interrupt definitions | configPRIO_BITS=4；LIBRARY_LOWEST_INTERRUPT_PRIORITY=15；LIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5 | 寄存器值KERNEL=0xF0、MAX_SYSCALL=0x50；若PRIO_BITS隐藏由__NVIC_PRIO_BITS推导，审核宏而非杜撰菜单 | 否 | 9 |
| FREERTOS configASSERT | 启用并在USER CODE保留真实assert处理 | 错优先级/堆不足可观测，Release不能只删除必要安全检查省ROM | 否 | 9 |
| HAL模块/启动文件（审核） | RCC/RCC_EX、GPIO、CORTEX、DMA、UART、PWR、FLASH/EX、TIM/TIM_EX、I2C、ADC/ADC_EX；startup_stm32f103xb.s | 没有HAL SPI/W25/RTC业务；官方FreeRTOS在Middlewares，AC5只RVDS/ARM_CM3 | 否 | 0/9 |
| 危险/未来初始化 | ADC1/I2C1取消自动函数调用；RTOS产品启动由USER CODE Phase入口接管；RTC/IWDG无自动初始化 | 所有外设一次配齐不等于Phase0运行完整App | 否 | 0～9 |

**Tasks and Queues 页面一次准备三个骨架**：把原defaultTask改名第一行，再添加两行；不要保留第4个defaultTask。CMSIS_V1源码实际优先级转换为`priority-osPriorityIdle`，因此下表的CMSIS文字选项才对应产品3/2/1，不能直接选osPriorityHigh造成实际5。

| GUI Task name / Entry function（仅生成骨架） | GUI CMSIS_V1 Priority | Stack Size（Words） | 最终User任务/Native优先级 |
|---|---|---:|---|
| `UartRecv` / `CubeStub_UartRecv` | **osPriorityNormal** | **256** | `Task_UartRecv`，3，1024B |
| `UpgradeCtrl` / `CubeStub_UpgradeCtrl` | **osPriorityBelowNormal** | **192** | `Task_UpgradeCtrl`，2，768B |
| `Business` / `CubeStub_Business` | **osPriorityLow** | **192** | `Task_Business`，1，768B |

三行Allocation均Dynamic，Parameter=NULL（界面若显示0等价），无额外Queues/Timers/Semaphores预创建。骨架名故意不等于产品任务函数，避免两处定义。同版页面若栈单位显示Bytes而非Words必须按1024/768/768输入并记录，不能仅复制数字。

**Native API与生成骨架的衔接**：生成的CMSIS任务只是配置/内核取件骨架，正式运行**只由Application/User/App任务注册器调用一次xTaskCreate（三任务3/2/1、256/192/192 words）和vTaskStartScheduler**。检查点B后，在main的USER CODE初始化挂钩进入不可返回的User Phase入口：Phase0运行隔离最小App，Phase9初始化ADC/I2C/SSD1306、注册Native任务并调度；失败有界诊断/停留且不确认，不落回CubeMX自动`MX_FREERTOS_Init/osKernelStart`路径。生成RTOS自动创建函数不被正式入口执行；其stub实现保持生成代码，不放业务，Release MAP核对它们被移除。不修改非USER CODE生成块、CMSIS wrapper或内核。Idle系统任务由内核管理，不计为第四个自定义任务。若本版生成结构无法保留此挂钩，检查点B统一提出由User主入口接管的Keil集成修正，不擅自改.ioc。

**App时基**：TIM2由HAL_InitTick按APB1 timer72MHz生成1ms更新；不能只看PCLK1=36MHz就设置错误PSC。TIM2 IRQ唯一HAL_TIM_IRQHandler、HAL_TIM_PeriodElapsedCallback只有TIM2分支HAL_IncTick。SysTick由FreeRTOS端口独占；RVDS port与FreeRTOSConfig中SVC/PendSV/SysTick映射必须逐个对照生成it.c，禁止双符号/双计数。调度器前SysTick RTOS处理不得提前访问未初始化内核；移植层Tick入口和生成配置统一审核。TIM2优先级15会被RTOS临界区暂时屏蔽，因此BSP有界等待不能在屏蔽TIM2的上下文依赖HAL Tick；App HAL时间不承诺硬实时精度。

### 19.6 Keil MDK5 / ARMCC5 / Scatter / VTOR / 容量检查表

| Keil页面/检查项 | Bootloader | Application | 审核方法/用途 |
|---|---|---|---|
| Options for Target → Device | STM32F103C8，Cortex-M3，无FPU | 同左 | 与.ioc/LQFP48/64K一致，不选CB/128K |
| Target → ARM Compiler | **Use ARM Compiler5；5.06 update7 build960** | 同左 | full rebuild真实输出；不因模板名MDK5.32改工具版本 |
| Target → Xtal | **8.0MHz** | 同左 | 板上晶振；CPU实际72由代码决定 |
| Target → IROM1 | **Start0x08000000、Size0x4000** | **Start0x08004000、Size0xC000** | 同时查Target/XML、scatter、MAP、HEX，不能只看GUI |
| Target → IRAM1 | **Start0x20000000、Size0x5000** | 同左 | RW/ZI、heap、MSP不得越界 |
| C/C++ → Optimization / Code Generation | **Level3(-O3)、Optimize for Time不勾、One ELF Section per Function勾** | 同左 | AC5 Size基准；不使用-Oz/-Os；C99按所需启用，不引AC6语言专属特性 |
| Target → Use MicroLIB | **勾选，作为本轮初始构建基准** | 同左 | 不用浮点printf；若包与库不兼容统一列必须修改；不据此预测容量 |
| C/C++ Defines | USE_HAL_DRIVER、STM32F103xB；Phase0隔离target额外PHASE0_TEST_ONLY | 无硬件SPI/W25宏；Phase0测试目标与Phase9入口隔离 | Release不得定义size_probe/LinkOnly/test gates |
| Include Paths | Core/Inc、官方HAL Inc/Legacy、CMSIS Device/ST/STM32F1xx/Include、CMSIS/Include；User各模块；`../../Shared` | 加FreeRTOS/Source/include与portable/RVDS/ARM_CM3；CMSIS_RTOS仅取件所需 | 路径以MDK-ARM为基准，无IAP_REFERENCE依赖，无同名头抢占 |
| Source Group | CubeMX生成、Boot_BSP/W25/Protocol/IAP、Shared；每.c一处 | App_BSP/SSD1306/App、Shared、FreeRTOS；仅heap_4与一个port.c | 头能找到不等于.c在Target；Exclude逐项审；无历史探针 |
| Startup | **startup_stm32f103xb.s**；Stack_Size0x400、Heap_Size0x200、8B alignment | 同左；任务使用PSP、所有IRQ使用MSP | Boot/App各自vector FIRST；不改ST启动文件，若生成值不符用用户owned启动配置/链接集成 |
| Linker → Use Memory Layout from Target Dialog | 本次生成可勾；审核与锁定时采用显式用户scatter | 同左 | 不同时让GUI与手工scatter各自成为真值；最终Linker命令必须用锁定sct |
| Scatter最终load/execute | LR_IROM1与ER_IROM1 `0x08000000 0x4000` | LR_IROM1与ER_IROM1 `0x08004000 0xC000` | RESET FIRST、InRoot$$Sections、.ANY(+RO)；RW_IRAM1 `0x20000000 0x5000`含.ANY(+RW +ZI) |
| VTOR | Boot复位vector0x08000000；Jump Commit设App0x08004000 | **SystemInit之后、HAL_Init/IRQ开启之前设0x08004000** | 优先由User/BSP的AC5 SystemInit链接拦截（$Sub$$/$Super$$）追加VTOR+DSB/ISB，不改官方system_stm32f1xx.c；检查真实调用链。仅main后期写VTOR不满足“早期”要求 |
| Output/Listings | Create HEX勾；AXF/MAP和完整Rebuild日志保存 | 同左；fromelf导出仅App.bin | Build ID+SHA256、符号保留/删除、vector/MSP/PC、Flash地址独立审 |
| Debug → ST-Link Settings | SWD、Connect Under Reset、Hardware Reset；初始SWD1MHz可降速 | 同左 | 不进入调试/Download直到单次硬件测试授权；App直接调试不得由默认reset误跑未知Boot |
| Utilities → Flash Download | **Erase Sectors + Program + Verify；禁Erase Full Chip/Mass Erase；禁自动Reset and Run/Load application at startup** | 同左，地址区仅App | Flash算法通常覆盖整个64KB，不能以“算法覆盖”推定只擦IROM；每次审实际HEX/页面与全片回读 |
| ROM门槛 | **Code+RO+RW及LR load均≤16384B；14KiB预警** | **≤49152B，起点0x08004000** | O3真实入口；必须保留所有安全功能；历史15596非承诺 |
| SRAM门槛 | RW+ZI含512RX/2048Ring/协议缓冲等；加未计入MAP的实际栈/堆峰值；MSP初始1024B | 512+2048+1024frame+128Shell+6144heap+1024MSP+512C heap=**11392B**基础账本，另加句柄/业务/TX/ADC等 | 若startup栈/heap已计入MAP ZI不得再加。任务2560B及Idle/TCB/队列在6144heap内，不重复计；审高水位/最小剩余堆 |
| Warning /入口可达 | 0Error；新Warning阻断或逐条依据；正式main→BootMain真实保留 | 同左；只运行三Native自定义Task | 本轮NOT RUN；不会生成虚构MAP/容量PASS |

正式scatter表达式在检查点B根据生成项目路径集成；本轮没有创建链接脚本或产品代码。Boot/App分别占整个RAM是不同镜像的独立预算，不能相加；任务栈不在MSP上。FreeRTOS禁止额外CMSIS静态栈/Timer任务隐藏分配。Stack/Heap数字是初始基线，按真实最坏场景审计调整要保留证据，不能删除功能。

### 19.7 历史迁移候选与回归契约（尚未迁移）

以 `R=IAP_REFERENCE/G4D_OPT2B3_OPT3_Candidate 04B_TRIAL`、`P=IAP_REFERENCE/C07_PB12_P1_D2_W25_READONLY_NO_JUMP_TRIAL` 为下表路径缩写。构建身份必须按具体MAP/日志哈希核对，不能仅按工程名。除已逐函数读取的纯CRC候选外，本轮仅完成定位/配置审查，**不批准未经当前模块完整审查的函数直接部署**。

| 模块/来源 | 可保留候选/必须重构/排除 | 主机测试与AC5回归 |
|---|---|---|
| CRC：R/User/User_Preflight_CRCSize/{crc16_xmodem,crc32_ieee}.c | `C04_CRC16_XMODEM`、`C04_CRC32_IEEE_Update/IEEE`算法可迁移候选；统一Shared命名、参数/原始state约定；排除c04_size_probe | 123456789、空串、随机分块/独立参考；AC5 O3同向量/ROM RO表检查，未执行不能称直接复用PASS |
| Header：同目录firmware_header.c/.h | LE解码思路可留；当前函数未拒firmware_version0且无输入缓冲长度参数，需重构并分离>=8可启动判定 | 32字段/截断/长度/溢出/版本0/向量；AC5 ABI与固定32B序列测试 |
| UART：R/User/Preflight_UARTSize/uart_size_probe.c | 512/2048和TX缓冲生命周期候选；由新BSP统一HAL ReceiveToIdle回调；排除探针Poll/自动C03输出/LinkOnly | 10000边界模型、HT/TC/IDLE去重/覆盖/错误/TX TC；AC5回调保留+DMA错误真机 |
| W25：P/User/Drivers/w25q64.c/.h | ReadSR/JEDEC/页拆分等候选；Init的SPE修复移交spi_bus owner；核实型号超时、初始化与异常恢复；排除W25Q64_TestSpi*、User/Tests/Watch门 | 跨页/物理+Store边界/CS连续/WEL/WIP超时；AC5 O3新增4.5MHz真实入口后再真机只读→受控写擦 |
| PB12：P/User/User_Preflight_BootSafety/c07a_platform.c | 两轮各16点、仅全高放行、配置前后核验可留；4000setup/64gap/8000between是CPU循环，**不等于毫秒**；在72MHz与入口次序重验；排除P1D2快照/三次诊断main | 纯决策组合、配置失败保持；AC5 O3反汇编/有限循环；松/按×NRST/冷启4组合，不以旧8MHz继承 |
| YModem：R/User/User_Preflight_YModemSize/ymodem_preflight.c | 标准Packet/Session候选，必须完整审ACK落盘、Header/长度与模式接口；排除c05_size_probe/实验发送 | SOH/STX/block0/尾块/重包/EOT/取消/超时；AC5真实链保留+标准Sender |
| Flash：R/User/User_Preflight_FlashInstall/g4b_flash_install.c | 受限安装候选；唯一BSP硬保护页16..63，FSM授予；排除g4b_size_probe/危险gate | 全边界/奇数/半写/复位恢复；AC5调用链/MAP，用户隔离页测试 |
| Metadata/Backup/Recovery：R/User/User_Preflight_RecoverySize/c06_*.c/.h | 72B/双CRC/Decide思路；重构共享编码/同序冲突/sequence上限/九态语义/落盘权限；排除c06*_size_probe及旧4B MSP谓词 | 每写擦点注入、dirty/切区/双valid/48KB恢复；AC5真实FSM+日志所有必需符号保留 |
| BootPolicy/Jump/BKP：R或P/User/User_Preflight_BootSafety/c07a_*.c | 两段Prepare/Commit候选；统一可信快照、8B MSP、屏蔽/异常/VTOR/不可返回、DR2/DR3最新契约；排除测试放行与LinkOnly | 向量/优先顺序/TC/清理状态模型；AC5反汇编/调用链；Phase0真实Boot→App，不烧历史C04-B |
| IWDG/LSI：R/User/User_Preflight_IWDG/g4a_iwdg.c；User/User_Preflight_ProdEntry/g4d2b_lsi.c | 有界监督思路；旧RTC测频参考HSI且同步顺序须审，改HSE参考/现场守卫；排除只保留Feed地址/LongOpSafe探针、无界RTC一致读 | reload/健康/长事务/超时/已启动交接模型；AC5真正Feed调用保留，Phase8实测LSI+复位 |
| Log/入口：R/User/User_Preflight_UpgradeLog/g4d_upgrade_log.c；User/User_Preflight_ProdEntry/*.c | 编排/查询思路；适配正式32B Log、薄入口、完整健康Feed；排除G4D2_LinkOnly/工装/size probes | 日志损坏/轮转/状态失败；AC5同功能ROM/正式入口可达审计 |
| SSD1306/FreeRTOS与HW05C/E | 当前没有可审查SSD1306产品源码或独立HW05C/E原件；使用官方包移植层，不从实验拼产品 | Phase9 framebuffer/地址/ADC/健康测试、AC5/App48K+20K、高水位；HW仅二级来源记录 |

每次真正迁移前，先补齐当前模块函数级审查、来源文件与构建哈希、保留/重构/排除清单；迁移后按§14契约依次测试/构建/真机验收。本轮没有执行迁移、主机算法测试或AC5产品构建。

### 19.8 高风险复核、用户确认与两个检查点

| 风险 | 本次配置准备的处理 | 何时必须关闭 |
|---|---|---|
| IWDG初始化即启动 | Boot生成但禁止调用；App禁生成Start；Phase8唯一StartMeasured；占位4095无4s含义 | B查main所有调用；Phase8只读OB/LSI实测/停喂/长事务/接力 |
| AC5与FreeRTOS版本 | FW_F1固定1.8.7、Kernel10.3.1、RVDS/ARM_CM3；GUI CMSIS_V1，用户Native运行 | B真实AC5 full rebuild，使用已验证可执行环境 |
| FromISR阈值 | Group4，IRQ6，max5、kernel15；三串口IRQ同优先级 | B宏移位0x50/0xF0和每调用者检查；Phase9configASSERT |
| HAL/RTOS重复时基与IRQ | Boot SysTick；App HAL TIM2、SysTick RTOS；HAL独占UART/DMA清标志 | B检查it.c/port/回调和HAL_InitTick；2/9故障注入 |
| SPI/OLED/ADC | Mode3/4.5MHz；OLED固定0x3C/100k；CH16 239.5/12M/校准 | 0/6真实子型号与CS/SPE；9ACK/刷新/温度/超时 |
| defaultTask/双任务注册 | GUI改名+2行仅骨架；User唯一Native创建；自动CMSIS路径不执行 | B后入口集成，Phase0最小App隔离；9运行任务计数与MAP |
| 地址/VTOR/启动文件 | 双IROM、IRAM与早期VTOR用户链接挂钩 | B项目/MAP/HEX；0真实Jump，9RTOS首次异常正确 |
| 误复用USB/Recovery/SWD | USB Disabled；PB12 Boot专用；SWD保留；PC13 App低速高初始 | B引脚和生成GPIO审查 |
| 未来功能随初始化运行 | RTC无生成、IWDG/ADC/I2C调用抑制、RTOS Phase入口控制；无自动写擦自检 | B全文调用/寄存器副作用审查；未集成骨架禁止烧录 |

**真正阻碍本轮配置的未决项**：在本机已确认的CubeMX6.17.0/FW_F1V1.8.7下，没有未决引脚、协议或分区选择；用户按清单操作即可。如果实际GUI来自其他安装/版本，**版本不一致才阻止按此菜单一次配置**，先报告实际About版本和包版本，不重复询问已定硬件。实物W25完整丝印、LSI/WDG_SW、BKP读回、OLED ACK、最终资源高水位属于相应Phase硬件门槛，不能借此阻止离线生成，也不能跳过写擦/运行前验证。

**证据限定与未关闭事项**：§9.1 已纠正 PB12 “4/4 PASS”的过度表述，不改变 fail-closed 采样策略；历史 8MHz/不同入口的结果不能继承为正式 72MHz 全场景验收。`DOWNLOADED/VERIFYING` 掉电后不可重造未持久 Block0 历史长度；Phase4/7 按现有 ABI 做保守恢复审查，若需改 ABI 必须先提供证据与请求批准，本轮不改字段。

**检查点 A 退出**：两份全部配置表、模块所有权、Keil/资源/受控启动方案已写入本节；停止代码工作，用户配置并生成两工程。预期文件为Bootloader/Bootloader.ioc、MDK-ARM/Bootloader.uvprojx及Application对应文件；Core/Drivers/Middlewares完整。无下载/烧录步骤，无硬件PASS日志要求。

**检查点 B 输入**：用户通知两工程均已生成，提供所在路径（本路径则无需再问）；集中一次读取两.ioc、两.uvprojx、startup/system、main/msp/it、gpio/rcc/usart/spi/i2c/adc/dma、HAL timebase、FreeRTOSConfig/port/heap、版本/Source Group/Include/编译优化/Debug/Download/scatter/IROM/IRAM/VTOR。按“必须修改、推荐修改、无需修改”一次列全，不得每发现一项就让用户重生成。

只有完整一致、必要构建/链接检查真实通过才输出 **`CUBEMX_BASELINE_REVIEW = PASS`**，封存两.ioc/工程/包/编译器/生成源码哈希，然后Phase0当前模块设计→实施。若检查点B先只拿到原始CubeMX骨架，VTOR/Scatter/Phase门禁/Native桥接属于Codex集成必修项，完成且复核前不能签PASS。用户只操作.ioc/GUI/硬件；Codex可在该检查点集成User挂钩和Keil目标，仍不提前开发UART/W25/YModem业务。以后改变基础外设配置必须给出实际原因、影响范围、重新生成风险并获批准。

**用户本轮操作验证与STOP**：先核Help/About和FW包；分别配置所有行并Generate Code；确认两个.ioc路径、PA4初始SET、三IRQ6/0、Boot没有MX_IWDG_Init自动调用、App TIM2时基且没有MX_RTC_Init/MX_IWDG_Init；保存两工程后通知Codex。未找到抑制调用选项、GUI Native模式与本机不同、错误MCU/地址、IWDG自动调用、意外BDRST或丢SWD时停止生成后运行，只记录不一致留待集中审核；不要自行烧录验证。业务预期日志和硬件失败表现由各模块实施轮按Phase专门提供，本轮不存在已经可运行的产品日志。

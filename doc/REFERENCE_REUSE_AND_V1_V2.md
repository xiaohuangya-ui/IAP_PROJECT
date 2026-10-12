# F103 IAP：外部参考代码使用指南与 App V1/V2 交付计划

> **文档定位**：仅作开发时的参考源码导航和演示实施指南，存放于 `IAP_PROJECT/doc/`。不是第四份产品规格，不取代 `DESIGN_SPEC.md`、`ENGINEERING_LESSONS.md`、`AGENTS.md`。**本文所有冲突以当前 `doc/DESIGN_SPEC.md` 为准；硬件细节还必须服从对应 F103/实物器件手册。**
>
> **证据边界**：本指南整理时记载参考了《F103 IAP 项目开发手册 V2.0》及项目概览；这些原文当前不作为已重新审核的附件。H747 三个 Windows 目录仅有路径、模块清单和职责描述，其真实源码仍为 NOT REVIEWED。下表是定位线索，不是移植许可或硬件证据。

## 1. 唯一在用文档集合与职责

| 文件 | 职责 |
|---|---|
| 根 `AGENTS.md` | Codex 行为、所有权、安全门禁与工作契约 |
| `doc/DESIGN_SPEC.md` | 唯一产品规格、Phase 0～9、ABI、安全策略及一次性 CubeMX/Keil 基线 |
| `doc/ENGINEERING_LESSONS.md` | 历史问题、来源等级、已验证修复与未关闭风险 |
| 本指南 | 按模块定位参考源码与同源 V1/V2 构建/演示方法；固定要求以设计书为准 |

原厂手册、硬件图和只读 IAP_REFERENCE 保留。被上述文件吸收的旧独立规则、重复规范和临时快照不另维护；历史工程内部资料不在本轮清理范围。当前不预建产品目录，后续由用户 CubeMX 创建 Bootloader/Application。

## 2. 挑选参考材料的规则

1. **先看需求**：当前 Phase 和模块的接口/ABI/验收条件以 `DESIGN_SPEC.md` 为准，不根据参考例程变更功能。
2. **查故障经验**：从 `ENGINEERING_LESSONS.md` 读取该模块曾发生的问题（O3、HAL/寄存器、16KB、Journal、W25、Jump、DMA 等）。
3. **再找源码**：优先从 `IAP_REFERENCE/` 中已在 F103+ARMCC5 上构建/验证的候选查找；新增 H747 例程作为架构或算法参考，不是移植首选硬件驱动。
4. **先阅读源文件而非照抄表格**：必须核对 `.h/.c`、实际调用关系、旧 HAL/CMSIS/FreeRTOS 版本、内存分配与错误路径；看不到文件就写 `NOT REVIEWED`，不得声称已复用。
5. **只精简本文内容，不删改原始外部源码**：不得直接在 `F:\STM32_Projects\...` 或 `IAP_REFERENCE/` 执行删除/修改；真正迁移时只把当前模块获批准的实现引入正式工程。
6. **选一种处理结果**：`REUSE_WITH_REVIEW` / `ADAPT` / `DESIGN_IDEA_ONLY` / `DO_NOT_COPY`。每次只保留当前模块必要的代码，不复制整库。
7. **迁移后重新证明**：ARM Compiler 5.06 update 7 真正参与链接、Warning/MAP、Host Test、相应 Phase 真机验收独立通过；历史 H747 或旧 F103 PASS 不能继承为新固件 PASS。

## 3. H747 参考工程：保留哪些思想

### 3.1 `F:\STM32_Projects\STM32H747_Projects\04_Architecture_Examples`

| 来源 | 借鉴的核心思想 | F103 推荐目标 | 明确不要复制 |
|---|---|---|---|
| `baremetal-fsm` | **事件驱动状态机**：状态、事件、guard、action 分离；非法事件拒绝 | `Bootloader/User/IAP/` 的 FSM/Recovery/BootPolicy；可用纯 C Host Test | 未经审查的 enum 编号；不能修改现行九状态 ABI |
| `baremetal-layered` | BSP → Driver/Port → Service/Protocol → App 的**单向依赖** | `User/BSP`、`User/Protocol`、`User/IAP`；薄 `main.c` | 为追求通用架构引入复杂运行时多态/反复分配，导致 Boot 16KB 超限 |
| `baremetal-layered_rtos` | **Shell、通知、队列、设备唯一所有者** | Phase 9 `Application/User/App` 三任务 | 不得照搬其任务数量、优先级或中断优先级 |
| `baremetal-timer-tasks` | 非阻塞 tick/软定时器和定期轮询 | Boot 主循环超时/重传计时；App `vTaskDelayUntil` | 不复制与 FreeRTOS Tick 重复的调度器，不在 ISR 中跑事务 |

**优先复用方式**：读数据结构、状态转移、接口依赖和 Host Test 构造方法；优先把设计思想改写为少量 F103 专用纯 C 模块。不要迁移整个 Middleware 目录。

### 3.2 `F:\STM32_Projects\STM32H747_Projects\03_Common_Modules`

| 优先查找 | 可复用部分 | F103 实施限制 |
|---|---|---|
| `USART`、`USART_ringbuffer`、`DMA` | 环形缓冲模型、producer/consumer、超时/错误计数、吞吐测试 | DMA 通道/寄存器/IRQ/IDLE 标志**必须使用 F103 定义**；DMA RX 512B Circular、Ring 2KB；满环**不能静默丢字节后继续把 YModem 包视为有效** |
| `WatchDog`、`RTC`、`Low power` | 监督条件、错误归因、复位/状态接口分离 | F103 的 IWDG LSI 实测和 BKP/RTC 语义为准；不照抄 H7 寄存器或 RAM 保留语义 |
| `Shell`、`TIM` | 命令分发表、有限长度解析、软计时与超时模式 | 当前 `@APP/@IAP` + CRLF、128B 行缓冲、YModem 独占模式；避免大 `printf` |
| `ADC`、`IIC`、`GPIO` | 设备接口划分、参数检查和诊断方法 | F103 ADC1 CH16 片内**结温估算**、SSD1306 I2C1 PB6/PB7 100kHz、PC13 板载 LED；由用户 CubeMX 配置 |
| `KEY` | 非阻塞输入采样、反跳和多次读一致性思路 | PB12 Recovery 优先审查 F103 P1/P1-D2 候选采样机制；历史证据范围见设计书 §9.1，正式 72MHz 全场景需重新验收 |

**不列入本项目必要依赖**：MPU、Cache、QSPI、SDRAM、SDNAND、Fatfs、JSON、USB、LAN8720/LwIP、MQTT、W5500、RGB/LVGL/Touch。它们不是本次 IAP + SSD1306 固定功能，不能为了提高“技术栈数量”引入。

### 3.3 `F:\STM32_Projects\STM32H747_Projects\02_Camera`

优先读用户指出的 `2_Terminal_SingleCore/` 和 `3_Terminal_DualCore/` 中这些职责明确的文件；**先借鉴可靠升级原理，再重新实现 F103 介质适配**：

| H747 候选源码 | 值得参考的思想 | F103 映射与处理 |
|---|---|---|
| `boot_slot.c/.h` | MSP/Reset 向量范围验证、镜像边界、启动目标选择 | F103 `BootPolicy` 与 `Jump`；**仅一个内部 App**，目标固定 `0x08004000`，不复制 Bank/Slot A/B 地址 |
| `boot_eeprom.c/.h`、`boot_meta.c` | active/pending/trial、sequence、CRC、主副本和 fail-closed | F103 **W25 Metadata A/B 72B append-only**，Boot 是唯一写入者；不能沿用 EEPROM/BKPSRAM 持久记录格式 |
| `boot_jump.c` | 先停中断/时基/外设，再 VTOR/MSP/Thumb Reset_Handler | 仅作为审查提纲；F103 采用现行 C07/HW05E 实证与 F103 Cortex-M3 语义，**不复制 CM4 HOLD/DTCM 魔数/双核握手** |
| `boot_slot_copy.c`、`bsp_flash_ram.c` | 写前验证、写后回读、分块进度、超时/看门狗服务 | F103 1KB 页擦除、**16bit 半字编程**；是否 RAM 执行由 F103 真正的 Flash 时序及代码证明，不照搬 H7 FLASHWORD 或 DTCM 执行方案 |
| `app_ota.c` | Begin/Chunk/Finish 三段事务以及分块落盘、CRC、幂等、失败回收 | 作为 **Boot 中 YModem→Download→Verify→Backup→Install** 的事务思想；本项目 App 不能写 W25/Flash 固件 |
| `app_http_ota.c`、Qt `otaclient.*` | 分块会话与错误分类、上位机版本可视化 | **架构参考，不移植协议**；正式 PC 使用标准 YModem Sender + `fw_pack.py`，不做 HTTP/GUI/断点续传 |
| `app_ota_coop.c`、`cm4_ota_hold.c` | 升级前业务停机、外设释放、跨核协作先后顺序 | F103 仅一个 Cortex-M3；App 在 `@APP:UPDATE` 后 UART TX **TC**→BKP_DR1→软件复位，不存在停 CM4/D3 mailbox |

**绝对不可借用**：H747 `0x08020000/0x080A0000/0x081xxxxx` 的地址、双 Bank A/B、CM7/CM4/I-cache/D-cache、DTCM、D3 mailbox、EEPROM BL24C16F 接线、LwIP/HTTP Begin/Chunk/End/Resume、Qt 协议、`FLASH_TYPEPROGRAM_FLASHWORD`、H7 的 NVIC/Flash API。我们的可靠性是**单内部 App + W25 物理旧镜像 Backup + Journal + Trial/Rollback**，不是双内部 Flash 槽。

## 4. 从《F103 IAP 项目开发手册 V2.0》中只保留这些内容

> 该手册自称“唯一参考”，但它存在旧的 Phase 0～8 顺序、H7 专属地址/API、错误健康心跳示例以及 App 在 Boot 期间刷 OLED 等旧内容；**现在必须视为历史参考素材，不能覆盖当前 `DESIGN_SPEC.md`**。其代码附录不是可直接编译的 F103 产品源码。

| 保留为参考 | 原附录/内容 | 如何使用 |
|---|---|---|
| **事件状态机、分层、生产者消费者** | FSM/Layered 思想、YModem 双 FSM | 配置当前 `IAP FSM`/Packet FSM，不引入第二套持久状态编码 |
| **CRC16/XMODEM、CRC32/IEEE 自测向量** | `123456789 → 0x31C3 / 0xCBF43926` | 实现纯 C、主机 golden tests；**Modbus CRC16/A001 不得用在 YModem** |
| **基于数据长度的边界校验** | Header32、块号/CRC、分区保护等 | 固定协议字段、长度与类型按 `DESIGN_SPEC.md` 序列化，不强制将 Header 放入 YModem Block0 |
| **紧凑 Shell 命令表** | `shell_cmd_t` 概念 | CRLF/128B、唯一 ownership、边界检查；避免 `strtok` 在多个上下文复用或溢出 argv |
| **分块事务 + 提交状态** | H747 OTA Begin/Chunk/End 思想 | 只有被 CRC/Journal 证明完成才能进入下一阶段；不能出现“ACK 就等于完整固件可信” |
| **版本与构建输出** | `fromelf --bin` + 版本号 | ARMCC5 真正从 `0x08004000` 导出 App `.bin`，通过 32B Header 包装标准 YModem 文件 |

**必须删除/废弃的旧示例**：H747 跳转函数中写 DTCM 魔数及 CM4 控制；环形缓冲满时“丢最老但继续解析”；Modbus CRC16；H7 `FLASHWORD`；未经版本核对就假设 `HAL_UARTEx_ReceiveToIdle_DMA` 一定存在（本机 FW_F1 V1.8.7/HAL1.1.10 已实际核对存在，采用设计书 §19 的唯一 HAL 回调方案）；App 独自每 500ms 自增健康计数；App/Boot 共享 OLED 下载进度；未核对的 `RCC->RSR`（F103 使用 `RCC->CSR` 的复位标志）；OLED 400kHz（本项目 100kHz）；将动态 FreeRTOS heap 和动态任务栈重复加算；“断点续传扩展”占位。**旧手册的 Phase 0～8 排期不采用：正式路线只以当前设计 Phase 0～9 为准。**

### 两段可保留的极简算法形状（示例，不是直接投产代码）

```c
/* 地址合法性：先减后比较，避免 addr+len 的 uint32_t 溢出。 */
static bool in_region(uint32_t addr, uint32_t len,
                      uint32_t base, uint32_t capacity)
{
    return (addr >= base) && (len <= capacity) &&
           ((addr - base) <= (capacity - len));
}
```

```c
/* ISR/前台之间的 Ring 只允许明确的溢出拒绝；不静默覆盖旧数据。 */
static bool ring_try_push(ring_t *r, uint8_t value)
{
    uint16_t next = (uint16_t)((r->head + 1U) % r->size);
    if (next == r->tail) {
        r->overflow = true; /* 当前 YModem 包必须作废并重新同步 */
        return false;
    }
    r->data[r->head] = value;
    r->head = next;
    return true;
}
```

这些片段仅说明**边界/溢出策略**：实际代码还须审查 ISR 读写原子性、SPSC 内存可见性、ring.size 的有效性、重新同步/超时及 ARMCC5 O3 行为。不得把示意代码未经测试直接视为可用驱动。

## 5. 两个正式 App 固件（V1.0 / V2.0）必须真的构建出来

固定版本、功能与验收要求唯一见设计书 §14.2，本节仅解释构建和演示方法。这是**Phase 9 的产品演示交付物**，不是 Phase 0 的 HW05C 最小测试 App；不创建新 Phase，不提前进入 App 业务开发。始终使用**同一套** FreeRTOS/SSD1306/ADC/IWDG/BKP/串口驱动源码，按版本配置生成独立镜像，禁止维护两份会分叉的 App 驱动。

| 构建 | 固定版本编号 | 屏幕可见特征 | 不可缺少的共同能力 |
|---|---|---|---|
| **APP_V1_RELEASE** | `0x00010000`（1.0） | SSD1306 文字仪表盘：`APP V1 NORMAL`、结温估值、uptime、健康 | FreeRTOS 三任务、UART `@APP:*`、ADC1 CH16、PC13、I2C1、BKP DR1/DR2/DR3、IWDG Trial 接力 |
| **APP_V2_RELEASE** | `0x00020000`（2.0） | **可见而真实的版本变化**：`APP V2 NORMAL`，新增真实业务逻辑：每 1s 将有效结温估值写入上限 60 项的 `int16_t temp_x10` 有界环形历史（120B），OLED 绘制近期趋势折线；仍标明是 Tj 估值 | 同一协议/硬件契约；能够 Trial 3s+5 次真实 Business 进展后写 DR2 请求 Boot 确认 |

**版本一致性**：版本由一个受控构建常量驱动屏幕、`@APP:VERSION`、32B Header 的 firmware_version 和产物清单；不能分别手动改 4 个常量而造成不一致。V1/V2 不允许变更内部 Flash 分区、Metadata 72B ABI、YModem 协议或 BKP 值来制造“升级效果”。两份产物都必须拥有完整启动向量、真实 image_size/CRC32 与唯一构建哈希。

**示例构建产物**（实际文件名以工具真正产出为准）：

```text
Evidence/releases/
  app_v1.bin                 # AXF/fromelf，起始地址 0x08004000
  app_v1_ymodem.bin          # Header32 + app_v1.bin
  app_v2.bin
  app_v2_ymodem.bin
  build_v1.map / build_v2.map
  release_checksums.txt     # App BIN、32B Header、最终 YModem 文件的版本/size/CRC/SHA
```

**严禁为了演示“升级成功”只改 OLED 文本和 PC 端输出**：必须在真机上确认 Flash App 内容及 CRC 确实从 V1 变成 V2；Boot 负责 Download→Backup→Install→Trial→Boot Commit，全流程状态/日志可查询。

### 独立回滚测试固件（仅 Test Target，不是第三个正式产品版本）

在测试构建中为 **V2 使用独立 `APP_V2_HEALTH_FAIL_TEST`**，除受控故障注入外与 V2 相同，但特意使至少一个必需健康条件不成立（例如 Business 不发布成功进展，或模拟 OLED 健康失败）。**不能伪造 DR2 已确认或修改固件版本/协议来实现“回滚”**。产物与 Release 物理隔离，必须经编译标记、打包哈希、用户授权，真机只在已证明 V1 Backup/Recovery 可靠的条件下安装。

## 6. 三条实际验收链，不能混为一次测试

1. **正常升级**：先按安全 Provisioning 流程建立可信 `APP_V1_RELEASE`；V1 OLED/串口正常 → App 发 `@APP:UPDATE` → `BKP_DR1`→Boot 接收 `app_v2_ymodem.bin` → W25 Download 有效 → 备份 V1 整 48KB/双 CRC → 安装 V2 → Trial → `BKP_DR2` → Boot 写 Journal `CONFIRMED` → OLED `APP V2 NORMAL`，串口和 Flash CRC 与 V2 一致。**同一次演示期间 Boot 不变**。
2. **失败回滚**：重新通过**明确授权的受控初始化/夹具恢复**建立 `V1 已 CONFIRMED` 的独立初始场景，再升级 `APP_V2_HEALTH_FAIL_TEST` → 真实 Trial 失败至三次上限 → W25 Backup 恢复 V1 **全部 48KB** → V1 CRC、Metadata、版本及 OLED 恢复。**不能在 V2 已确认成为新 Active 后，误称“下一次回滚应该恢复 V1”**；Backup 存的是“本次安装前已确认版本”。
3. **掉电恢复**：以已确认 V1 为起点，分别在 Download、Backup、Install、首次 Trial、Rollback 的安全注入点断电，要求复位后不跳半写镜像，事务根据 Journal/双 CRC 决定重试或恢复；保留用户亲自采集的前后 Flash/W25/日志/串口证据。

**共同验收门槛**：Boot≤16384B，App V1/V2≤49152B，各自静态+动态峰值 SRAM≤20480B；真实 ARMCC5 AXF/MAP、Warning 可解释、产品安全状态机运行正常；App 健康所需 `≥3000ms + ≥5 次 Business 真实成功心跳 + ADC/OLED/UART/IWDG` 不打折。演示视频不是 CRC/介质证据的替代。

### Keil 中两个正式版本怎样切换

**不要再建第二套 Application CubeMX 工程。** 在同一个正式 `Application/*.uvprojx` 内使用两套明确定义的 Release Target（或同一源码的可复现构建参数）：`APP_V1_RELEASE` 和 `APP_V2_RELEASE`。两个 Target 使用**同一份源文件、HAL/FreeRTOS 包、App scatter/VTOR、编译器与引脚配置**；仅通过一个受控配置头（如 `app_release_config.h`）或构建宏选择版本号与 V2 的温度趋势功能。所有版本相关字符串、`@APP:VERSION` 和 `fw_pack.py` 包头从同一版本常量生成。

首次建立两个 Target 时，逐项检查 `.uvprojx` 的 `Source Group`、宏、IROM `0x08004000/0xC000`、IRAM `0x20000000/0x5000`、O3 与生成文件名，防止同名 AXF/MAP/BIN 被覆盖。**两个 Release 必须分别 Rebuild、导出和实机验收**。`APP_V2_HEALTH_FAIL_TEST` 只能是隔离的 Test Target，不得加入产品 Release 构建。

### 按 Phase 选择参考源（不用通读整个 H747 工程）

| 当前任务 | 第一优先 | 可择优补充 | 迁移结果 |
|---|---|---|---|
| Phase 0 Jump/Recovery | F103 C07/PB12、HW05C/HW05E | H747 `boot_slot.c`/`boot_jump.c` 的**检查顺序思想** | F103 硬件专用的 Policy/Jump；不复制 H7 寄存器 |
| Phase 1～3 UART/DMA/Shell | F103 已验证 USART/环形接收 | `03_Common_Modules/USART_ringbuffer`、`Shell`、`baremetal-layered` | 有界 Ring、CRLF 命令表、唯一 ISR owner |
| Phase 4 YModem/CRC | F103 C04/C05 Header、CRC、YModem | `baremetal-fsm` 的包状态机设计 | 标准 YModem，严格 CRC/XMODEM 与 Block0 文件大小 |
| Phase 5～6 Flash/W25 | F103 W25 驱动、G4B/C06 | H747 `bsp_flash_ram.c` 的验证/超时/分块**原则** | F103 页擦/半字写，W25 Readback/Backup |
| Phase 7～8 Journal/Recovery/Trial | F103 C06/G4C/G4D 和真机证据 | H747 `boot_eeprom.c`、`boot_meta.c` 的 sequence/CRC/双副本思想 | **W25 72B Journal + Boot 提交确认**，不是 H747 EEPROM 双槽 |
| Phase 9 App V1/V2 | F103 App 设计、已有 OLED/I2C 实测、当前 HAL | `baremetal-layered_rtos`、H747 `ADC/IIC/Shell` 的解耦思路 | 一套 FreeRTOS App 源码、两个 Release Target、独立测试变体 |

## 7. Codex 每次开发一个模块的执行模板

请在修改文件前遵循以下固定流程：

1. 从 `doc/DESIGN_SPEC.md` 读取当前模块的固定接口和所属 Phase；从 `doc/ENGINEERING_LESSONS.md` 读取相关历史坑。
2. 只定位对应参考来源（优先 `IAP_REFERENCE/`，然后上述 H747 路径）；外部 `F:\...` 不可读时告知用户复制目标模块的 `.c/.h`，**不要求搬入整套 H747 工程**。
3. 提交一段短迁移结论：`来源文件 → 迁移的思想/函数 → 正式目标文件 → 禁止复制的硬件/协议内容 → 验证方法`。
4. 经审查后直接完成当前模块源码、Host Test、ARMCC5 Rebuild、MAP/Warning 审计；若需要用户 CubeMX 变更必须事前请求，不得擅改 `.ioc`。
5. 只按本 Phase 退出条件验收，绝不把 H747/历史 F103 成功或 CPU 级 SRAM 桥接当成正式产品升级通过。

**简明判断口诀：规格定行为，手册定硬件，历史 F103 提供实证，H747 提供架构与算法灵感，当前新工程的真实构建和真机验收决定最终 PASS。**

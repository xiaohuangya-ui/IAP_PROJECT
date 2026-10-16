# 架构与存储边界

MCU 仅使用 64KiB Flash、20KiB SRAM。Boot 为 0x08000000..0x08003FFF，App 为 0x08004000..0x0800FFFF；不得扩大 Boot 或使用非标容量。

| W25 地址 | 用途及 owner |
|---|---|
| 0x000000..0x00FFFF | Download，Boot |
| 0x010000..0x01FFFF | Backup，Boot |
| 0x020000..0x021FFF | 双扇区 Metadata，Boot |
| 0x022000..0x025FFF | 升级日志，Boot |
| 0x026000..0x125FFF | 温度历史，App 业务任务 |
| 其余 | Reserved，禁写 |

USART1 115200 8N1；DMA RX CH5/TX CH4，ASCII 命令以 CRLF 结束，固件使用单文件标准 YMODEM、无断点续传。包为 32B Header 加 App 镜像，magic 0x46574D47，目标地址 0x08004000；CRC16/XMODEM 与 CRC32/IEEE。准确布局由 Shared 中 Header 编解码实现定义。

升级先持久化 RECEIVING；完整接收并回读验证后才能建立下载有效标记。备份前持久化 BACKING_UP 并清除旧备份有效标记；只有 Download、Backup 均验证可信且 INSTALLING 已持久化，才能擦写内部 App。安装回读校验成功进入 PENDING_CONFIRM。Trial 最多三次，失败按可信 Backup 恢复，无可信路径则停留 Boot。

App 健康条件包含真实业务进展至少 5 次、启动至少 3000ms、ADC/OLED 至少成功一次、UART Ready 且无致命错误；满足后写 BKP 确认请求并复位，由 Boot 复核后提交 CONFIRMED。BKP 不替代 W25 持久状态。IWDG 喂狗受运行健康条件约束。

PB12 为启动时停留 Boot 维护的按键，不是版本切换或下载后安装确认键。当前是先进入 Boot 再下载并自动安装，没有实现“App 接收固件后按键安装”。App 不得写 IAP 分区，Trial 和 UPDATE 阶段不进行历史存储操作。

OLED 使用 I2C1 PB6/PB7；W25 使用 SPI1；温度为 ADC1 CH16 片内结温估算，不是外部环境温度传感器。Boot 不驱动 OLED，停留 Boot 时屏幕不亮不等于设备损坏。

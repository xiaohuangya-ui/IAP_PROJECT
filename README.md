# STM32F103 UART IAP 与 FreeRTOS 温度记录系统

基于 STM32F103C8T6、W25Q64BV 和 SSD1306，提供 UART/YMODEM 固件升级、旧镜像备份、Trial 健康确认、异常恢复及温度历史记录。Boot 裸机运行，App 使用 FreeRTOS。

## 工程入口

| 工程 | Keil Target | ROM / 静态 RAM |
|---|---|---|
| Bootloader/MDK-ARM/Bootloader.uvprojx | BOOT_RELEASE | 16372 / 9856 B |
| Application/MDK-ARM/Application_Phase9.uvprojx | APP_V1_RELEASE | 25244 / 13168 B |
| 同上 | APP_V2_RELEASE | 25632 / 13288 B |

工具链为 ARM Compiler 5.06 update 7 build 960。上述三个正式 Target 已完整 Rebuild，均为 0 Error / 0 Warning。Boot 16KB 分区仅余 12B。Tests/keil 中 HealthFail 为隔离故障测试，不能作为正常升级包。

## 主要实现

- Boot 独占固件 Download、Backup、Metadata 和升级日志，接收完成后回读校验，再备份旧 App、安装和试运行；App 通过 BKP 请求升级及健康确认。
- FreeRTOS 三任务：串口通信优先级 3、升级控制 2、业务 1；任务通知协调升级交接，等待 USART TC 后复位，健康条件控制 IWDG 喂狗。
- App 业务任务独占独立 1MiB NOR 历史区，按分钟记录温度，采用 CRC 提交、写后回读、分批扫描和环形覆盖。最多 32512 条，约 22.58 天；查询返回最近至多 4 条。
- CRC/Header 与 W25 物理驱动复用单一实现，业务区域和 IAP 区域权限隔离。

## 使用与验证

当前包在 [Firmware](Firmware)，身份见 [IDENTITY.json](Firmware/IDENTITY.json)。V1/V2 版本号为 1.0/2.0，请结合 SHA256 区分不同构建。同版本升级允许，正常协议拒绝降级。

- [构建与离线测试](doc/REPRODUCING.md)
- [串口升级与硬件测试](doc/USER_GUIDE.md)
- [架构与存储边界](doc/DESIGN_SPEC.md)
- [验证范围与已知问题](doc/VALIDATION.md)

仓库保留厂商源码自带许可声明；不额外授予其未声明的许可。原始数据手册置于 doc，仅作硬件参考。

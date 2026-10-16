# 验证范围与已知问题

## 离线

三个正式 Target 由 AC5.06u7 build960 完整 Rebuild，均 0 Error / 0 Warning。14 组离线回归通过，包含实际 C 状态机、协议、UART、存储、Trial/回滚及 App 任务运行场景。写裂、环回等主机故障注入不等于真实硬件掉电验收。当前固件大小及 SHA256 见 Firmware/IDENTITY.json。

## 用户提供的串口观察（2026-10-16）

以下来自用户提交的串口文本，未独立连接硬件或读回新 App，也不据此认定 OLED 验收通过。

- 收到 YMODEM 完成消息，随后 APP HEALTH CONFIRM REQUEST、APP VER=2.0 MODE=NORMAL。
- 16:27:53 STORE STATE=2 SEQ=3 CORRUPT=0 SCANNED=256 BYTES=1048576。
- 16:28:55 HISTORY 显示 SEQ2..5，记录间隔约 60～61 秒。
- 按普通复位测试流程，初次查询 STATE=0/COUNT=0，随后 STATE=1 恢复相同 SEQ2..5；16:30:17 STATE=2 SEQ=5。
- 16:31:33 新增 SEQ6，UP_MS=75504，旧记录仍在。支持正常复位后记录保留、扫描恢复和后续周期记录。

原文关键记录：

```text
HISTORY SEQ=3 VER=131072 UP_MS=222107 TJ_X10=-380 RAW=2111
HISTORY SEQ=4 VER=131072 UP_MS=283107 TJ_X10=-391 RAW=2117
HISTORY SEQ=5 VER=131072 UP_MS=343107 TJ_X10=-391 RAW=2117
HISTORY SEQ=6 VER=131072 UP_MS=75504 TJ_X10=-380 RAW=2111
```

## OPEN

温度数值约 -38～-39℃；若处于室温则明显不合理，需要核查 ADC 与换算，不能宣称温度准确。真实写入中掉电、满环覆盖、最坏擦写/OLED 并发与 IWDG、任务栈高水位、最小堆、独立 V1 回滚、PB12 全场景仍待硬件验证。历史图标版安装曾有读回证据，但屏幕观察缺失的 FAIL 不因本次串口记录而消除。

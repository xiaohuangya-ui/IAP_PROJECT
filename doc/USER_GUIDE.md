# 串口升级与历史记录

保留当前有效 Boot 和 Metadata。新板首次 provisioning 工具尚未适配当前镜像，不可直接用旧工装；也不能只覆盖 App HEX，造成 Metadata 与内部镜像不一致。

使用 Tera Term，关闭其他占用 COM 的软件，选择实际串口，设置 115200、8N1、无流控，发送换行为 CR+LF。每条命令手动输入后按 Enter。

1. `@APP:VERSION`：确认 NORMAL。
2. `@APP:UPDATE`：等待 APP UPDATE REQUESTED、APP ENTER BOOT。
3. `@IAP:STATUS`：记录状态。
4. 先准备好 Firmware/APP_V2_RELEASE/APP_V2_RELEASE.fw 的本机完整路径。
5. `@IAP:ENTER`：看到 C 后在 30 秒内选择 文件→传输→YMODEM→发送，选择完整 .fw；不要使用普通发送文件或 XMODEM。
6. 保持供电，传输完成后等待约 20 秒，供自动备份、安装、Trial 和确认。传输完成消息本身不代表确认完成。
7. `@APP:VERSION`、`@APP:TEMP`、`@APP:STORE`。健康运行约 80 秒后查询 `@APP:HISTORY`。

STORE 状态 0=等待、1=扫描、2=就绪、3=锁存错误。SCANNED=256 表示扇区头扫描完成，仍可能继续扫描正文。CORRUPT 只代表已扫描范围。HISTORY 返回最近至多 4 条，UP_MS 是生成记录时的本次启动时间，不是日期；TJ_X10 单位 0.1℃。

PB12 释放后按 RESET，等待扫描结束，历史应保留并继续增加序号。此测试不等于写入中掉电测试。STATE=3 时保留现场，不自动格式化或清保护位。

YMODEM 超时会返回 ERR YMODEM；协议会丢弃直到下一个 CRLF，先单独按 Enter 同步，再查 STATUS。STATE=2 表示接收中，复位后停留 Boot 符合设计。ERR INVALID 表示非法字符或换行；ERR COMMAND 表示当前角色不支持该命令。避免输入退格控制字符。

原串口助手使用 HEX 时关闭额外追加换行，只发送一次含 CRLF 的命令：

| 命令 | HEX |
|---|---|
| VERSION | 40 41 50 50 3A 56 45 52 53 49 4F 4E 0D 0A |
| STORE | 40 41 50 50 3A 53 54 4F 52 45 0D 0A |
| HISTORY | 40 41 50 50 3A 48 49 53 54 4F 52 59 0D 0A |

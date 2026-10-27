# 芯片与器件资料

原来的 `doc/` 目录放有完整芯片参考手册 PDF，体积较大，而且不是工程编译所需。为了让仓库主要保留我自己的代码、实验和测试文档，这里改成**原厂资料入口**，不再直接分发这些 PDF。

| 器件/资料 | 查询入口 | 建议关注 |
| --- | --- | --- |
| STM32F103（DS5319 数据手册、RM0008 参考手册） | [ST STM32F103 官方文档页](https://www.st.com/en/microcontrollers-microprocessors/stm32f103/documentation.html) | 标准 Flash/RAM 容量、寄存器、Flash 擦写、USART、DMA |
| W25Q64BV 串行 NOR Flash | [Winbond 官方网站](https://www.winbond.com/)（搜索 W25Q64BV 或对应芯片型号） | 擦写粒度、写入时序、状态寄存器、掉电场景 |
| SSD1306 OLED 控制器 | [Solomon Systech 官方网站](https://www.solomon-systech.com/)（搜索 SSD1306） | 初始化时序、I²C 指令与显示内存 |

**注意：** W25Q64 不同后缀版本的手册可能有时序和状态位差异，复现实验时应核对手头芯片的丝印型号。SSD1306 还应结合实际 OLED 模组的引脚和供电方式确认。

第三方 HAL、CMSIS、FreeRTOS 等源文件保留其原始许可信息。这些依赖并非个人原创代码；当前仓库也没有对全树另行授权的统一 LICENSE。下载、引用或二次发布前，请遵守各文件原有的许可证和器件厂商的文档使用条件。

[返回文档导航](README.md)

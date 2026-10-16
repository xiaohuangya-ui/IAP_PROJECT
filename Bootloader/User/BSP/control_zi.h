#ifndef BOOT_CONTROL_ZI_H
#define BOOT_CONTROL_ZI_H
/* 小控制量独立于大缓冲分节，保留紧凑寻址；由本Target启动路径清零（正式Boot为startup_product自有逐字清零）。
 * HOST不模拟链接节布局；普通读写/volatile及初始化语义保持原样。 */
#if defined(__CC_ARM) && !defined(__GNUC__)
#define BOOT_CONTROL_ZI __attribute__((section("BOOT_CONTROL_ZI"),zero_init))
#else
#define BOOT_CONTROL_ZI
#endif
#endif

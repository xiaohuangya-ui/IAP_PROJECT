#ifndef APP_VERSION_H
#define APP_VERSION_H
extern const unsigned int app_firmware_version;
/* 只有Target定义版本；缺失/未知配置立即构建失败，禁止隐含版本。 */
#if APP_RELEASE_VERSION == 1
#define APP_FIRMWARE_VERSION 0x00010000UL
#define APP_VERSION_TEXT "APP V1 "
#elif APP_RELEASE_VERSION == 2
#define APP_FIRMWARE_VERSION 0x00020000UL
#define APP_VERSION_TEXT "APP V2 "
#else
#error Explicit APP_RELEASE_VERSION=1 or 2 required
#endif
#if defined(APP_HEALTH_FAIL_TEST) && !defined(APP_TEST_TARGET)
#error Health failure injection is forbidden in Release
#endif
#endif

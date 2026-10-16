#ifndef APP_OLED_PORT_H
#define APP_OLED_PORT_H
#include <stdint.h>
/* I2C1/PB6/7固定端口，HAL地址0x78；20ms超时，不在ISR/临界区调用。
 * 错误后只做外设软件复位，不对SCL/SDA发送未授权的人工脉冲。 */
int AppOled_Write(void *context,uint8_t control,const uint8_t *data,uint16_t size);
#endif

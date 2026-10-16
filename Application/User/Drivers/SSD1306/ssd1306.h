#ifndef APP_SSD1306_H
#define APP_SSD1306_H
#include <stdint.h>
/* Business单一owner；一个1024B framebuffer。所有绘图越界裁剪。
 * 端口返回1为成功；通用驱动严格调用传入端口，不隐式替换自定义端口。 */
typedef int (*SsdWrite)(void *ctx,uint8_t control,const uint8_t *data,uint16_t size);
void Ssd_Bind(SsdWrite write,void *ctx);
int Ssd_Init(void);
void Ssd_Clear(void);
void Ssd_Pixel(int x,int y);
void Ssd_Text(uint32_t x,uint32_t y,const char *text);
int Ssd_Flush(void);
const uint8_t *Ssd_Frame(void);
#endif

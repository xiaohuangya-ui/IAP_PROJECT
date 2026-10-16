#include "ssd1306.h"
#include <string.h>
static uint8_t frame[1024];
static SsdWrite port;
static void *context;
/* 本项目自有5x7列字模，6x8格；仅仪表盘ASCII，未知字符画问号。 */
static const char alphabet[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ .:-~/";
static const uint8_t glyph[][5]={
 {62,81,73,69,62},{0,66,127,64,0},{98,81,73,73,70},{34,65,73,73,54},
 {24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},
 {54,73,73,73,54},{6,73,73,41,30},
 {126,9,9,9,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
 {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
 {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
 {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
 {62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},
 {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
 {3,4,120,4,3},{97,81,73,69,67},{0,0,0,0,0},{0,96,96,0,0},
 {0,54,54,0,0},{8,8,8,8,8},{2,1,2,4,2},{32,16,8,4,2}};
void Ssd_Bind(SsdWrite write,void *ctx){port=write;context=ctx;}
int Ssd_Init(void)
{
    static const uint8_t init[]={0xAE,0xD5,0x80,0xA8,0x3F,0xD3,0,0x40,
      0x8D,0x14,0x20,0,0xA1,0xC8,0xDA,0x12,0x81,0x7F,0xD9,0xF1,0xDB,0x20,0xA4,0xA6,0xAF};
    /* SSD1306 Rev1.1 表9-1及10.1.19：VCOMH仅列00/20/30；
     * 采用复位默认20（约0.77 VCC），不向未定义的40编码赋予含义。
     * 水平寻址由Flush重新设置窗口，不能混用页模式定位指令。 */
    return port && port(context,0U,init,sizeof(init));
}
void Ssd_Clear(void){memset(frame,0,sizeof(frame));}
void Ssd_Pixel(int x,int y)
{if(x>=0 && x<128 && y>=0 && y<64){frame[(uint32_t)y/8U*128U+(uint32_t)x]|=(uint8_t)(1U<<((uint32_t)y%8U));}}
void Ssd_Text(uint32_t x,uint32_t y,const char *text)
{
    uint32_t c,r;const char *found;uint8_t bits;
    if(!text || x>=128U || y>=64U){return;}
    while(*text && x<=122U){
        found=strchr(alphabet,*text++);
        for(c=0;c<5U;++c){
            bits=found?glyph[found-alphabet][c]:(uint8_t)(c==2U?0x59U:1U);
            for(r=0;r<7U;++r){if(bits&(1U<<r)){Ssd_Pixel((int)(x+c),(int)(y+r));}}
        }
        x+=6U;
    }
    /* 限制为完整6像素字符，最后不足一格时显式截断。 */
}
int Ssd_Flush(void)
{
    static const uint8_t window[]={0x20,0,0x21,0,127,0x22,0,7};uint32_t i;
    if(!port || !port(context,0U,window,sizeof(window))){return 0;}
    for(i=0;i<1024U;i+=32U){if(!port(context,0x40U,frame+i,32U)){return 0;}}
    return 1;
}
const uint8_t *Ssd_Frame(void){return frame;}

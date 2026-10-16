#ifndef WIRE_CODEC_H
#define WIRE_CODEC_H
#include <stdint.h>
#include <string.h>
/* 单一LE32标量owner：至少4B普通内存，无对齐要求。
 * 固定AC5小端目标用memcpy表达合法的标量访问；其他平台显式LE。
 * 不做结构体序列化，不建立任何镜像/介质可信性，无IO和缓存。 */
static __inline uint32_t Wire_ReadLE32(const uint8_t *p)
{
#if defined(__CC_ARM) && !defined(__BIG_ENDIAN)
    uint32_t value;memcpy(&value,p,sizeof(value));return value;
#else
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
#endif
}
static __inline void Wire_WriteLE32(uint8_t *p,uint32_t value)
{
#if defined(__CC_ARM) && !defined(__BIG_ENDIAN)
    memcpy(p,&value,sizeof(value));
#else
    p[0]=(uint8_t)value;p[1]=(uint8_t)(value>>8);
    p[2]=(uint8_t)(value>>16);p[3]=(uint8_t)(value>>24);
#endif
}
#endif

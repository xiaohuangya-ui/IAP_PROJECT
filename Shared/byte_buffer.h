#ifndef BYTE_BUFFER_H
#define BYTE_BUFFER_H
#include <stdint.h>
/* 纯RAM字节谓词，不读取/缓存介质可信状态。length>0时buffer必须可读；
 * 空范围返回1。调用者仍独立完成介质读取，错误码和valid提交由原owner决定。 */
int Bytes_AllFF(const uint8_t *buffer,uint32_t length);
#endif

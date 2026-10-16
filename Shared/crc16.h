#ifndef CRC16_H
#define CRC16_H
#include <stdint.h>
/* CRC16/XMODEM，init=0，poly1021，非反射，wire高字节先发；纯算法。 */
uint16_t CRC16_Xmodem(const uint8_t *data,uint16_t length);
#endif

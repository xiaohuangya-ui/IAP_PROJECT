#ifndef JUMP_H
#define JUMP_H
#include "image_contract.h"
typedef struct { ImageIdentity identity; uint32_t msp, pc; } JumpTarget;
/* 简介：前台Prepare，可失败但不拆外设。调用者授予可信版本/长度/CRC，
 * 验证实像/向量、线程/MSP/屏蔽状态、UART TC/DMA、SPI及W25 WIP/WEL。
 * 返回1成功（out仅在成功时有效），0拒绝；无Flash/NOR/BKP写入。 */
int Jump_Prepare(const ImageIdentity *trusted, JumpTarget *out);
/* 简介：终端Commit，再验证后屏蔽IRQ；一旦开始拆环境仅允许复位或跳App。
 * 参数：Prepare所得快照，生命周期覆盖调用；不可从ISR/PSP调用。
 * 返回值：无，尾段纯汇编换MSP并BX，绝无普通C返回。 */
__declspec(noreturn) void Jump_Commit(const JumpTarget *target);
#endif

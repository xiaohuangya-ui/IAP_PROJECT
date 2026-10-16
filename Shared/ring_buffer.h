#ifndef RING_BUFFER_H
#define RING_BUFFER_H
#include <stdint.h>
#define RING_SIZE 2048U
#define RING_CAPACITY (RING_SIZE - 1U)
/* 单生产者/单消费者：uint16位置，一格留空；存储属owner，不动态分配。 */
typedef struct {
    uint8_t data[RING_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer;
/* 简介：初始化/丢弃整队列。仅在生产者停止或IRQ被owner屏蔽时调用。 */
void Ring_Reset(RingBuffer *ring);
/* 参数：byte为完整接收字节。返回1入队；0满，绝不覆盖未消费字节。ISR唯一生产者。 */
int Ring_Push(RingBuffer *ring, uint8_t byte);
/* 返回1并写byte；0空或空指针。仅前台唯一消费者。 */
int Ring_Pop(RingBuffer *ring, uint8_t *byte);
uint16_t Ring_Count(const RingBuffer *ring);
#endif

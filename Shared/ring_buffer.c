#include "ring_buffer.h"
#ifdef UART_HOST_TEST
#define RING_BARRIER() __sync_synchronize()
#else
#include "stm32f1xx.h"
#define RING_BARRIER() __DMB()
#endif
void Ring_Reset(RingBuffer *ring)
{
    ring->head = 0U;
    ring->tail = 0U;
    RING_BARRIER();
}
int Ring_Push(RingBuffer *ring, uint8_t byte)
{
    uint16_t head = ring->head;
    uint16_t next = (uint16_t)((head + 1U) & (RING_SIZE - 1U));
    if (next == ring->tail) { return 0; }
    /* 先写字节再发布head，消费者不能看到尚未写好的数据。 */
    ring->data[head] = byte;
    RING_BARRIER();
    ring->head = next;
    return 1;
}
int Ring_Pop(RingBuffer *ring, uint8_t *byte)
{
    uint16_t tail = ring->tail;
    if ((byte == 0) || (tail == ring->head)) { return 0; }
    RING_BARRIER();
    *byte = ring->data[tail];
    RING_BARRIER();
    ring->tail = (uint16_t)((tail + 1U) & (RING_SIZE - 1U));
    return 1;
}
uint16_t Ring_Count(const RingBuffer *ring)
{
    return (uint16_t)((ring->head - ring->tail) & (RING_SIZE - 1U));
}

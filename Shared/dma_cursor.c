#include "dma_cursor.h"
int DmaCursor_Advance(uint16_t *last, uint16_t remaining, uint16_t *delta)
{
    uint16_t pos, n;
    if (!last || !delta || *last >= 512U || remaining > 512U) { return 0; }
    pos = (uint16_t)((512U - remaining) & 511U);
    n = (uint16_t)((pos - *last) & 511U);
    if (n > 256U) { return 0; }
    *last = pos; *delta = n;
    return 1;
}

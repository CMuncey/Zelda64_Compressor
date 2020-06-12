#ifndef Z64DMA_H
#define Z64DMA_H

#include <stdint.h>
#include "bSwap.h"
#include "z64consts.h"

/* DMA table entry structure */
typedef struct
{
    uint32_t startV;
    uint32_t   endV;
    uint32_t startP;
    uint32_t   endP;
}
z64dma_t;

uint32_t findTable(uint8_t*);
void     getTableEnt(z64dma_t*, uint32_t*, uint32_t);

#endif

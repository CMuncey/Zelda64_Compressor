#include "z64dma.h"

uint32_t findTable(uint8_t* argROM)
{
    uint32_t i;
    uint32_t* tempROM;

    tempROM = (uint32_t*)argROM;

    /* Start at the end of the makerom (0x10600000) */
    /* Look for dma entry for the makeom */
    /* Should work for all Zelda64 titles */
    for(i = 1048; i+4 < UINTSIZE; i += 4)
    {
        if(tempROM[i] == 0x00000000)
            if(tempROM[i+1] == 0x60100000)
                return(i * 4);
    }

    return(0);
}

void getTableEnt(z64dma_t* tab, uint32_t* files, uint32_t i)
{
    tab->startV = bSwap32(files[i*4]);
    tab->endV   = bSwap32(files[(i*4)+1]);
    tab->startP = bSwap32(files[(i*4)+2]);
    tab->endP   = bSwap32(files[(i*4)+3]);
}

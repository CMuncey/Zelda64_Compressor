#ifndef Z64YAZ0_H
#define Z64YAZ0_H

#include <string.h>
#include "bSwap.h"

uint32_t RabinKarp(uint8_t*, int, int, uint32_t*);
uint32_t findBest(uint8_t*, int, int, uint32_t*, uint32_t*, uint32_t*, uint8_t*);
int      yaz0_internal(uint8_t*, int, uint8_t*);
void     yaz0_encode(uint8_t*, int, uint8_t*, int*);

#endif

#ifndef BSWAP_H
#define BSWAP_H

#include <stdint.h>

#if BYTE_ORDER == LITTLE_ENDIAN
#define bSwap32(x) _bSwap32(x)
#define bSwap16(x) _bSwap16(x)

#elif BYTE_ORDER == BIG_ENDIAN
#define bSwap32(x) (x)
#define bSwap16(x) (x)

#endif

uint32_t _bSwap32(uint32_t a);
uint16_t _bSwap16(uint16_t a);

#endif

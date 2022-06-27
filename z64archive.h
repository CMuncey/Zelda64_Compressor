#ifndef Z64ARCHIVE_H
#define Z64ARCHIVE_H

#include <stdlib.h>
#include <stdio.h>
#include <openssl/md5.h>

#include "z64dma.h"

typedef struct
{
    uint32_t  fileCount;
	uint32_t* refSize;
    uint32_t* srcSize;
    uint8_t** src;
	uint8_t** ref;
}
z64archive_t;

void makeArchive(uint8_t*, uint8_t*);
void readArchive(z64archive_t*, FILE*);

#endif

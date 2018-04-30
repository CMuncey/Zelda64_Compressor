#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "tables.h"
#include "readwrite.h"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

/* internal declarations */
u32 simpleEnc(u8* src, int size, int pos, u32 *pMatchPos);
u32 nintendoEnc(u8* src, int size, int pos, u32 *pMatchPos);
int yaz0_encode_internal(u8* src, int srcSize, u8 * Data);
void yaz0_decode_internal (u8 *src, u8 *dst, int uncompressedSize);

int
yaz0_get_size (u8 * src)
{
    return U32(src + 0x4);
}

void
yaz0_decode (u8 *src, u8 *dst, int maxsize)
{
    yaz0_decode_internal(src + 0x10, dst, maxsize );
}

void
yaz0_decode_internal (u8 *src, u8 *dst, int uncompressedSize)
{
  int srcPlace = 0, dstPlace = 0; /*current read/write positions*/
  
  u32 validBitCount = 0; /*number of valid bits left in "code" byte*/
  u8 currCodeByte = 0;
  
  while(dstPlace < uncompressedSize)
  {
    /*read new "code" byte if the current one is used up*/
    if(!validBitCount)
    {
      currCodeByte = src[srcPlace];
      ++srcPlace;
      validBitCount = 8;
    }
    
    if(currCodeByte & 0x80)
    {
      /*straight copy*/
      dst[dstPlace] = src[srcPlace];
      dstPlace++;
      srcPlace++;
    }
    else
    {
      /*RLE part*/
      u8 byte1 = src[srcPlace];
      u8 byte2 = src[srcPlace + 1];
      srcPlace += 2;
      
      u32 dist = ((byte1 & 0xF) << 8) | byte2;
      u32 copySource = dstPlace - (dist + 1);

      u32 numBytes = byte1 >> 4;
      if(numBytes)
        numBytes += 2;
      else
      {
        numBytes = src[srcPlace] + 0x12;
        srcPlace++;
      }

      /*copy run*/
      int i;
      for(i = 0; i < numBytes; ++i)
      {
        dst[dstPlace] = dst[copySource];
        copySource++;
        dstPlace++;
      }
    }
    
    /*use next bit from "code" byte*/
    currCodeByte <<= 1;
    validBitCount-=1;    
  }
}

u32 toDWORD(u32 d)
{
  u8 w1 = d & 0xFF;
  u8 w2 = (d >> 8) & 0xFF;
  u8 w3 = (d >> 16) & 0xFF;
  u8 w4 = d >> 24;
  return (w1 << 24) | (w2 << 16) | (w3 << 8) | w4;
}

// simple and straight encoding scheme for Yaz0
u32
simpleEnc(u8* src, int size, int pos, u32 *pMatchPos)
{
  int startPos = pos - 0x1000, j, i;
  int smp = size - pos;
  u32 numBytes = 1;
  u32 matchPos = 0;

  if (startPos < 0)
    startPos = 0;

  if(smp > 0x111)
    smp = 0x111;

  for (i = startPos; i < pos; i++)
  {
    for (j = 0; j < smp; j++)
    {
      if (src[i+j] != src[j+pos])
        break;
    }
    if (j > numBytes)
    {
      numBytes = j;
      matchPos = i;
    }
  }
  *pMatchPos = matchPos;
  if (numBytes == 2)
    numBytes = 1;
  return numBytes;
}



// a lookahead encoding scheme for ngc Yaz0
u32
nintendoEnc(u8* src, int size, int pos, u32 *pMatchPos)
{
  u32 numBytes = 1;
  static u32 numBytes1;
  static u32 matchPos;
  static int prevFlag = 0;

  // if prevFlag is set, it means that the previous position was determined by look-ahead try.
  // so just use it. this is not the best optimization, but nintendo's choice for speed.
  if (prevFlag == 1) {
    *pMatchPos = matchPos;
    prevFlag = 0;
    return numBytes1;
  }
  prevFlag = 0;
  numBytes = simpleEnc(src, size, pos, &matchPos);
  *pMatchPos = matchPos;

  // if this position is RLE encoded, then compare to copying 1 byte and next position(pos+1) encoding
  if (numBytes >= 3) {
    numBytes1 = simpleEnc(src, size, pos+1, &matchPos);
    // if the next position encoding is +2 longer than current position, choose it.
    // this does not guarantee the best optimization, but fairly good optimization with speed.
    if (numBytes1 >= numBytes+2) {
      numBytes = 1;
      prevFlag = 1;
    }
  }
  return numBytes;
}

int
yaz0_encode_internal(u8* src, int srcSize, u8 * Data)
{
  u8 dst[24];    // 8 codes * 3 bytes maximum
  int dstSize = 0;
  int i;
  int pos=0;
  int srcPos=0, dstPos=0;
  
  u32 validBitCount = 0; //number of valid bits left in "code" byte
  u8 currCodeByte = 0;
  while(srcPos < srcSize)
  {
    u32 numBytes;
    u32 matchPos;
    u32 srcPosBak;

    numBytes = nintendoEnc(src, srcSize, srcPos, &matchPos);
    if (numBytes < 3)
    {
      //straight copy
      dst[dstPos] = src[srcPos];
      dstPos++;
      srcPos++;
      //set flag for straight copy
      currCodeByte |= (0x80 >> validBitCount);
    }
    else 
    {
      //RLE part
      u32 dist = srcPos - matchPos - 1; 
      u8 byte1, byte2, byte3;

      if (numBytes >= 0x12)  // 3 byte encoding
      {
        byte1 = 0 | (dist >> 8);
        byte2 = dist & 0xff;
        dst[dstPos++] = byte1;
        dst[dstPos++] = byte2;
        // maximum runlength for 3 byte encoding
        if (numBytes > 0xff+0x12)
          numBytes = 0xff+0x12;
        byte3 = numBytes - 0x12;
        dst[dstPos++] = byte3;
      } 
      else  // 2 byte encoding
      {
        byte1 = ((numBytes - 2) << 4) | (dist >> 8);
        byte2 = dist & 0xff;
        dst[dstPos++] = byte1;
        dst[dstPos++] = byte2;
      }
      srcPos += numBytes;
    }
    validBitCount++;
    //write eight codes
    if(validBitCount == 8)
    {
      Data[pos] = currCodeByte;
      pos++;
      for (i=0;i</*=*/dstPos;pos++,i++)
        Data[pos] = dst[i];
      dstSize += dstPos+1;

      srcPosBak = srcPos;
      currCodeByte = 0;
      validBitCount = 0;
      dstPos = 0;
    }
  }
  if(validBitCount > 0)
  {
    Data[pos] = currCodeByte;
    pos++;
    for (i=0;i</*=*/dstPos;pos++,i++)
      Data[pos] = dst[i];
    dstSize += dstPos+1;

    currCodeByte = 0;
    validBitCount = 0;
    dstPos = 0;
  }
    
  return dstSize;
}

void
yaz0_encode(u8 * src, int src_size, u8 *dst, int *dst_size )
{
  //check for minimum size
  if(*dst_size < src_size + 0x20)
  {
		perror("yaz0_encode: Bad size\n");
      *dst_size = -1;
      return;
  }
  
  // write 4 bytes yaz0 header
  memcpy(dst, "Yaz0", 4);
  
  // write 4 bytes uncompressed size
  W32(dst + 4, src_size);
  
  // write 8 bytes unused dummy 
  memset(dst + 8, 0, 8);
  
  //encode
  *dst_size = yaz0_encode_internal(src, src_size, dst + 16);
}

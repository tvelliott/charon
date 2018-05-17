//MIT License
//
//Copyright (c) 2018 tvelliott
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#include <stdio.h>
#include <stdint.h>
#include "crc.h"

uint32_t crc32_val = 0;

#define CRC32_POLYNOMIAL 0xEDB88320L  //common poly used in 32-bit crc calculations

#define MAX_DUPES 256
static uint32_t duplicates[MAX_DUPES];
static int dup_idx;
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
static uint32_t CRC32Value(uint16_t i)
{
  int16_t j;
  uint32_t ulCRC;

  ulCRC = i;
  for ( j = 8 ; j > 0; j-- ) {
    if ( ulCRC & 1 )
      ulCRC = ( ulCRC >> 1 ) ^ CRC32_POLYNOMIAL;
    else
      ulCRC >>= 1;
  }
  return ulCRC;
}
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
uint32_t crc32_range( uint8_t *ucBuffer, int32_t ulCount)
{
  uint32_t ulTemp1;
  uint32_t ulTemp2;

  while ( ulCount-- != 0 ) {
    ulTemp1 = ( crc32_val >> 8 ) & 0x00FFFFFFL;
    ulTemp2 = CRC32Value( ((uint16_t) crc32_val ^ *ucBuffer++ ) & 0xff );
    crc32_val = ulTemp1 ^ ulTemp2;
  }
  return crc32_val;
}


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void add_dup(uint32_t pid)
{
  duplicates[dup_idx++] = pid;
  dup_idx &= (MAX_DUPES-1);
}
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
int is_dup(uint32_t pid)
{
  int i;
  for(i=0; i<MAX_DUPES; i++) {
    if(duplicates[i]==pid) return 1;
  }

  return 0;
}
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void clear_duplicates() {
  int i;
  for(i=0; i<MAX_DUPES; i++) {
    duplicates[i]=0;
  }
}

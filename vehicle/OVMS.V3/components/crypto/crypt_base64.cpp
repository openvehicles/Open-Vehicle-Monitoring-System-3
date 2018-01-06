/*********************************************************************
MODULE NAME:    b64.c

ORIGIN:         http://base64.sourceforge.net/b64.c

AUTHOR:         Bob Trower 08/04/01

PROJECT:        Crypt Data Packaging

COPYRIGHT:      Copyright (c) Trantor Standard Systems Inc., 2001

NOTE:           This source code may be used as you wish, subject to
                the MIT license.  See the LICENCE section below.

LICENCE:        Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.

                Permission is hereby granted, free of charge, to any person
                obtaining a copy of this software and associated
                documentation files (the "Software"), to deal in the
                Software without restriction, including without limitation
                the rights to use, copy, modify, merge, publish, distribute,
                sublicense, and/or sell copies of the Software, and to
                permit persons to whom the Software is furnished to do so,
                subject to the following conditions:

                The above copyright notice and this permission notice shall
                be included in all copies or substantial portions of the
                Software.

                THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
                KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
                WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
                PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
                OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
                OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
                OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
                SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

VERSION HISTORY:
                Bob Trower 08/04/01 -- Create Version 0.00.00B
*********************************************************************/

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string>
#include "crypt_base64.h"

// Translation Table as described in RFC1113
const uint8_t cb64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Translation Table to decode (created by author)
const uint8_t cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

void encodeblock( uint8_t *in, uint8_t *out, int len )
  {
  out[0] = cb64[ in[0] >> 2 ];
  out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
  out[2] = (uint8_t) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
  out[3] = (uint8_t) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
  }

char *base64encode(const uint8_t *inputData, int inputLen, uint8_t *outputData)
  {
  uint8_t in[4];
  uint8_t out[4];
  int len = 0;
  int k;
  for (k=0;k<inputLen;k++)
    {
    in[len++] = inputData[k];
    if (len==3)
      {
      // Block is full
      encodeblock(in, out, 3);
      for (len=0;len<4;len++) *outputData++ = out[len];
      len = 0;
      }
    }
  if (len>0)
    {
    for (k=len;k<3;k++) in[k]=0;
    encodeblock(in, out, len);
    for (len=0;len<4;len++) *outputData++ = out[len];
    }
  *outputData = 0;
  return (char*)outputData;
  }

std::string base64encode(const std::string inputData)
  {
  uint8_t in[4];
  uint8_t out[4];
  int len = 0;
  int k;
  std::string outputData;
  outputData.reserve(howmany(inputData.size(), 3) * 4);
  for (k=0;k<inputData.size();k++)
    {
    in[len++] = inputData[k];
    if (len==3)
      {
      // Block is full
      encodeblock(in, out, 3);
      outputData.append((char*) out, 4);
      len = 0;
      }
    }
  if (len>0)
    {
    for (k=len;k<3;k++) in[k]=0;
    encodeblock(in, out, len);
    outputData.append((char*) out, 4);
    }
  return outputData;
  }

void decodeblock(uint8_t *in, uint8_t *out )
  {
  out[ 0 ] = (unsigned char ) (in[0] << 2 | in[1] >> 4);
  out[ 1 ] = (unsigned char ) (in[1] << 4 | in[2] >> 2);
  out[ 2 ] = (unsigned char ) (((in[2] << 6) & 0xc0) | in[3]);
  }

int base64decode(const char *inputData, uint8_t *outputData)
  {
  uint8_t in[4];
  uint8_t out[4];
  uint8_t c = 1;
  uint8_t v;
  int i, len;
  int written = 0;

  while( c != 0 )
    {
    for( len = 0, i = 0; (i < 4) && (c != 0); i++ )
      {
      v = 0;
      while( (c != 0) && (v == 0) )
        {
        c = (*inputData) ? *inputData++ : 0;
        v = (uint8_t) ((c < 43 || c > 122) ? 0 : cd64[ c - 43 ]);
        if( v )
          {
          v = (uint8_t) ((v == '$') ? 0 : v - 61);
          }
        }
      if( c != 0 )
        {
        len++;
        if( v )
          {
          in[ i ] = (unsigned char) (v - 1);
          }
        }
      else
        {
        in[i] = 0;
        }
      }
    if( len )
      {
      decodeblock( in, out );
      for( i = 0; i < len - 1; i++ )
        {
        *outputData++ = out[i];
        written++;
        }
      }
    }
  *outputData = 0;
  return written;
  }

std::string base64decode(const std::string inputData)
  {
  uint8_t in[4];
  uint8_t out[4];
  uint8_t c = 1;
  uint8_t v;
  int i, len;
  int written = 0;
  
  std::string outputData;
  outputData.reserve(howmany(inputData.size(), 4) * 3);
  int ipos = 0;

  while( c != 0 )
    {
    for( len = 0, i = 0; (i < 4) && (c != 0); i++ )
      {
      v = 0;
      while( (c != 0) && (v == 0) )
        {
        c = (ipos < inputData.size()) ? inputData[ipos++] : 0;
        v = (uint8_t) ((c < 43 || c > 122) ? 0 : cd64[ c - 43 ]);
        if( v )
          {
          v = (uint8_t) ((v == '$') ? 0 : v - 61);
          }
        }
      if( c != 0 )
        {
        len++;
        if( v )
          {
          in[ i ] = (uint8_t) (v - 1);
          }
        }
      else
        {
        in[i] = 0;
        }
      }
    if( len )
      {
      decodeblock( in, out );
      outputData.append((char*) out, len-1);
      written += len-1;
      }
    }
  return outputData;
  }


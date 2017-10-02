/*
 * Copyright (c) 2007, Cameron Rich
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the axTLS project nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * An implementation of the RC4/ARC4 algorithm.
 * Originally written by Christophe Devine.
 */

#include <string.h>
#include "crypt_rc4.h"

/**
 * Get ready for an encrypt/decrypt operation
 */
void RC4_setup(RC4_CTX1 *ctx1, RC4_CTX2 *ctx2, const uint8_t *key, int length)
  {
  int i, j = 0, k = 0, a;
  uint8_t *m;

  ctx1->x = 0;
  ctx1->y = 0;
  m = ctx2->m;

  for (i = 0; i < 256; i++)
    m[i] = i;

  for (i = 0; i < 256; i++)
    {
    a = m[i];
    j = (unsigned char)(j + a + key[k]);
    m[i] = m[j];
    m[j] = a;

    if (++k >= length)
      k = 0;
    }
  }

/**
 * Perform the encrypt/decrypt operation (can use it for either since
 * this is a stream cipher).
 */
void RC4_crypt(RC4_CTX1 *ctx1, RC4_CTX2 *ctx2, uint8_t *msg, int length)
  {
  int i;
  uint8_t *m, x, y, a, b;

  x = ctx1->x;
  y = ctx1->y;
  m = ctx2->m;

  for (i = 0; i < length; i++)
    {
    a = m[++x];
    y += a;
    m[x] = b = m[y];
    m[y] = a;
    msg[i] ^= m[(uint8_t)(a + b)];
    }

  ctx1->x = x;
  ctx1->y = y;
  }


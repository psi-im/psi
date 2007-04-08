namespace QCA {
/*
Copyright (C) 1999-2004 The Botan Project. All rights reserved.

Redistribution and use in source and binary forms, for any use, with or without
modification, is permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions, and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions, and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

IN NO EVENT SHALL THE AUTHOR(S) OR CONTRIBUTOR(S) BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*************************************************
* Fixed Karatsuba Multiplication Source File     *
* (C) 1999-2004 The Botan Project                *
*************************************************/

}
#include <botan/mp_core.h>
namespace QCA {
}
#include <botan/util.h>
namespace QCA {
}
#include <botan/exceptn.h>
namespace QCA {
}
#include <botan/mem_ops.h>
namespace QCA {

namespace Botan {

/*************************************************
* Karatsuba Multiplication Operation             *
*************************************************/
#define KARATSUBA_CORE(N, INNER_MUL, z, x, y)                     \
   {                                                              \
   const u32bit N2 = N / 2;                                       \
                                                                  \
   const word* x0 = x;                                            \
   const word* x1 = x + N2;                                       \
   const word* y0 = y;                                            \
   const word* y1 = y + N2;                                       \
   word* z0 = z;                                                  \
   word* z1 = z + N;                                              \
                                                                  \
   const s32bit cmp0 = bigint_cmp(x0, N2, x1, N2);                \
   const s32bit cmp1 = bigint_cmp(y1, N2, y0, N2);                \
                                                                  \
   bool positive = (cmp0 == cmp1) || (cmp0 == 0) || (cmp1 == 0);  \
                                                                  \
   word ws[N+N+1] = { 0 };                                        \
   word* middle = ws + N;                                         \
                                                                  \
   if(cmp0 && cmp1)                                               \
      {                                                           \
      if(cmp0 > 0)                                                \
         bigint_sub3(middle, x0, N2, x1, N2);                     \
      else                                                        \
         bigint_sub3(middle, x1, N2, x0, N2);                     \
                                                                  \
      if(cmp1 > 0)                                                \
         bigint_sub3(z, y1, N2, y0, N2);                          \
      else                                                        \
         bigint_sub3(z, y0, N2, y1, N2);                          \
                                                                  \
      INNER_MUL(ws, middle, z);                                   \
      }                                                           \
                                                                  \
   INNER_MUL(z0, x0, y0);                                         \
   INNER_MUL(z1, x1, y1);                                         \
                                                                  \
   bigint_add3(middle, z0, N, z1, N);                             \
                                                                  \
   if(positive)                                                   \
      bigint_add2(middle, N+1, ws, N);                            \
   else                                                           \
      {                                                           \
      const s32bit scmp = bigint_cmp(middle, N+1, ws, N);         \
                                                                  \
      if(scmp < 0)                                                \
         throw Internal_Error("bigint_karat" + to_string(N) +     \
                              ": scmp < 0");                      \
                                                                  \
      if(scmp > 0)                                                \
         bigint_sub2(middle, N+1, ws, N);                         \
      else                                                        \
         clear_mem(middle, N+1);                                  \
      }                                                           \
   bigint_add2(z + N2, 2*N-N2, middle, N+1);                      \
                                                                  \
   clear_mem(ws, 2*N+1);                                          \
   }

/*************************************************
* Karatsuba 16x16 Multiplication                 *
*************************************************/
void bigint_karat16(word z[32], const word x[16], const word y[16])
   {
   KARATSUBA_CORE(16, bigint_comba8, z, x, y);
   }

/*************************************************
* Karatsuba 32x32 Multiplication                 *
*************************************************/
void bigint_karat32(word z[64], const word x[32], const word y[32])
   {
   KARATSUBA_CORE(32, bigint_karat16, z, x, y);
   }

/*************************************************
* Karatsuba 64x64 Multiplication                 *
*************************************************/
void bigint_karat64(word z[128], const word x[64], const word y[64])
   {
   KARATSUBA_CORE(64, bigint_karat32, z, x, y);
   }

/*************************************************
* Karatsuba 128x128 Multiplication               *
*************************************************/
void bigint_karat128(word z[256], const word x[128], const word y[128])
   {
   KARATSUBA_CORE(128, bigint_karat64, z, x, y);
   }

/*************************************************
* Karatsuba 12x12 Multiplication                 *
*************************************************/
void bigint_karat12(word z[24], const word x[12], const word y[12])
   {
   KARATSUBA_CORE(12, bigint_comba6, z, x, y);
   }

/*************************************************
* Karatsuba 24x24 Multiplication                 *
*************************************************/
void bigint_karat24(word z[48], const word x[24], const word y[24])
   {
   KARATSUBA_CORE(24, bigint_karat12, z, x, y);
   }

/*************************************************
* Karatsuba 48x48 Multiplication                 *
*************************************************/
void bigint_karat48(word z[96], const word x[48], const word y[48])
   {
   KARATSUBA_CORE(48, bigint_karat24, z, x, y);
   }

/*************************************************
* Karatsuba 96x96 Multiplication                 *
*************************************************/
void bigint_karat96(word z[192], const word x[96], const word y[96])
   {
   KARATSUBA_CORE(96, bigint_karat48, z, x, y);
   }

}
}

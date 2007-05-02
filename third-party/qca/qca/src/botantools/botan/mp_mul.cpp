/*
Copyright (C) 1999-2007 The Botan Project. All rights reserved.

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
// LICENSEHEADER_END
namespace QCA { // WRAPNS_LINE
/*************************************************
* Karatsuba Multiplication Source File           *
* (C) 1999-2007 The Botan Project                *
*************************************************/

} // WRAPNS_LINE
#include <botan/mp_core.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/mem_ops.h>
namespace QCA { // WRAPNS_LINE

namespace Botan {

namespace {

/*************************************************
* Simple O(N^2) Multiplication                   *
*************************************************/
void bigint_simple_mul(word z[], const word x[], u32bit x_size,
                                 const word y[], u32bit y_size)
   {
   clear_mem(z, x_size + y_size);

   for(u32bit j = 0; j != x_size; ++j)
      z[j+y_size] = bigint_mul_add_words(z + j, y, y_size, x[j]);
   }

/*************************************************
* Karatsuba Multiplication Operation             *
*************************************************/
void karatsuba_mul(word z[], const word x[], const word y[], u32bit N,
                   word workspace[])
   {
   const u32bit KARATSUBA_MUL_LOWER_SIZE = BOTAN_KARAT_MUL_THRESHOLD;

   if(N == 6)
      bigint_comba_mul6(z, x, y);
   else if(N == 8)
      bigint_comba_mul8(z, x, y);
   else if(N < KARATSUBA_MUL_LOWER_SIZE || N % 2)
      bigint_simple_mul(z, x, N, y, N);
   else
      {
      const u32bit N2 = N / 2;

      const word* x0 = x;
      const word* x1 = x + N2;
      const word* y0 = y;
      const word* y1 = y + N2;
      word* z0 = z;
      word* z1 = z + N;

      const s32bit cmp0 = bigint_cmp(x0, N2, x1, N2);
      const s32bit cmp1 = bigint_cmp(y1, N2, y0, N2);

      clear_mem(workspace, 2*N);

      if(cmp0 && cmp1)
         {
         if(cmp0 > 0)
            bigint_sub3(z0, x0, N2, x1, N2);
         else
            bigint_sub3(z0, x1, N2, x0, N2);

         if(cmp1 > 0)
            bigint_sub3(z1, y1, N2, y0, N2);
         else
            bigint_sub3(z1, y0, N2, y1, N2);

         karatsuba_mul(workspace, z0, z1, N2, workspace+N);
         }

      karatsuba_mul(z0, x0, y0, N2, workspace+N);
      karatsuba_mul(z1, x1, y1, N2, workspace+N);

      word carry = bigint_add3_nc(workspace+N, z0, N, z1, N);
      carry += bigint_add2_nc(z + N2, N, workspace + N, N);
      bigint_add2_nc(z + N + N2, N2, &carry, 1);

      if((cmp0 == cmp1) || (cmp0 == 0) || (cmp1 == 0))
         bigint_add2(z + N2, 2*N-N2, workspace, N);
      else
         bigint_sub2(z + N2, 2*N-N2, workspace, N);
      }
   }

/*************************************************
* Pick a good size for the Karatsuba multiply    *
*************************************************/
u32bit karatsuba_size(u32bit z_size,
                      u32bit x_size, u32bit x_sw,
                      u32bit y_size, u32bit y_sw)
   {
   if(x_sw > x_size || x_sw > y_size || y_sw > x_size || y_sw > y_size)
      return 0;

   if(((x_size == x_sw) && (x_size % 2)) ||
      ((y_size == y_sw) && (y_size % 2)))
      return 0;

   u32bit start = (x_sw > y_sw) ? x_sw : y_sw;
   u32bit end = (x_size < y_size) ? x_size : y_size;

   if(start == end)
      {
      if(start % 2)
         return 0;
      return start;
      }

   for(u32bit j = start; j <= end; ++j)
      {
      if(j % 2)
         continue;

      if(2*j > z_size)
         return 0;

      if(x_sw <= j && j <= x_size && y_sw <= j && j <= y_size)
         {
         if(j % 4 == 2 &&
            (j+2) <= x_size && (j+2) <= y_size && 2*(j+2) <= z_size)
            return j+2;
         return j;
         }
      }

   return 0;
   }

/*************************************************
* Handle small operand multiplies                *
*************************************************/
void handle_small_mul(word z[], u32bit z_size,
                      const word x[], u32bit x_size, u32bit x_sw,
                      const word y[], u32bit y_size, u32bit y_sw)
   {
   if(x_sw == 1)        bigint_linmul3(z, y, y_sw, x[0]);
   else if(y_sw == 1)   bigint_linmul3(z, x, x_sw, y[0]);

   else if(x_sw <= 4 && x_size >= 4 &&
           y_sw <= 4 && y_size >= 4 && z_size >= 8)
      bigint_comba_mul4(z, x, y);

   else if(x_sw <= 6 && x_size >= 6 &&
           y_sw <= 6 && y_size >= 6 && z_size >= 12)
      bigint_comba_mul6(z, x, y);

   else if(x_sw <= 8 && x_size >= 8 &&
           y_sw <= 8 && y_size >= 8 && z_size >= 16)
      bigint_comba_mul8(z, x, y);

   else
      bigint_simple_mul(z, x, x_sw, y, y_sw);
   }

}

/*************************************************
* Multiplication Algorithm Dispatcher            *
*************************************************/
void bigint_mul(word z[], u32bit z_size, word workspace[],
                const word x[], u32bit x_size, u32bit x_sw,
                const word y[], u32bit y_size, u32bit y_sw)
   {
   if(x_size <= 8 || y_size <= 8)
      {
      handle_small_mul(z, z_size, x, x_size, x_sw, y, y_size, y_sw);
      return;
      }

   const u32bit N = karatsuba_size(z_size, x_size, x_sw, y_size, y_sw);

   if(N)
      {
      clear_mem(workspace, 2*N);
      karatsuba_mul(z, x, y, N, workspace);
      }
   else
      bigint_simple_mul(z, x, x_sw, y, y_sw);
   }

}
} // WRAPNS_LINE

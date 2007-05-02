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
* Lowest Level MPI Algorithms Source File        *
* (C) 1999-2007 The Botan Project                *
*************************************************/

} // WRAPNS_LINE
#include <botan/mp_asm.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/mp_asmi.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/mp_core.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/mem_ops.h>
namespace QCA { // WRAPNS_LINE

namespace Botan {

extern "C" {

/*************************************************
* Two Operand Addition, No Carry                 *
*************************************************/
word bigint_add2_nc(word x[], u32bit x_size, const word y[], u32bit y_size)
   {
   word carry = 0;

   const u32bit blocks = y_size - (y_size % 8);

   for(u32bit j = 0; j != blocks; j += 8)
      carry = word8_add2(x + j, y + j, carry);

   for(u32bit j = blocks; j != y_size; ++j)
      x[j] = word_add(x[j], y[j], &carry);

   if(!carry)
      return 0;

   for(u32bit j = y_size; j != x_size; ++j)
      if(++x[j])
         return 0;

   return 1;
   }

/*************************************************
* Three Operand Addition, No Carry               *
*************************************************/
word bigint_add3_nc(word z[], const word x[], u32bit x_size,
                              const word y[], u32bit y_size)
   {
   if(x_size < y_size)
      { return bigint_add3_nc(z, y, y_size, x, x_size); }

   word carry = 0;

   const u32bit blocks = y_size - (y_size % 8);

   for(u32bit j = 0; j != blocks; j += 8)
      carry = word8_add3(z + j, x + j, y + j, carry);

   for(u32bit j = blocks; j != y_size; ++j)
      z[j] = word_add(x[j], y[j], &carry);

   for(u32bit j = y_size; j != x_size; ++j)
      {
      word x_j = x[j] + carry;
      if(carry && x_j)
         carry = 0;
      z[j] = x_j;
      }

   return carry;
   }

/*************************************************
* Two Operand Addition                           *
*************************************************/
void bigint_add2(word x[], u32bit x_size, const word y[], u32bit y_size)
   {
   if(bigint_add2_nc(x, x_size, y, y_size))
      ++x[x_size];
   }

/*************************************************
* Three Operand Addition                         *
*************************************************/
void bigint_add3(word z[], const word x[], u32bit x_size,
                           const word y[], u32bit y_size)
   {
   if(bigint_add3_nc(z, x, x_size, y, y_size))
      ++z[(x_size > y_size ? x_size : y_size)];
   }

/*************************************************
* Two Operand Subtraction                        *
*************************************************/
void bigint_sub2(word x[], u32bit x_size, const word y[], u32bit y_size)
   {
   word carry = 0;

   const u32bit blocks = y_size - (y_size % 8);

   for(u32bit j = 0; j != blocks; j += 8)
      carry = word8_sub2(x + j, y + j, carry);

   for(u32bit j = blocks; j != y_size; ++j)
      x[j] = word_sub(x[j], y[j], &carry);

   if(!carry) return;

   for(u32bit j = y_size; j != x_size; ++j)
      {
      --x[j];
      if(x[j] != MP_WORD_MAX) return;
      }
   }

/*************************************************
* Three Operand Subtraction                      *
*************************************************/
void bigint_sub3(word z[], const word x[], u32bit x_size,
                           const word y[], u32bit y_size)
   {
   word carry = 0;

   const u32bit blocks = y_size - (y_size % 8);

   for(u32bit j = 0; j != blocks; j += 8)
      carry = word8_sub3(z + j, x + j, y + j, carry);

   for(u32bit j = blocks; j != y_size; ++j)
      z[j] = word_sub(x[j], y[j], &carry);

   for(u32bit j = y_size; j != x_size; ++j)
      {
      word x_j = x[j] - carry;
      if(carry && x_j != MP_WORD_MAX)
         carry = 0;
      z[j] = x_j;
      }
   }

/*************************************************
* Two Operand Linear Multiply                    *
*************************************************/
void bigint_linmul2(word x[], u32bit x_size, word y)
   {
   const u32bit blocks = x_size - (x_size % 8);

   word carry = 0;

   for(u32bit j = 0; j != blocks; j += 8)
      carry = word8_linmul2(x + j, y, carry);

   for(u32bit j = blocks; j != x_size; ++j)
      x[j] = word_madd2(x[j], y, carry, &carry);

   x[x_size] = carry;
   }

/*************************************************
* Three Operand Linear Multiply                  *
*************************************************/
void bigint_linmul3(word z[], const word x[], u32bit x_size, word y)
   {
   const u32bit blocks = x_size - (x_size % 8);

   word carry = 0;

   for(u32bit j = 0; j != blocks; j += 8)
      carry = word8_linmul3(z + j, x + j, y, carry);

   for(u32bit j = blocks; j != x_size; ++j)
      z[j] = word_madd2(x[j], y, carry, &carry);

   z[x_size] = carry;
   }

/*************************************************
* Montgomery Reduction Algorithm                 *
*************************************************/
#ifndef BOTAN_MINIMAL_BIGINT
void bigint_monty_redc(word z[], u32bit z_size,
                       const word x[], u32bit x_size, word u)
   {
   for(u32bit j = 0; j != x_size; ++j)
      {
      word* z_j = z + j;

      const word y = z_j[0] * u;

      word carry = bigint_mul_add_words(z_j, x, x_size, y);

      word z_sum = z_j[x_size] + carry;
      carry = (z_sum < z_j[x_size]);
      z_j[x_size] = z_sum;

      for(u32bit k = x_size + 1; carry && k != z_size - j; ++k)
         {
         ++z_j[k];
         carry = !z_j[k];
         }
      }

   if(bigint_cmp(z + x_size, x_size + 1, x, x_size) >= 0)
      bigint_sub2(z + x_size, x_size + 1, x, x_size);
   }
#endif

}

}
} // WRAPNS_LINE

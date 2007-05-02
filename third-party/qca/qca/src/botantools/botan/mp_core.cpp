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
* MPI Addition/Subtraction Source File           *
* (C) 1999-2004 The Botan Project                *
*************************************************/

}
#include <botan/mp_core.h>
namespace QCA {

namespace Botan {

/*************************************************
* Two Operand Addition                           *
*************************************************/
void bigint_add2(word x[], u32bit x_size, const word y[], u32bit y_size)
   {
   word carry = 0;

   for(u32bit j = 0; j != y_size; j++)
      {
      word z = x[j] + y[j] + carry;

      const u32bit top_x = x[j] >> (MP_WORD_BITS - 1);
      const u32bit top_y = y[j] >> (MP_WORD_BITS - 1);
      const u32bit top_z = z >> (MP_WORD_BITS - 1);

      x[j] = z;
      carry = ((top_x | top_y) & !top_z) | (top_x & top_y);
      }

   if(!carry) return;

   for(u32bit j = y_size; j != x_size; j++)
      {
      x[j]++;
      if(x[j]) return;
      }
   x[x_size]++;
   }

/*************************************************
* Three Operand Addition                         *
*************************************************/
void bigint_add3(word z[], const word x[], u32bit x_size,
                           const word y[], u32bit y_size)
   {
   if(x_size < y_size)
      { bigint_add3(z, y, y_size, x, x_size); return; }

   word carry = 0;
   for(u32bit j = 0; j != y_size; j++)
      {
      z[j] = x[j] + y[j] + carry;

      const u32bit top_x = x[j] >> (MP_WORD_BITS - 1);
      const u32bit top_y = y[j] >> (MP_WORD_BITS - 1);
      const u32bit top_z = z[j] >> (MP_WORD_BITS - 1);

      carry = ((top_x | top_y) & !top_z) | (top_x & top_y);
      }

   for(u32bit j = y_size; j != x_size; j++)
      z[j] = x[j];

   if(!carry) return;

   for(u32bit j = y_size; j != x_size; j++)
      {
      z[j]++;
      if(z[j]) return;
      }
   z[x_size]++;
   }

/*************************************************
* Two Operand Subtraction                        *
*************************************************/
void bigint_sub2(word x[], u32bit x_size, const word y[], u32bit y_size)
   {
   word borrow = 0;
   for(u32bit j = 0; j != y_size; j++)
      {
      word r = x[j] - y[j];
      word next = ((x[j] < r) ? 1 : 0);
      r -= borrow;
      borrow = next | ((r == MP_WORD_MAX) ? borrow : 0);
      x[j] = r;
      }

   if(!borrow) return;

   for(u32bit j = y_size; j != x_size; j++)
      {
      x[j]--;
      if(x[j] != MP_WORD_MAX) return;
      }
   }

/*************************************************
* Three Operand Subtraction                      *
*************************************************/
void bigint_sub3(word z[], const word x[], u32bit x_size,
                           const word y[], u32bit y_size)
   {
   word borrow = 0;
   for(u32bit j = 0; j != y_size; j++)
      {
      z[j] = x[j] - y[j];
      word next = ((x[j] < z[j]) ? 1 : 0);
      z[j] -= borrow;
      borrow = next | ((z[j] == MP_WORD_MAX) ? borrow : 0);
      }

   for(u32bit j = y_size; j != x_size; j++)
      z[j] = x[j];

   if(!borrow) return;

   for(u32bit j = y_size; j != x_size; j++)
      {
      z[j]--;
      if(z[j] != MP_WORD_MAX) return;
      }
   }

}
}

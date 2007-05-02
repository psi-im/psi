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
* MP Shift Algorithms Source File                *
* (C) 1999-2007 The Botan Project                *
*************************************************/

} // WRAPNS_LINE
#include <botan/mp_core.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/mem_ops.h>
namespace QCA { // WRAPNS_LINE

namespace Botan {

extern "C" {

/*************************************************
* Single Operand Left Shift                      *
*************************************************/
void bigint_shl1(word x[], u32bit x_size, u32bit word_shift, u32bit bit_shift)
   {
   if(word_shift)
      {
      for(u32bit j = 1; j != x_size + 1; ++j)
         x[(x_size - j) + word_shift] = x[x_size - j];
      clear_mem(x, word_shift);
      }

   if(bit_shift)
      {
      word carry = 0;
      for(u32bit j = word_shift; j != x_size + word_shift + 1; ++j)
         {
         word temp = x[j];
         x[j] = (temp << bit_shift) | carry;
         carry = (temp >> (MP_WORD_BITS - bit_shift));
         }
      }
   }

/*************************************************
* Single Operand Right Shift                     *
*************************************************/
void bigint_shr1(word x[], u32bit x_size, u32bit word_shift, u32bit bit_shift)
   {
   if(x_size < word_shift)
      {
      clear_mem(x, x_size);
      return;
      }

   if(word_shift)
      {
      for(u32bit j = 0; j != x_size - word_shift; ++j)
         x[j] = x[j + word_shift];
      for(u32bit j = x_size - word_shift; j != x_size; ++j)
         x[j] = 0;
      }

   if(bit_shift)
      {
      word carry = 0;
      for(u32bit j = x_size - word_shift; j > 0; --j)
         {
         word temp = x[j-1];
         x[j-1] = (temp >> bit_shift) | carry;
         carry = (temp << (MP_WORD_BITS - bit_shift));
         }
      }
   }

/*************************************************
* Two Operand Left Shift                         *
*************************************************/
void bigint_shl2(word y[], const word x[], u32bit x_size,
                 u32bit word_shift, u32bit bit_shift)
   {
   for(u32bit j = 0; j != x_size; ++j)
      y[j + word_shift] = x[j];
   if(bit_shift)
      {
      word carry = 0;
      for(u32bit j = word_shift; j != x_size + word_shift + 1; ++j)
         {
         word temp = y[j];
         y[j] = (temp << bit_shift) | carry;
         carry = (temp >> (MP_WORD_BITS - bit_shift));
         }
      }
   }

/*************************************************
* Two Operand Right Shift                        *
*************************************************/
void bigint_shr2(word y[], const word x[], u32bit x_size,
                 u32bit word_shift, u32bit bit_shift)
   {
   if(x_size < word_shift) return;

   for(u32bit j = 0; j != x_size - word_shift; ++j)
      y[j] = x[j + word_shift];
   if(bit_shift)
      {
      word carry = 0;
      for(u32bit j = x_size - word_shift; j > 0; --j)
         {
         word temp = y[j-1];
         y[j-1] = (temp >> bit_shift) | carry;
         carry = (temp << (MP_WORD_BITS - bit_shift));
         }
      }
   }

}

}
} // WRAPNS_LINE

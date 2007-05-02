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
* MP Misc Functions Source File                  *
* (C) 1999-2007 The Botan Project                *
*************************************************/

} // WRAPNS_LINE
#include <botan/mp_core.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/mp_asm.h>
namespace QCA { // WRAPNS_LINE

namespace Botan {

extern "C" {

/*************************************************
* Core Division Operation                        *
*************************************************/
u32bit bigint_divcore(word q, word y1, word y2,
                      word x1, word x2, word x3)
   {
   word y0 = 0;
   y2 = word_madd2(q, y2, y0, &y0);
   y1 = word_madd2(q, y1, y0, &y0);

   if(y0 > x1) return 1;
   if(y0 < x1) return 0;
   if(y1 > x2)  return 1;
   if(y1 < x2)  return 0;
   if(y2 > x3)  return 1;
   if(y2 < x3)  return 0;
   return 0;
   }

/*************************************************
* Compare two MP integers                        *
*************************************************/
s32bit bigint_cmp(const word x[], u32bit x_size,
                  const word y[], u32bit y_size)
   {
   if(x_size < y_size) { return (-bigint_cmp(y, y_size, x, x_size)); }

   while(x_size > y_size)
      {
      if(x[x_size-1])
         return 1;
      x_size--;
      }
   for(u32bit j = x_size; j > 0; --j)
      {
      if(x[j-1] > y[j-1]) return 1;
      if(x[j-1] < y[j-1]) return -1;
      }
   return 0;
   }

/*************************************************
* Do a 2-word/1-word Division                    *
*************************************************/
word bigint_divop(word n1, word n0, word d)
   {
   word high = n1 % d, quotient = 0;

   for(u32bit j = 0; j != MP_WORD_BITS; ++j)
      {
      word high_top_bit = (high & MP_WORD_TOP_BIT);

      high <<= 1;
      high |= (n0 >> (MP_WORD_BITS-1-j)) & 1;
      quotient <<= 1;

      if(high_top_bit || high >= d)
         {
         high -= d;
         quotient |= 1;
         }
      }

   return quotient;
   }

/*************************************************
* Do a 2-word/1-word Modulo                      *
*************************************************/
word bigint_modop(word n1, word n0, word d)
   {
   word z = bigint_divop(n1, n0, d);
   word dummy = 0;
   z = word_madd2(z, d, dummy, &dummy);
   return (n0-z);
   }

/*************************************************
* Do a word*word->2-word Multiply                *
*************************************************/
void bigint_wordmul(word a, word b, word* out_low, word* out_high)
   {
   const u32bit MP_HWORD_BITS = MP_WORD_BITS / 2;
   const word MP_HWORD_MASK = ((word)1 << MP_HWORD_BITS) - 1;

   const word a_hi = (a >> MP_HWORD_BITS);
   const word a_lo = (a & MP_HWORD_MASK);
   const word b_hi = (b >> MP_HWORD_BITS);
   const word b_lo = (b & MP_HWORD_MASK);

   word x0 = a_hi * b_hi;
   word x1 = a_lo * b_hi;
   word x2 = a_hi * b_lo;
   word x3 = a_lo * b_lo;

   x2 += x3 >> (MP_HWORD_BITS);
   x2 += x1;
   if(x2 < x1)
      x0 += ((word)1 << MP_HWORD_BITS);

   *out_high = x0 + (x2 >> MP_HWORD_BITS);
   *out_low = ((x2 & MP_HWORD_MASK) << MP_HWORD_BITS) + (x3 & MP_HWORD_MASK);
   }

}

}
} // WRAPNS_LINE

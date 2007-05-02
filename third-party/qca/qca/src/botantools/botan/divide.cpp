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
* Division Algorithm Source File                 *
* (C) 1999-2007 The Botan Project                *
*************************************************/

} // WRAPNS_LINE
#include <botan/numthry.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/mp_core.h>
namespace QCA { // WRAPNS_LINE

namespace Botan {

namespace {

/*************************************************
* Handle signed operands, if necessary           *
*************************************************/
void sign_fixup(const BigInt& x, const BigInt& y, BigInt& q, BigInt& r)
   {
   if(x.sign() == BigInt::Negative)
      {
      q.flip_sign();
      if(r.is_nonzero()) { --q; r = y.abs() - r; }
      }
   if(y.sign() == BigInt::Negative)
      q.flip_sign();
   }

}

/*************************************************
* Solve x = q * y + r                            *
*************************************************/
void divide(const BigInt& x, const BigInt& y_arg, BigInt& q, BigInt& r)
   {
   if(y_arg.is_zero())
      throw BigInt::DivideByZero();

   BigInt y = y_arg;
   const u32bit y_words = y.sig_words();
   r = x;

   r.set_sign(BigInt::Positive);
   y.set_sign(BigInt::Positive);

   s32bit compare = r.cmp(y);

   if(compare < 0)
      q = 0;
   else if(compare ==  0)
      {
      q = 1;
      r = 0;
      }
   else
      {
      u32bit shifts = 0;
      word y_top = y[y.sig_words()-1];
      while(y_top < MP_WORD_TOP_BIT) { y_top <<= 1; ++shifts; }
      y <<= shifts;
      r <<= shifts;

      const u32bit n = r.sig_words() - 1, t = y_words - 1;

      q.get_reg().create(n - t + 1);
      if(n <= t)
         {
         while(r > y) { r -= y; q++; }
         r >>= shifts;
         sign_fixup(x, y_arg, q, r);
         return;
         }

      BigInt temp = y << (MP_WORD_BITS * (n-t));

      while(r >= temp) { r -= temp; ++q[n-t]; }

      for(u32bit j = n; j != t; --j)
         {
         const word x_j0  = r.word_at(j);
         const word x_j1 = r.word_at(j-1);
         const word y_t  = y.word_at(t);

         if(x_j0 == y_t)
            q[j-t-1] = MP_WORD_MAX;
         else
            q[j-t-1] = bigint_divop(x_j0, x_j1, y_t);

         while(bigint_divcore(q[j-t-1], y_t, y.word_at(t-1),
                              x_j0, x_j1, r.word_at(j-2)))
            --q[j-t-1];

         r -= (q[j-t-1] * y) << (MP_WORD_BITS * (j-t-1));
         if(r.is_negative())
            {
            r += y << (MP_WORD_BITS * (j-t-1));
            --q[j-t-1];
            }
         }
      r >>= shifts;
      }

   sign_fixup(x, y_arg, q, r);
   }

}
} // WRAPNS_LINE

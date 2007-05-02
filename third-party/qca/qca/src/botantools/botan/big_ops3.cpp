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
* BigInt Binary Operators Source File            *
* (C) 1999-2007 The Botan Project                *
*************************************************/

} // WRAPNS_LINE
#include <botan/bigint.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/numthry.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/mp_core.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/bit_ops.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <algorithm>
namespace QCA { // WRAPNS_LINE

namespace Botan {

/*************************************************
* Addition Operator                              *
*************************************************/
BigInt operator+(const BigInt& x, const BigInt& y)
   {
   const u32bit x_sw = x.sig_words(), y_sw = y.sig_words();

#ifdef BOTAN_TYPES_QT
   BigInt z(x.sign(), qMax(x_sw, y_sw) + 1);
#else
   BigInt z(x.sign(), std::max(x_sw, y_sw) + 1);
#endif

   if((x.sign() == y.sign()))
      bigint_add3(z.get_reg(), x.data(), x_sw, y.data(), y_sw);
   else
      {
      s32bit relative_size = bigint_cmp(x.data(), x_sw, y.data(), y_sw);

      if(relative_size < 0)
         {
         bigint_sub3(z.get_reg(), y.data(), y_sw, x.data(), x_sw);
         z.set_sign(y.sign());
         }
      else if(relative_size == 0)
         z.set_sign(BigInt::Positive);
      else if(relative_size > 0)
         bigint_sub3(z.get_reg(), x.data(), x_sw, y.data(), y_sw);
      }

   return z;
   }

/*************************************************
* Subtraction Operator                           *
*************************************************/
BigInt operator-(const BigInt& x, const BigInt& y)
   {
   const u32bit x_sw = x.sig_words(), y_sw = y.sig_words();

   s32bit relative_size = bigint_cmp(x.data(), x_sw, y.data(), y_sw);

#ifdef BOTAN_TYPES_QT
   BigInt z(BigInt::Positive, qMax(x_sw, y_sw) + 1);
#else
   BigInt z(BigInt::Positive, std::max(x_sw, y_sw) + 1);
#endif

   if(relative_size < 0)
      {
      if(x.sign() == y.sign())
         bigint_sub3(z.get_reg(), y.data(), y_sw, x.data(), x_sw);
      else
         bigint_add3(z.get_reg(), x.data(), x_sw, y.data(), y_sw);
      z.set_sign(y.reverse_sign());
      }
   else if(relative_size == 0)
      {
      if(x.sign() != y.sign())
         bigint_shl2(z.get_reg(), x.data(), x_sw, 0, 1);
      }
   else if(relative_size > 0)
      {
      if(x.sign() == y.sign())
         bigint_sub3(z.get_reg(), x.data(), x_sw, y.data(), y_sw);
      else
         bigint_add3(z.get_reg(), x.data(), x_sw, y.data(), y_sw);
      z.set_sign(x.sign());
      }
   return z;
   }

/*************************************************
* Multiplication Operator                        *
*************************************************/
BigInt operator*(const BigInt& x, const BigInt& y)
   {
   const u32bit x_sw = x.sig_words(), y_sw = y.sig_words();

   BigInt z(BigInt::Positive, x.size() + y.size());

   if(x_sw == 1 && y_sw)
      bigint_linmul3(z.get_reg(), y.data(), y_sw, x.word_at(0));
   else if(y_sw == 1 && x_sw)
      bigint_linmul3(z.get_reg(), x.data(), x_sw, y.word_at(0));
   else if(x_sw && y_sw)
      {
      SecureVector<word> workspace(z.size());
      bigint_mul(z.get_reg(), z.size(), workspace,
                 x.data(), x.size(), x_sw,
                 y.data(), y.size(), y_sw);
      }

   if(x_sw && y_sw && x.sign() != y.sign())
      z.flip_sign();
   return z;
   }

/*************************************************
* Division Operator                              *
*************************************************/
BigInt operator/(const BigInt& x, const BigInt& y)
   {
   BigInt q, r;
   divide(x, y, q, r);
   return q;
   }

/*************************************************
* Modulo Operator                                *
*************************************************/
BigInt operator%(const BigInt& n, const BigInt& mod)
   {
   if(mod.is_zero())
      throw BigInt::DivideByZero();
   if(mod.is_negative())
      throw Invalid_Argument("BigInt::operator%: modulus must be > 0");
   if(n.is_positive() && mod.is_positive() && n < mod)
      return n;

   BigInt q, r;
   divide(n, mod, q, r);
   return r;
   }

/*************************************************
* Modulo Operator                                *
*************************************************/
word operator%(const BigInt& n, word mod)
   {
   if(mod == 0)
      throw BigInt::DivideByZero();
   if(power_of_2(mod))
      return (n.word_at(0) & (mod - 1));

   word remainder = 0;

   for(u32bit j = n.sig_words(); j > 0; --j)
      remainder = bigint_modop(remainder, n.word_at(j-1), mod);

   if(remainder && n.sign() == BigInt::Negative)
      return mod - remainder;
   return remainder;
   }

/*************************************************
* Left Shift Operator                            *
*************************************************/
BigInt operator<<(const BigInt& x, u32bit shift)
   {
   if(shift == 0)
      return x;

   const u32bit shift_words = shift / MP_WORD_BITS,
                shift_bits  = shift % MP_WORD_BITS;

   const u32bit x_sw = x.sig_words();

   BigInt y(x.sign(), x_sw + shift_words + (shift_bits ? 1 : 0));
   bigint_shl2(y.get_reg(), x.data(), x_sw, shift_words, shift_bits);
   return y;
   }

/*************************************************
* Right Shift Operator                           *
*************************************************/
BigInt operator>>(const BigInt& x, u32bit shift)
   {
   if(shift == 0)
      return x;
   if(x.bits() <= shift)
      return 0;

   const u32bit shift_words = shift / MP_WORD_BITS,
                shift_bits  = shift % MP_WORD_BITS,
                x_sw = x.sig_words();

   BigInt y(x.sign(), x_sw - shift_words);
   bigint_shr2(y.get_reg(), x.data(), x_sw, shift_words, shift_bits);
   return y;
   }

}
} // WRAPNS_LINE

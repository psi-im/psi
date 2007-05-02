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
* BigInt Assignment Operators Source File        *
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
#include <botan/util.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <algorithm>
namespace QCA { // WRAPNS_LINE

namespace Botan {

/*************************************************
* Addition Operator                              *
*************************************************/
BigInt& BigInt::operator+=(const BigInt& y)
   {
   const u32bit x_sw = sig_words(), y_sw = y.sig_words();

#ifdef BOTAN_TYPES_QT
   const u32bit reg_size = qMax(x_sw, y_sw) + 1;
#else
   const u32bit reg_size = std::max(x_sw, y_sw) + 1;
#endif
   grow_to(reg_size);

   if((sign() == y.sign()))
      bigint_add2(get_reg(), reg_size - 1, y.data(), y_sw);
   else
      {
      s32bit relative_size = bigint_cmp(data(), x_sw, y.data(), y_sw);

      if(relative_size < 0)
         {
         SecureVector<word> z(reg_size - 1);
         bigint_sub3(z, y.data(), reg_size - 1, data(), x_sw);
         copy_mem(reg.begin(), z.begin(), z.size());
         set_sign(y.sign());
         }
      else if(relative_size == 0)
         {
         reg.clear();
         set_sign(Positive);
         }
      else if(relative_size > 0)
         bigint_sub2(get_reg(), x_sw, y.data(), y_sw);
      }

   return (*this);
   }

/*************************************************
* Subtraction Operator                           *
*************************************************/
BigInt& BigInt::operator-=(const BigInt& y)
   {
   const u32bit x_sw = sig_words(), y_sw = y.sig_words();

   s32bit relative_size = bigint_cmp(data(), x_sw, y.data(), y_sw);

#ifdef BOTAN_TYPES_QT
   const u32bit reg_size = qMax(x_sw, y_sw) + 1;
#else
   const u32bit reg_size = std::max(x_sw, y_sw) + 1;
#endif
   grow_to(reg_size);

   if(relative_size < 0)
      {
      if(sign() == y.sign())
         {
         SecureVector<word> z(reg_size - 1);
         bigint_sub3(z, y.data(), reg_size - 1, data(), x_sw);
         copy_mem(reg.begin(), z.begin(), z.size());
         }
      else
         bigint_add2(get_reg(), reg_size - 1, y.data(), y_sw);

      set_sign(y.reverse_sign());
      }
   else if(relative_size == 0)
      {
      if(sign() == y.sign())
         {
         reg.clear();
         set_sign(Positive);
         }
      else
         bigint_shl1(get_reg(), x_sw, 0, 1);
      }
   else if(relative_size > 0)
      {
      if(sign() == y.sign())
         bigint_sub2(get_reg(), x_sw, y.data(), y_sw);
      else
         bigint_add2(get_reg(), reg_size - 1, y.data(), y_sw);
      }

   return (*this);
   }

/*************************************************
* Multiplication Operator                        *
*************************************************/
BigInt& BigInt::operator*=(const BigInt& y)
   {
   const u32bit x_sw = sig_words(), y_sw = y.sig_words();
   set_sign((sign() == y.sign()) ? Positive : Negative);

   if(x_sw == 0 || y_sw == 0)
      {
      reg.clear();
      set_sign(Positive);
      }
   else if(x_sw == 1 && y_sw)
      {
      grow_to(y_sw + 2);
      bigint_linmul3(get_reg(), y.data(), y_sw, word_at(0));
      }
   else if(y_sw == 1 && x_sw)
      {
      grow_to(x_sw + 2);
      bigint_linmul2(get_reg(), x_sw, y.word_at(0));
      }
   else
      {
      grow_to(size() + y.size());

      SecureVector<word> z(data(), x_sw);
      SecureVector<word> workspace(size());

      bigint_mul(get_reg(), size(), workspace,
                 z, z.size(), x_sw,
                 y.data(), y.size(), y_sw);
      }

   return (*this);
   }

/*************************************************
* Division Operator                              *
*************************************************/
BigInt& BigInt::operator/=(const BigInt& y)
   {
   if(y.sig_words() == 1 && power_of_2(y.word_at(0)))
      (*this) >>= (y.bits() - 1);
   else
      (*this) = (*this) / y;
   return (*this);
   }

/*************************************************
* Modulo Operator                                *
*************************************************/
BigInt& BigInt::operator%=(const BigInt& mod)
   {
   return (*this = (*this) % mod);
   }

/*************************************************
* Modulo Operator                                *
*************************************************/
word BigInt::operator%=(word mod)
   {
   if(mod == 0)
      throw BigInt::DivideByZero();
   if(power_of_2(mod))
       {
       word result = (word_at(0) & (mod - 1));
       clear();
       grow_to(2);
       reg[0] = result;
       return result;
       }

   word remainder = 0;

   for(u32bit j = sig_words(); j > 0; --j)
      remainder = bigint_modop(remainder, word_at(j-1), mod);
   clear();
   grow_to(2);

   if(remainder && sign() == BigInt::Negative)
      reg[0] = mod - remainder;
   else
      reg[0] = remainder;

   set_sign(BigInt::Positive);

   return word_at(0);
   }

/*************************************************
* Left Shift Operator                            *
*************************************************/
BigInt& BigInt::operator<<=(u32bit shift)
   {
   if(shift)
      {
      const u32bit shift_words = shift / MP_WORD_BITS,
                   shift_bits  = shift % MP_WORD_BITS,
                   words = sig_words();

      grow_to(words + shift_words + (shift_bits ? 1 : 0));
      bigint_shl1(get_reg(), words, shift_words, shift_bits);
      }

   return (*this);
   }

/*************************************************
* Right Shift Operator                           *
*************************************************/
BigInt& BigInt::operator>>=(u32bit shift)
   {
   if(shift)
      {
      const u32bit shift_words = shift / MP_WORD_BITS,
                   shift_bits  = shift % MP_WORD_BITS;

      bigint_shr1(get_reg(), sig_words(), shift_words, shift_bits);

      if(is_zero())
         set_sign(Positive);
      }

   return (*this);
   }

}
} // WRAPNS_LINE

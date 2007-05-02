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
* BigInt Assignment Operators Source File        *
* (C) 1999-2004 The Botan Project                *
*************************************************/

}
#include <botan/bigint.h>
namespace QCA {
}
#include <botan/numthry.h>
namespace QCA {
}
#include <botan/mp_core.h>
namespace QCA {

namespace Botan {

/*************************************************
* Addition Operator                              *
*************************************************/
BigInt& BigInt::operator+=(const BigInt& n)
   {
   if((sign() == n.sign()))
      {
      const u32bit reg_size = qMax(sig_words(), n.sig_words()) + 1;
      grow_to(reg_size);
      bigint_add2(get_reg(), reg_size-1, n.data(), n.sig_words());
      }
   else
      (*this) = (*this) + n;
   return (*this);
   }

/*************************************************
* Subtraction Operator                           *
*************************************************/
BigInt& BigInt::operator-=(const BigInt& n)
   {
   s32bit relative_size = bigint_cmp(data(), sig_words(),
                                     n.data(), n.sig_words());

   if(relative_size == 0)
      {
      if(sign() == n.sign())
         (*this) = 0;
      else
         (*this) <<= 1;
      return (*this);
      }

   const u32bit reg_size = qMax(sig_words(), n.sig_words()) + 1;
   grow_to(reg_size);

   if(relative_size == -1)
      {
      if(sign() == n.sign())
         (*this) = (*this) - n;
      else
         bigint_add2(get_reg(), reg_size-1, n.data(), n.sig_words());
      set_sign(n.reverse_sign());
      }
   if(relative_size == 1)
      {
      if(sign() == n.sign())
         bigint_sub2(get_reg(), sig_words(), n.data(), n.sig_words());
      else
         bigint_add2(get_reg(), reg_size-1, n.data(), n.sig_words());
      }
   return (*this);
   }

/*************************************************
* Multiplication Operator                        *
*************************************************/
BigInt& BigInt::operator*=(const BigInt& n)
   {
   if(is_zero()) return (*this);
   if(n.is_zero()) { (*this) = 0; return (*this); }

   if(sign() != n.sign())
      set_sign(Negative);
   else
      set_sign(Positive);

   const u32bit words = sig_words();
   const u32bit n_words = n.sig_words();

   if(words == 1 || n_words == 1)
      {
      grow_to(words + n_words);
      if(n_words == 1)
         bigint_linmul2(get_reg(), words, n.word_at(0));
      else
         bigint_linmul3(get_reg(), n.data(), n_words, word_at(0));
      return (*this);
      }

   BigInt z(sign(), size() + n.size());
   bigint_mul3(z.get_reg(), z.size(),
               data(),   size(),   words,
               n.data(), n.size(), n_words);
   (*this) = z;
   return (*this);
   }

/*************************************************
* Division Operator                              *
*************************************************/
BigInt& BigInt::operator/=(const BigInt& n)
   {
   if(n.sig_words() == 1 && power_of_2(n.word_at(0)))
      (*this) >>= (n.bits() - 1);
   else
      (*this) = (*this) / n;
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
      reg.grow_to(2);
      reg[0] = result;
      return result;
      }

   word remainder = 0;
   u32bit size = sig_words();

   for(u32bit j = size; j > 0; j--)
      remainder = bigint_modop(remainder, word_at(j-1), mod);
   clear();
   reg.grow_to(2);
   reg[0] = remainder;
   return word_at(0);
   }

/*************************************************
* Left Shift Operator                            *
*************************************************/
BigInt& BigInt::operator<<=(u32bit shift)
   {
   if(shift == 0) return (*this);
   const u32bit shift_words = shift / MP_WORD_BITS,
                shift_bits  = shift % MP_WORD_BITS;

   grow_to(sig_words() + shift_words + (shift_bits ? 1 : 0));
   bigint_shl1(get_reg(), sig_words(), shift_words, shift_bits);
   return (*this);
   }

/*************************************************
* Right Shift Operator                           *
*************************************************/
BigInt& BigInt::operator>>=(u32bit shift)
   {
   if(shift == 0) return (*this);

   if(bits() <= shift)
      {
      (*this) = 0;
      return (*this);
      }

   const u32bit shift_words = shift / MP_WORD_BITS,
                shift_bits  = shift % MP_WORD_BITS;
   bigint_shr1(get_reg(), sig_words(), shift_words, shift_bits);
   return (*this);
   }

}
}

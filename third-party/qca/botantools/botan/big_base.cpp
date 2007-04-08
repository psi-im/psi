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
* BigInt Base Source File                        *
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
}
#include <botan/util.h>
namespace QCA {
#ifndef BOTAN_MINIMAL_BIGINT
}
# include <botan/rng.h>
namespace QCA {
#endif

namespace Botan {

/*************************************************
* Construct a BigInt from a regular number       *
*************************************************/
BigInt::BigInt(u64bit n)
   {
   set_sign(Positive);

   if(n == 0)
      return;

   const u32bit limbs_needed = sizeof(u64bit) / sizeof(word);

   reg.create(2*limbs_needed + 2);
   for(u32bit j = 0; j != limbs_needed; j++)
      reg[j] = (word)((n >> (j*MP_WORD_BITS)) & MP_WORD_MASK);
   }

/*************************************************
* Construct a BigInt of the specified size       *
*************************************************/
BigInt::BigInt(Sign s, u32bit size)
   {
   reg.create(size);
   signedness = s;
   }

/*************************************************
* Construct a BigInt from a "raw" BigInt         *
*************************************************/
BigInt::BigInt(const BigInt& b)
   {
   if(b.sig_words())
      {
      reg.set(b.data(), b.sig_words());
      set_sign(b.sign());
      }
   else
      {
      reg.create(2);
      set_sign(Positive);
      }
   }

/*************************************************
* Construct a BigInt from a string               *
*************************************************/
BigInt::BigInt(const std::string& str)
   {
   Base base = Decimal;
   u32bit markers = 0;
   bool negative = false;
   if(str.length() > 0 && str[0] == '-') { markers += 1; negative = true; }

   if(str.length() > markers + 2 && str[markers    ] == '0' &&
                                    str[markers + 1] == 'x')
      { markers += 2; base = Hexadecimal; }
   else if(str.length() > markers + 1 && str[markers] == '0')
      { markers += 1; base = Octal; }

   *this = decode((const byte*)str.data() + markers,
                  str.length() - markers, base);

   if(negative) set_sign(Negative);
   else         set_sign(Positive);
   }

/*************************************************
* Construct a BigInt from an encoded BigInt      *
*************************************************/
BigInt::BigInt(const byte input[], u32bit length, Base base)
   {
   set_sign(Positive);
   *this = decode(input, length, base);
   }

/*************************************************
* Construct a Random BigInt                      *
*************************************************/
#ifndef BOTAN_MINIMAL_BIGINT

BigInt::BigInt(NumberType type, u32bit bits)
   {
   set_sign(Positive);
   if(type == Random && bits)
      randomize(bits);
   else if(type == Power2)
      set_bit(bits);
   }

#endif // BOTAN_MINIMAL_BIGINT

/*************************************************
* Swap this BigInt with another                  *
*************************************************/
void BigInt::swap(BigInt& other)
   {
   std::swap(reg, other.reg);
   std::swap(signedness, other.signedness);
   }

/*************************************************
* Comparison Function                            *
*************************************************/
s32bit BigInt::cmp(const BigInt& n, bool check_signs) const
   {
   if(check_signs)
      {
      if(n.is_positive() && this->is_negative()) return -1;
      if(n.is_negative() && this->is_positive()) return 1;
      if(n.is_negative() && this->is_negative())
         return (-bigint_cmp(data(), sig_words(), n.data(), n.sig_words()));
      }
   return bigint_cmp(data(), sig_words(), n.data(), n.sig_words());
   }

/*************************************************
* Add n to this number                           *
*************************************************/
void BigInt::add(word n)
   {
   if(!n) return;
   word temp = reg[0];
   reg[0] += n;
   if(reg[0] > temp)
      return;
   for(u32bit j = 1; j != size(); j++)
      if(++reg[j]) return;
   grow_to(2*size());
   reg[size() / 2] = 1;
   }

/*************************************************
* Subtract n from this number                    *
*************************************************/
void BigInt::sub(word n)
   {
   if(!n) return;
   word temp = reg[0];
   reg[0] -= n;
   if(reg[0] < temp)
      return;
   for(u32bit j = 1; j != size(); j++)
      if(reg[j]--) return;
   reg.create(2);
   flip_sign();
   reg[0] = n - temp;
   }

/*************************************************
* Prefix Increment Operator                      *
*************************************************/
BigInt& BigInt::operator++()
   {
   if(is_negative()) sub(1);
   else              add(1);
   return (*this);
   }

/*************************************************
* Prefix Decrement Operator                      *
*************************************************/
BigInt& BigInt::operator--()
   {
   if(is_negative()) add(1);
   else              sub(1);
   return (*this);
   }

/*************************************************
* Return word n of this number                   *
*************************************************/
word BigInt::word_at(u32bit n) const
   {
   if(n >= size()) return 0;
   else            return reg[n];
   }

/*************************************************
* Convert this number to a u32bit, if possible   *
*************************************************/
u32bit BigInt::to_u32bit() const
   {
   if(is_negative())
      throw Encoding_Error("BigInt::to_u32bit: Number is negative");
   if(bits() >= 32)
      throw Encoding_Error("BigInt::to_u32bit: Number is too big to convert");

   u32bit out = 0;
   for(u32bit j = 0; j != 4; j++)
      out = (out << 8) | byte_at(3-j);
   return out;
   }

/*************************************************
* Return byte n of this number                   *
*************************************************/
byte BigInt::byte_at(u32bit n) const
   {
   const u32bit WORD_BYTES = sizeof(word);
   u32bit word_num = n / WORD_BYTES, byte_num = n % WORD_BYTES;
   if(word_num >= size())
      return 0;
   else
      return get_byte(WORD_BYTES - byte_num - 1, reg[word_num]);
   }

/*************************************************
* Return bit n of this number                    *
*************************************************/
bool BigInt::get_bit(u32bit n) const
   {
   return ((word_at(n / MP_WORD_BITS) >> (n % MP_WORD_BITS)) & 1);
   }

/*************************************************
* Return nibble n of this number                 *
*************************************************/
u32bit BigInt::get_nibble(u32bit n, u32bit nibble_size) const
   {
   if(nibble_size > 32)
      throw Invalid_Argument("BigInt::get_nibble: Nibble size too large");

   u32bit nibble = 0;
   for(s32bit j = (s32bit)nibble_size-1; j >= 0; j--)
      {
      nibble <<= 1;
      if(get_bit(n * nibble_size + j))
         nibble |= 1;
      }
   return nibble;
   }

/*************************************************
* Set bit number n                               *
*************************************************/
void BigInt::set_bit(u32bit n)
   {
   const u32bit which = n / MP_WORD_BITS;
   const word mask = (word)1 << (n % MP_WORD_BITS);
   if(which >= size()) grow_to(which + 1);
   reg[which] |= mask;
   }

/*************************************************
* Clear bit number n                             *
*************************************************/
void BigInt::clear_bit(u32bit n)
   {
   const u32bit which = n / MP_WORD_BITS;
   const word mask = (word)1 << (n % MP_WORD_BITS);
   if(which < size())
      reg[which] &= ~mask;
   }

/*************************************************
* Clear all but the lowest n bits                *
*************************************************/
void BigInt::mask_bits(u32bit n)
   {
   if(n == 0) { clear(); return; }
   if(n >= bits()) return;

   const u32bit top_word = n / MP_WORD_BITS;
   const word mask = ((word)1 << (n % MP_WORD_BITS)) - 1;

   if(top_word < size())
      for(u32bit j = top_word + 1; j != size(); j++)
         reg[j] = 0;

   reg[top_word] &= mask;
   }

/*************************************************
* Count the significant words                    *
*************************************************/
u32bit BigInt::sig_words() const
   {
   const word* x = data();
   u32bit top_set = size();

   while(top_set >= 4)
      {
      word sum = x[top_set-1] | x[top_set-2] | x[top_set-3] | x[top_set-4];
      if(sum) break;
      else    top_set -= 4;
      }
   while(top_set && (x[top_set-1] == 0))
      top_set--;
   return top_set;
   }

/*************************************************
* Count how many bytes are being used            *
*************************************************/
u32bit BigInt::bytes() const
   {
   return (bits() + 7) / 8;
   }

/*************************************************
* Count how many bits are being used             *
*************************************************/
u32bit BigInt::bits() const
   {
   if(sig_words() == 0) return 0;
   u32bit full_words = sig_words() - 1, top_bits = MP_WORD_BITS;
   word top_word = word_at(full_words), mask = MP_WORD_TOP_BIT;
   while(top_bits && ((top_word & mask) == 0))
      { mask >>= 1; top_bits--; }
   return (full_words * MP_WORD_BITS + top_bits);
   }

/*************************************************
* Calcluate the size in a certain base           *
*************************************************/
u32bit BigInt::encoded_size(Base base) const
   {
   static const double LOG_2_BASE_10 = 0.30102999566;
   if(base == Binary)
      return bytes();
   else if(base == Hexadecimal)
      return 2*bytes();
   else if(base == Octal)
      return ((bits() + 2) / 3);
   else if(base == Decimal)
      return (u32bit)((bits() * LOG_2_BASE_10) + 1);
   else
      throw Invalid_Argument("Unknown base for BigInt encoding");
   }

/*************************************************
* Return true if this number is zero             *
*************************************************/
bool BigInt::is_zero() const
   {
   for(u32bit j = 0; j != size(); j++)
      if(reg[j]) return false;
   return true;
   }

/*************************************************
* Set the sign                                   *
*************************************************/
void BigInt::set_sign(Sign s)
   {
   if(is_zero())
      signedness = Positive;
   else
      signedness = s;
   }

/*************************************************
* Reverse the value of the sign flag             *
*************************************************/
void BigInt::flip_sign()
   {
   set_sign(reverse_sign());
   }

/*************************************************
* Return the opposite value of the current sign  *
*************************************************/
BigInt::Sign BigInt::reverse_sign() const
   {
   if(sign() == Positive)
      return Negative;
   return Positive;
   }

/*************************************************
* Return the negation of this number             *
*************************************************/
BigInt BigInt::operator-() const
   {
   BigInt x = (*this);
   x.flip_sign();
   return x;
   }

/*************************************************
* Return the absolute value of this number       *
*************************************************/
BigInt BigInt::abs() const
   {
   BigInt x = (*this);
   x.set_sign(Positive);
   return x;
   }

/*************************************************
* Randomize this number                          *
*************************************************/
#ifndef BOTAN_MINIMAL_BIGINT

void BigInt::randomize(u32bit bitsize, RNG_Quality level)
   {
   set_sign(Positive);

   if(bitsize == 0)
      clear();
   else
      {
      SecureVector<byte> array((bitsize + 7) / 8);
      Global_RNG::randomize(array, array.size(), level);
      if(bitsize % 8)
         array[0] &= 0xFF >> (8 - (bitsize % 8));
      array[0] |= 0x80 >> ((bitsize % 8) ? (8 - bitsize % 8) : 0);
      binary_decode(array, array.size());
      }
   }

#endif // BOTAN_MINIMAL_BIGINT

/*************************************************
* Encode this number into bytes                  *
*************************************************/
void BigInt::binary_encode(byte output[]) const
   {
   const u32bit sig_bytes = bytes();
   for(u32bit j = 0; j != sig_bytes; j++)
      output[sig_bytes-j-1] = byte_at(j);
   }

/*************************************************
* Set this number to the value in buf            *
*************************************************/
void BigInt::binary_decode(const byte buf[], u32bit length)
   {
   const u32bit WORD_BYTES = sizeof(word);
   reg.create(length / WORD_BYTES + 1);

   for(u32bit j = 0; j != length / WORD_BYTES; j++)
      {
      u32bit top = length - WORD_BYTES*j;
      for(u32bit k = WORD_BYTES; k > 0; k--)
         reg[j] = (reg[j] << 8) | buf[top - k];
      }
   for(u32bit j = 0; j != length % WORD_BYTES; j++)
      reg[length / WORD_BYTES] = (reg[length / WORD_BYTES] << 8) | buf[j];
   }

#ifndef BOTAN_MINIMAL_BIGINT

/*************************************************
* Generate a random integer                      *
*************************************************/
BigInt random_integer(u32bit bits, RNG_Quality level)
   {
   BigInt x;
   x.randomize(bits, level);
   return x;
   }

/*************************************************
* Generate a random integer within given range   *
*************************************************/
BigInt random_integer(const BigInt& min, const BigInt& max, RNG_Quality level)
   {
   BigInt range = max - min;

   if(range <= 0)
      throw Invalid_Argument("random_integer: invalid min/max values");

   return (min + (random_integer(range.bits() + 2, level) % range));
   }

/*************************************************
* Generate a random safe prime                   *
*************************************************/
BigInt random_safe_prime(u32bit bits, RNG_Quality level)
   {
   if(bits <= 64)
      throw Invalid_Argument("random_safe_prime: Can't make a prime of " +
                             to_string(bits) + " bits");

   BigInt p;
   do
      p = (random_prime(bits - 1, level) << 1) + 1;
   while(!is_prime(p));
   return p;
   }

#endif // BOTAN_MINIMAL_BIGINT

}
}

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
* BigInt Header File                             *
* (C) 1999-2007 The Botan Project                *
*************************************************/

#ifndef BOTAN_BIGINT_H__
#define BOTAN_BIGINT_H__

#ifdef BOTAN_MINIMAL_BIGINT
} // WRAPNS_LINE
# include <botan/secmem.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
# include <botan/exceptn.h>
namespace QCA { // WRAPNS_LINE
#else
} // WRAPNS_LINE
# include <botan/base.h>
namespace QCA { // WRAPNS_LINE
#endif

} // WRAPNS_LINE
#include <botan/mp_types.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <iosfwd>
namespace QCA { // WRAPNS_LINE

namespace Botan {

/*************************************************
* BigInt                                         *
*************************************************/
class BigInt
   {
   public:
      enum Base { Octal = 8, Decimal = 10, Hexadecimal = 16, Binary = 256 };
      enum Sign { Negative = 0, Positive = 1 };
      enum NumberType { Random, Power2 };

      struct DivideByZero : public Exception
         { DivideByZero() : Exception("BigInt divide by zero") {} };

      BigInt& operator+=(const BigInt&);
      BigInt& operator-=(const BigInt&);

      BigInt& operator*=(const BigInt&);
      BigInt& operator/=(const BigInt&);
      BigInt& operator%=(const BigInt&);
      word    operator%=(word);
      BigInt& operator<<=(u32bit);
      BigInt& operator>>=(u32bit);

      BigInt& operator++() { return (*this += 1); }
      BigInt& operator--() { return (*this -= 1); }
      BigInt  operator++(int) { BigInt x = (*this); ++(*this); return x; }
      BigInt  operator--(int) { BigInt x = (*this); --(*this); return x; }

      BigInt operator-() const;
      bool operator !() const { return (!is_nonzero()); }

      s32bit cmp(const BigInt&, bool = true) const;
      bool is_even() const { return (get_bit(0) == 0); }
      bool is_odd()  const { return (get_bit(0) == 1); }
      bool is_nonzero() const { return (!is_zero()); }
      bool is_zero() const;

      void set_bit(u32bit);
      void clear_bit(u32bit);
      void mask_bits(u32bit);

      bool get_bit(u32bit) const;
      u32bit get_substring(u32bit, u32bit) const;
      byte byte_at(u32bit) const;
      word word_at(u32bit n) const
         { return ((n < size()) ? reg[n] : 0); }

      u32bit to_u32bit() const;

      bool is_negative() const { return (sign() == Negative); }
      bool is_positive() const { return (sign() == Positive); }
      Sign sign() const { return (signedness); }
      Sign reverse_sign() const;
      void flip_sign();
      void set_sign(Sign);
      BigInt abs() const;

      u32bit size() const { return reg.size(); }
      u32bit sig_words() const;
      u32bit bytes() const;
      u32bit bits() const;

      const word* data() const { return reg.begin(); }
      SecureVector<word>& get_reg() { return reg; }
      void grow_reg(u32bit) const;

      word& operator[](u32bit index) { return reg[index]; }
      word operator[](u32bit index) const { return reg[index]; }
      void clear() { reg.clear(); }

#ifndef BOTAN_MINIMAL_BIGINT
      void randomize(u32bit = 0);
#endif

      void binary_encode(byte[]) const;
      void binary_decode(const byte[], u32bit);
      u32bit encoded_size(Base = Binary) const;

      static SecureVector<byte> encode(const BigInt&, Base = Binary);
      static void encode(byte[], const BigInt&, Base = Binary);
      static BigInt decode(const byte[], u32bit, Base = Binary);
      static BigInt decode(const MemoryRegion<byte>&, Base = Binary);
      static SecureVector<byte> encode_1363(const BigInt&, u32bit);

      void swap(BigInt&);

      BigInt() { signedness = Positive; }
      BigInt(u64bit);
      BigInt(const BigInt&);
      BigInt(const std::string&);
      BigInt(const byte[], u32bit, Base = Binary);
      BigInt(Sign, u32bit);
#ifndef BOTAN_MINIMAL_BIGINT
      BigInt(NumberType, u32bit);
#endif
   private:
      void grow_to(u32bit) const;
      SecureVector<word> reg;
      Sign signedness;
   };

/*************************************************
* Arithmetic Operators                           *
*************************************************/
BigInt operator+(const BigInt&, const BigInt&);
BigInt operator-(const BigInt&, const BigInt&);
BigInt operator*(const BigInt&, const BigInt&);
BigInt operator/(const BigInt&, const BigInt&);
BigInt operator%(const BigInt&, const BigInt&);
word   operator%(const BigInt&, word);
BigInt operator<<(const BigInt&, u32bit);
BigInt operator>>(const BigInt&, u32bit);

/*************************************************
* Comparison Operators                           *
*************************************************/
inline bool operator==(const BigInt& a, const BigInt& b)
   { return (a.cmp(b) == 0); }
inline bool operator!=(const BigInt& a, const BigInt& b)
   { return (a.cmp(b) != 0); }
inline bool operator<=(const BigInt& a, const BigInt& b)
   { return (a.cmp(b) <= 0); }
inline bool operator>=(const BigInt& a, const BigInt& b)
   { return (a.cmp(b) >= 0); }
inline bool operator<(const BigInt& a, const BigInt& b)
   { return (a.cmp(b) < 0); }
inline bool operator>(const BigInt& a, const BigInt& b)
   { return (a.cmp(b) > 0); }

/*************************************************
* I/O Operators                                  *
*************************************************/
#ifndef BOTAN_MINIMAL_BIGINT
std::ostream& operator<<(std::ostream&, const BigInt&);
std::istream& operator>>(std::istream&, BigInt&);
#endif

}

#ifndef BOTAN_MINIMAL_BIGINT
} // WRAPNS_LINE
namespace std {

inline void swap(Botan::BigInt& a, Botan::BigInt& b) { a.swap(b); }

}
namespace QCA { // WRAPNS_LINE
#endif

#endif
} // WRAPNS_LINE

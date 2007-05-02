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
* Number Theory Source File                      *
* (C) 1999-2004 The Botan Project                *
*************************************************/

}
#include <botan/numthry.h>
namespace QCA {
#ifndef BOTAN_MINIMAL_BIGINT
}
# include <botan/ui.h>
namespace QCA {
#endif

namespace Botan {

namespace {

/*************************************************
* Miller-Rabin Iterations                        *
*************************************************/
u32bit miller_rabin_test_iterations(u32bit bits, bool verify)
   {
   struct mapping { u32bit bits; u32bit verify_iter; u32bit check_iter; };

   static const mapping tests[] = {
      {   50, 55, 25 },
      {  100, 38, 22 },
      {  160, 32, 18 },
      {  163, 31, 17 },
      {  168, 30, 16 },
      {  177, 29, 16 },
      {  181, 28, 15 },
      {  185, 27, 15 },
      {  190, 26, 15 },
      {  195, 25, 14 },
      {  201, 24, 14 },
      {  208, 23, 14 },
      {  215, 22, 13 },
      {  222, 21, 13 },
      {  231, 20, 13 },
      {  241, 19, 12 },
      {  252, 18, 12 },
      {  264, 17, 12 },
      {  278, 16, 11 },
      {  294, 15, 10 },
      {  313, 14,  9 },
      {  334, 13,  8 },
      {  360, 12,  8 },
      {  392, 11,  7 },
      {  430, 10,  7 },
      {  479,  9,  6 },
      {  542,  8,  6 },
      {  626,  7,  5 },
      {  746,  6,  4 },
      {  926,  5,  3 },
      { 1232,  4,  2 },
      { 1853,  3,  2 },
      {    0,  0,  0 }
   };

   for(u32bit j = 0; tests[j].bits; j++)
      {
      if(bits <= tests[j].bits)
         if(verify)
            return tests[j].verify_iter;
         else
            return tests[j].check_iter;
      }
   return 2;
   }

}

/*************************************************
* Return the number of 0 bits at the end of n    *
*************************************************/
u32bit low_zero_bits(const BigInt& n)
   {
   if(n.is_zero()) return 0;

   u32bit bits = 0, max_bits = n.bits();
   while((n.get_bit(bits) == 0) && bits < max_bits) bits++;
   return bits;
   }

/*************************************************
* If this number is of the form 2^n, return n    *
*************************************************/
u32bit power_of_2(const BigInt& n)
   {
   if(n <= 1 || n % 2 == 1) return 0;
   if(n == 2) return 1;

   u32bit bit_set = 0;

   const u32bit n_bits = n.bits();
   for(u32bit j = 1; j != n_bits; j++)
      if(n.get_bit(j))
         {
         if(bit_set) return 0;
         bit_set = j;
         }

   return bit_set;
   }

/*************************************************
* Calculate the GCD                              *
*************************************************/
BigInt gcd(const BigInt& a, const BigInt& b)
   {
   if(a.is_zero() || b.is_zero()) return 0;
   if(a == 1 || b == 1)           return 1;

   BigInt x = a, y = b;
   x.set_sign(BigInt::Positive);
   y.set_sign(BigInt::Positive);
   u32bit shift = qMin(low_zero_bits(x), low_zero_bits(y));

   x >>= shift;
   y >>= shift;

   while(x.is_nonzero())
      {
      x >>= low_zero_bits(x);
      y >>= low_zero_bits(y);
      if(x >= y) { x -= y; x >>= 1; }
      else       { y -= x; y >>= 1; }
      }

   return (y << shift);
   }

/*************************************************
* Calculate the LCM                              *
*************************************************/
BigInt lcm(const BigInt& a, const BigInt& b)
   {
   return ((a * b) / gcd(a, b));
   }

/*************************************************
* Square a BigInt                                *
*************************************************/
BigInt square(const BigInt& a)
   {
   return (a * a);
   }

/*************************************************
* Find the Modular Inverse                       *
*************************************************/
BigInt inverse_mod(const BigInt& n, const BigInt& mod)
   {
   if(mod.is_zero())
      throw BigInt::DivideByZero();
   if(mod.is_negative() || n.is_negative())
      throw Invalid_Argument("inverse_mod: arguments must be non-negative");

   if(n.is_zero() || (n.is_even() && mod.is_even()))
      return 0;

   BigInt x = mod, y = n, u = mod, v = n;
   BigInt A = 1, B = 0, C = 0, D = 1;

   while(u.is_nonzero())
      {
      u32bit zero_bits = low_zero_bits(u);
      u >>= zero_bits;
      for(u32bit j = 0; j != zero_bits; j++)
         {
         if(A.is_odd() || B.is_odd())
            { A += y; B -= x; }
         A >>= 1; B >>= 1;
         }

      zero_bits = low_zero_bits(v);
      v >>= zero_bits;
      for(u32bit j = 0; j != zero_bits; j++)
         {
         if(C.is_odd() || D.is_odd())
            { C += y; D -= x; }
         C >>= 1; D >>= 1;
         }

      if(u >= v) { u -= v; A -= C; B -= D; }
      else       { v -= u; C -= A; D -= B; }
      }

   if(v != 1)
      return 0;

   while(D.is_negative()) D += mod;
   while(D >= mod) D -= mod;

   return D;
   }

/*************************************************
* Calculate the Jacobi symbol                    *
*************************************************/
s32bit jacobi(const BigInt& a, const BigInt& n)
   {
   if(a.is_negative())
      throw Invalid_Argument("jacobi: first argument must be non-negative");
   if(n.is_even() || n < 2)
      throw Invalid_Argument("jacobi: second argument must be odd and > 1");

   BigInt x = a, y = n;
   s32bit J = 1;

   while(y > 1)
      {
      x %= y;
      if(x > y / 2)
         {
         x = y - x;
         if(y % 4 == 3)
            J = -J;
         }
      if(x.is_zero())
         return 0;
      while(x % 4 == 0)
         x >>= 2;
      if(x.is_even())
         {
         x >>= 1;
         if(y % 8 == 3 || y % 8 == 5)
            J = -J;
         }
      if(x % 4 == 3 && y % 4 == 3)
         J = -J;
      std::swap(x, y);
      }
   return J;
   }

/*************************************************
* Exponentiation                                 *
*************************************************/
BigInt power(const BigInt& base, u32bit exp)
   {
   BigInt x = 1, a = base;
   while(exp)
      {
      if(exp % 2)
         x *= a;
      exp >>= 1;
      if(exp)
         a = square(a);
      }
   return x;
   }

#ifndef BOTAN_MINIMAL_BIGINT

/*************************************************
* Do simple tests of primality                   *
*************************************************/
s32bit simple_primality_tests(const BigInt& n)
   {
   const s32bit NOT_PRIME = -1, UNKNOWN = 0, PRIME = 1;

   if(n == 2)
      return PRIME;
   if(n <= 1 || n.is_even())
      return NOT_PRIME;

   if(n <= PRIMES[PRIME_TABLE_SIZE-1])
      {
      const u32bit num = n.word_at(0);
      for(u32bit j = 0; PRIMES[j]; j++)
         {
         if(num == PRIMES[j]) return PRIME;
         if(num <  PRIMES[j]) return NOT_PRIME;
         }
      return NOT_PRIME;
      }

   u32bit check_first = qMin(n.bits() / 32, PRIME_PRODUCTS_TABLE_SIZE);
   for(u32bit j = 0; j != check_first; j++)
      if(gcd(n, PRIME_PRODUCTS[j]) != 1)
         return NOT_PRIME;

   return UNKNOWN;
   }

/*************************************************
* Fast check of primality                        *
*************************************************/
bool check_prime(const BigInt& n)
   {
   return run_primality_tests(n, 0);
   }

/*************************************************
* Test for primality                             *
*************************************************/
bool is_prime(const BigInt& n)
   {
   return run_primality_tests(n, 1);
   }

/*************************************************
* Verify primality                               *
*************************************************/
bool verify_prime(const BigInt& n)
   {
   return run_primality_tests(n, 2);
   }

/*************************************************
* Verify primality                               *
*************************************************/
bool run_primality_tests(const BigInt& n, u32bit level)
   {
   s32bit simple_tests = simple_primality_tests(n);
   if(simple_tests) return (simple_tests == 1) ? true : false;
   return passes_mr_tests(n, level);
   }

/*************************************************
* Test for primaility using Miller-Rabin         *
*************************************************/
bool passes_mr_tests(const BigInt& n, u32bit level)
   {
   const u32bit PREF_NONCE_BITS = 40;

   if(level > 2)
      level = 2;

   MillerRabin_Test mr(n);

   if(!mr.passes_test(2))
      return false;

   if(level == 0)
      return true;

   const u32bit NONCE_BITS = qMin(n.bits() - 1, PREF_NONCE_BITS);

   const bool verify = (level == 2);

   u32bit tests = miller_rabin_test_iterations(n.bits(), verify);

   BigInt nonce;
   for(u32bit j = 0; j != tests; j++)
      {
      nonce = (verify) ? (random_integer(NONCE_BITS, Nonce)) : (PRIMES[j]);
      if(!mr.passes_test(nonce))
         return false;
      }
   return true;
   }

/*************************************************
* Miller-Rabin Test                              *
*************************************************/
bool MillerRabin_Test::passes_test(const BigInt& a)
   {
   if(a < 2 || a >= n_minus_1)
      throw Invalid_Argument("Bad size for 'a' in Miller-Rabin test");

   UI::pulse(UI::PRIME_TESTING);
   BigInt y = power_mod(a, r, reducer);

   if(y == 1 || y == n_minus_1)
      return true;

   for(u32bit j = 1; j != s; j++)
      {
      UI::pulse(UI::PRIME_TESTING);
      y = reducer->square(y);
      if(y == 1)
         return false;
      if(y == n_minus_1)
         return true;
      }
   return false;
   }

/*************************************************
* Miller-Rabin Constructor                       *
*************************************************/
MillerRabin_Test::MillerRabin_Test(const BigInt& num)
   {
   if(num.is_even() || num < 3)
      throw Invalid_Argument("MillerRabin_Test: Invalid number for testing");

   n = num;
   n_minus_1 = n - 1;
   r = n_minus_1;
   s = 0;

   while(r.is_even()) { s++; r >>= 1; }
   reducer = get_reducer(n);
   }

#endif // BOTAN_MINIMAL_BIGINT

}
}

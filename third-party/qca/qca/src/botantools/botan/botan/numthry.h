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
* Number Theory Header File                      *
* (C) 1999-2007 The Botan Project                *
*************************************************/

#ifndef BOTAN_NUMBTHRY_H__
#define BOTAN_NUMBTHRY_H__

} // WRAPNS_LINE
#include <botan/bigint.h>
namespace QCA { // WRAPNS_LINE
#ifndef BOTAN_MINIMAL_BIGINT
} // WRAPNS_LINE
#include <botan/reducer.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/pow_mod.h>
namespace QCA { // WRAPNS_LINE
#endif

namespace Botan {

#ifndef BOTAN_MINIMAL_BIGINT
/*************************************************
* Fused Arithmetic Operations                    *
*************************************************/
BigInt mul_add(const BigInt&, const BigInt&, const BigInt&);
BigInt sub_mul(const BigInt&, const BigInt&, const BigInt&);

/*************************************************
* Number Theory Functions                        *
*************************************************/
inline BigInt abs(const BigInt& n) { return n.abs(); }
#endif

void divide(const BigInt&, const BigInt&, BigInt&, BigInt&);

#ifndef BOTAN_MINIMAL_BIGINT
BigInt gcd(const BigInt&, const BigInt&);
BigInt lcm(const BigInt&, const BigInt&);

BigInt square(const BigInt&);
BigInt inverse_mod(const BigInt&, const BigInt&);
s32bit jacobi(const BigInt&, const BigInt&);

BigInt power_mod(const BigInt&, const BigInt&, const BigInt&);

/*************************************************
* Utility Functions                              *
*************************************************/
u32bit low_zero_bits(const BigInt&);

/*************************************************
* Primality Testing                              *
*************************************************/
bool check_prime(const BigInt&);
bool is_prime(const BigInt&);
bool verify_prime(const BigInt&);

s32bit simple_primality_tests(const BigInt&);
bool passes_mr_tests(const BigInt&, u32bit = 1);
bool run_primality_tests(const BigInt&, u32bit = 1);

/*************************************************
* Random Number Generation                       *
*************************************************/
BigInt random_integer(u32bit);
BigInt random_integer(const BigInt&, const BigInt&);
BigInt random_prime(u32bit, const BigInt& = 1, u32bit = 1, u32bit = 2);
BigInt random_safe_prime(u32bit);

SecureVector<byte> generate_dsa_primes(BigInt&, BigInt&, u32bit);
bool generate_dsa_primes(BigInt&, BigInt&, const byte[], u32bit, u32bit,
                         u32bit = 0);

/*************************************************
* Prime Numbers                                  *
*************************************************/
const u32bit PRIME_TABLE_SIZE = 6541;
const u32bit PRIME_PRODUCTS_TABLE_SIZE = 256;

extern const u16bit PRIMES[];
extern const u64bit PRIME_PRODUCTS[];

/*************************************************
* Miller-Rabin Primality Tester                  *
*************************************************/
class MillerRabin_Test
   {
   public:
      bool passes_test(const BigInt&);
      MillerRabin_Test(const BigInt&);
   private:
      BigInt n, r, n_minus_1;
      u32bit s;
      Fixed_Exponent_Power_Mod pow_mod;
      Modular_Reducer reducer;
   };
#endif

}

#endif
} // WRAPNS_LINE

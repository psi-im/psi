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
* Lowest Level MPI Algorithms Header File        *
* (C) 1999-2007 The Botan Project                *
*************************************************/

#ifndef BOTAN_MP_ASM_H__
#define BOTAN_MP_ASM_H__

} // WRAPNS_LINE
#include <botan/mp_types.h>
namespace QCA { // WRAPNS_LINE

#if (BOTAN_MP_WORD_BITS == 8)
  typedef Botan::u16bit dword;
#elif (BOTAN_MP_WORD_BITS == 16)
  typedef Botan::u32bit dword;
#elif (BOTAN_MP_WORD_BITS == 32)
  typedef Botan::u64bit dword;
#elif (BOTAN_MP_WORD_BITS == 64)
  #error BOTAN_MP_WORD_BITS can be 64 only with assembly support
#else
  #error BOTAN_MP_WORD_BITS must be 8, 16, 32, or 64
#endif

namespace Botan {

extern "C" {

/*************************************************
* Word Multiply/Add                              *
*************************************************/
inline word word_madd2(word a, word b, word c, word* carry)
   {
   dword z = (dword)a * b + c;
   *carry = (word)(z >> BOTAN_MP_WORD_BITS);
   return (word)z;
   }

/*************************************************
* Word Multiply/Add                              *
*************************************************/
inline word word_madd3(word a, word b, word c, word d, word* carry)
   {
   dword z = (dword)a * b + c + d;
   *carry = (word)(z >> BOTAN_MP_WORD_BITS);
   return (word)z;
   }

}

}

#endif
} // WRAPNS_LINE

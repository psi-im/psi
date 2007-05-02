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
* Utility Functions Source File                  *
* (C) 1999-2007 The Botan Project                *
*************************************************/

} // WRAPNS_LINE
#include <botan/util.h>
namespace QCA { // WRAPNS_LINE
#ifndef BOTAN_TOOLS_ONLY
} // WRAPNS_LINE
#include <botan/bit_ops.h>
namespace QCA { // WRAPNS_LINE
#endif
} // WRAPNS_LINE
#include <algorithm>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <cmath>
namespace QCA { // WRAPNS_LINE

namespace Botan {

/*************************************************
* Round up n to multiple of align_to             *
*************************************************/
u32bit round_up(u32bit n, u32bit align_to)
   {
   if(n % align_to || n == 0)
      n += align_to - (n % align_to);
   return n;
   }

/*************************************************
* Round down n to multiple of align_to           *
*************************************************/
u32bit round_down(u32bit n, u32bit align_to)
   {
   return (n - (n % align_to));
   }

#ifndef BOTAN_TOOLS_ONLY
/*************************************************
* Return the work required for solving DL        *
*************************************************/
u32bit dl_work_factor(u32bit n_bits)
   {
   const u32bit MIN_ESTIMATE = 64;

   if(n_bits < 32)
      return 0;

   const double log_x = n_bits / 1.44;

   u32bit estimate = (u32bit)(2.76 * std::pow(log_x, 1.0/3.0) *
                                     std::pow(std::log(log_x), 2.0/3.0));

   return std::max(estimate, MIN_ESTIMATE);
   }

/*************************************************
* Estimate the entropy of the buffer             *
*************************************************/
u32bit entropy_estimate(const byte buffer[], u32bit length)
   {
   if(length <= 4)
      return 0;

   u32bit estimate = 0;
   byte last = 0, last_delta = 0, last_delta2 = 0;

   for(u32bit j = 0; j != length; ++j)
      {
      byte delta = last ^ buffer[j];
      last = buffer[j];

      byte delta2 = delta ^ last_delta;
      last_delta = delta;

      byte delta3 = delta2 ^ last_delta2;
      last_delta2 = delta2;

      byte min_delta = delta;
      if(min_delta > delta2) min_delta = delta2;
      if(min_delta > delta3) min_delta = delta3;

      estimate += hamming_weight(min_delta);
      }

   return (estimate / 2);
   }
#endif

}
} // WRAPNS_LINE

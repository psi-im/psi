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
* MP Multiplication Source File                  *
* (C) 1999-2004 The Botan Project                *
*************************************************/

}
#include <botan/mp_core.h>
namespace QCA {

namespace Botan {

namespace {

/*************************************************
* Length Checking                                *
*************************************************/
bool use_op(u32bit x_sw, u32bit y_sw,
            u32bit x_size, u32bit y_size, u32bit z_size,
            u32bit limit, u32bit min = 0)
   {
   return (x_sw <= limit && y_sw <= limit &&
           x_size >= limit && y_size >= limit && z_size >= 2*limit &&
           (x_sw + y_sw) >= min);
   }

/*************************************************
* Attempt a Karatsuba multiply                   *
*************************************************/
bool do_karat(word z[], u32bit z_size,
              const word x[], u32bit x_size, u32bit x_sw,
              const word y[], u32bit y_size, u32bit y_sw)
   {
   const u32bit KARAT_12_BOUND = 20;
   const u32bit KARAT_16_BOUND = 24;
   const u32bit KARAT_24_BOUND = 38;
   const u32bit KARAT_32_BOUND = 46;
   const u32bit KARAT_48_BOUND = 66;
   const u32bit KARAT_64_BOUND = 80;
   const u32bit KARAT_96_BOUND = 114;
   const u32bit KARAT_128_BOUND = 136;

   if(use_op(x_sw, y_sw, x_size, y_size, z_size, 12, KARAT_12_BOUND))
      bigint_karat12(z, x, y);
   else if(use_op(x_sw, y_sw, x_size, y_size, z_size, 16, KARAT_16_BOUND))
      bigint_karat16(z, x, y);
   else if(use_op(x_sw, y_sw, x_size, y_size, z_size, 24, KARAT_24_BOUND))
      bigint_karat24(z, x, y);
   else if(use_op(x_sw, y_sw, x_size, y_size, z_size, 32, KARAT_32_BOUND))
      bigint_karat32(z, x, y);
   else if(use_op(x_sw, y_sw, x_size, y_size, z_size, 48, KARAT_48_BOUND))
      bigint_karat48(z, x, y);
   else if(use_op(x_sw, y_sw, x_size, y_size, z_size, 64, KARAT_64_BOUND))
      bigint_karat64(z, x, y);
   else if(use_op(x_sw, y_sw, x_size, y_size, z_size, 96, KARAT_96_BOUND))
      bigint_karat96(z, x, y);
   else if(use_op(x_sw, y_sw, x_size, y_size, z_size, 128, KARAT_128_BOUND))
      bigint_karat128(z, x, y);
   else
      return false;

   return true;
   }


}

/*************************************************
* MP Multiplication Algorithm Dispatcher         *
*************************************************/
void bigint_mul3(word z[], u32bit z_size,
                 const word x[], u32bit x_size, u32bit x_sw,
                 const word y[], u32bit y_size, u32bit y_sw)
   {
   if(x_sw == 1)      bigint_linmul3(z, y, y_sw, x[0]);
   else if(y_sw == 1) bigint_linmul3(z, x, x_sw, y[0]);

   else if(use_op(x_sw, y_sw, x_size, y_size, z_size, 4))
      bigint_comba4(z, x, y);
   else if(use_op(x_sw, y_sw, x_size, y_size, z_size, 6))
      bigint_comba6(z, x, y);
   else if(use_op(x_sw, y_sw, x_size, y_size, z_size, 8))
      bigint_comba8(z, x, y);
   else if(!do_karat(z, z_size, x, x_size, x_sw, y, y_size, y_sw))
      bigint_smul(z, x, x_sw, y, y_sw);
   }

}
}

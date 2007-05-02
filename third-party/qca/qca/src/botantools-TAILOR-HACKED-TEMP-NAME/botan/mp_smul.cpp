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
* Simple Multiplication Source File              *
* (C) 1999-2004 The Botan Project                *
*************************************************/

}
#include <botan/mp_core.h>
namespace QCA {
}
#include <botan/mp_madd.h>
namespace QCA {
}
#include <botan/mem_ops.h>
namespace QCA {

namespace Botan {

/*************************************************
* Two Operand Linear Multiply                    *
*************************************************/
void bigint_linmul2(word x[], u32bit x_size, word y)
   {
   word carry = 0;
   for(u32bit j = 0; j != x_size; j++)
      bigint_madd(x[j], y, carry, 0, x + j, &carry);
   x[x_size] = carry;
   }

/*************************************************
* Three Operand Linear Multiply                  *
*************************************************/
void bigint_linmul3(word z[], const word x[], u32bit x_size, word y)
   {
   word carry = 0;
   for(u32bit j = 0; j != x_size; j++)
      bigint_madd(x[j], y, carry, 0, z + j, &carry);
   z[x_size] = carry;
   }

/*************************************************
* Simple O(N^2) Multiplication                   *
*************************************************/
void bigint_smul(word z[], const word x[], u32bit x_size,
                           const word y[], u32bit y_size)
   {
   const u32bit blocks = y_size - (y_size % 8);

   clear_mem(z, x_size + y_size);

   for(u32bit j = 0; j != x_size; j++)
      {
      const word x_j = x[j];

      word carry = 0;

      for(u32bit k = 0; k != blocks; k += 8)
         {
         bigint_madd(x_j, y[k+0], z[j+k+0], carry, z + (j+k+0), &carry);
         bigint_madd(x_j, y[k+1], z[j+k+1], carry, z + (j+k+1), &carry);
         bigint_madd(x_j, y[k+2], z[j+k+2], carry, z + (j+k+2), &carry);
         bigint_madd(x_j, y[k+3], z[j+k+3], carry, z + (j+k+3), &carry);
         bigint_madd(x_j, y[k+4], z[j+k+4], carry, z + (j+k+4), &carry);
         bigint_madd(x_j, y[k+5], z[j+k+5], carry, z + (j+k+5), &carry);
         bigint_madd(x_j, y[k+6], z[j+k+6], carry, z + (j+k+6), &carry);
         bigint_madd(x_j, y[k+7], z[j+k+7], carry, z + (j+k+7), &carry);
         }

      for(u32bit k = blocks; k != y_size; k++)
         bigint_madd(x_j, y[k], z[j+k], carry, z + (j+k), &carry);
      z[j+y_size] = carry;
      }
   }

}
}

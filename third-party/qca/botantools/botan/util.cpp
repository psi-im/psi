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
* Utility Functions Source File                  *
* (C) 1999-2004 The Botan Project                *
*************************************************/

}
#include <botan/util.h>
namespace QCA {
}
#include <botan/exceptn.h>
namespace QCA {
}
namespace QCA {

namespace Botan {

#ifndef BOTAN_TOOLS_ONLY

/*************************************************
* XOR arrays together                            *
*************************************************/
void xor_buf(byte data[], const byte mask[], u32bit length)
   {
   while(length >= 8)
      {
      data[0] ^= mask[0]; data[1] ^= mask[1];
      data[2] ^= mask[2]; data[3] ^= mask[3];
      data[4] ^= mask[4]; data[5] ^= mask[5];
      data[6] ^= mask[6]; data[7] ^= mask[7];
      data += 8; mask += 8; length -= 8;
      }
   for(u32bit j = 0; j != length; j++)
      data[j] ^= mask[j];
   }

void xor_buf(byte out[], const byte in[], const byte mask[], u32bit length)
   {
   while(length >= 8)
      {
      out[0] = in[0] ^ mask[0]; out[1] = in[1] ^ mask[1];
      out[2] = in[2] ^ mask[2]; out[3] = in[3] ^ mask[3];
      out[4] = in[4] ^ mask[4]; out[5] = in[5] ^ mask[5];
      out[6] = in[6] ^ mask[6]; out[7] = in[7] ^ mask[7];
      in += 8; out += 8; mask += 8; length -= 8;
      }
   for(u32bit j = 0; j != length; j++)
      out[j] = in[j] ^ mask[j];
   }

/*************************************************
* Byte Reversal Functions                        *
*************************************************/
u16bit reverse_bytes(u16bit input)
   {
   return rotate_left(input, 8);
   }

u32bit reverse_bytes(u32bit input)
   {
   input = ((input & 0xFF00FF00) >> 8) | ((input & 0x00FF00FF) << 8);
   return rotate_left(input, 16);
   }

u64bit reverse_bytes(u64bit input)
   {
   input = ((input & 0xFF00FF00FF00FF00) >>  8) |
           ((input & 0x00FF00FF00FF00FF) <<  8);
   input = ((input & 0xFFFF0000FFFF0000) >> 16) |
           ((input & 0x0000FFFF0000FFFF) << 16);
   return rotate_left(input, 32);
   }

/*************************************************
* Bit Reversal Functions                         *
*************************************************/
byte reverse_bits(byte input)
   {
   input = ((input & 0xAA) >> 1) | ((input & 0x55) << 1);
   input = ((input & 0xCC) >> 2) | ((input & 0x33) << 2);
   return rotate_left(input, 4);
   }

u16bit reverse_bits(u16bit input)
   {
   input = ((input & 0xAAAA) >> 1) | ((input & 0x5555) << 1);
   input = ((input & 0xCCCC) >> 2) | ((input & 0x3333) << 2);
   input = ((input & 0xF0F0) >> 4) | ((input & 0x0F0F) << 4);
   return reverse_bytes(input);
   }

u32bit reverse_bits(u32bit input)
   {
   input = ((input & 0xAAAAAAAA) >> 1) | ((input & 0x55555555) << 1);
   input = ((input & 0xCCCCCCCC) >> 2) | ((input & 0x33333333) << 2);
   input = ((input & 0xF0F0F0F0) >> 4) | ((input & 0x0F0F0F0F) << 4);
   return reverse_bytes(input);
   }

u64bit reverse_bits(u64bit input)
   {
   input = ((input & 0xAAAAAAAAAAAAAAAA) >> 1) |
           ((input & 0x5555555555555555) << 1);
   input = ((input & 0xCCCCCCCCCCCCCCCC) >> 2) |
           ((input & 0x3333333333333333) << 2);
   input = ((input & 0xF0F0F0F0F0F0F0F0) >> 4) |
           ((input & 0x0F0F0F0F0F0F0F0F) << 4);
   return reverse_bytes(input);
   }

/*************************************************
* Return true iff arg is 2**n for some n > 0     *
*************************************************/
bool power_of_2(u64bit arg)
   {
   if(arg == 0 || arg == 1)
      return false;
   if((arg & (arg-1)) == 0)
      return true;
   return false;
   }

/*************************************************
* Combine a two time values into a single one    *
*************************************************/
u64bit combine_timers(u32bit seconds, u32bit parts, u32bit parts_hz)
   {
   const u64bit NANOSECONDS_UNITS = 1000000000;
   parts *= (NANOSECONDS_UNITS / parts_hz);
   return ((seconds * NANOSECONDS_UNITS) + parts);
   }

/*************************************************
* Return the index of the highest set bit        *
*************************************************/
u32bit high_bit(u64bit n)
   {
   for(u32bit count = 64; count > 0; count--)
      if((n >> (count - 1)) & 0x01)
         return count;
   return 0;
   }

/*************************************************
* Return the index of the lowest set bit         *
*************************************************/
u32bit low_bit(u64bit n)
   {
   for(u32bit count = 0; count != 64; count++)
      if((n >> count) & 0x01)
         return (count + 1);
   return 0;
   }

/*************************************************
* Return the number of significant bytes in n    *
*************************************************/
u32bit significant_bytes(u64bit n)
   {
   for(u32bit j = 0; j != 8; j++)
      if(get_byte(j, n))
         return 8-j;
   return 0;
   }

/*************************************************
* Return the Hamming weight of n                 *
*************************************************/
u32bit hamming_weight(u64bit n)
   {
   u32bit weight = 0;
   for(u32bit j = 0; j != 64; j++)
      if((n >> j) & 0x01)
         weight++;
   return weight;
   }

#endif // BOTAN_TOOLS_ONLY

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

#endif // BOTAN_TOOLS_ONLY

/*************************************************
* Convert an integer into a string               *
*************************************************/
std::string to_string(u64bit n, u32bit min_len)
   {
   std::string lenstr;
   if(n)
      {
      while(n > 0)
         {
         lenstr = digit2char(n % 10) + lenstr;
         n /= 10;
         }
      }
   else
      lenstr = "0";

   while(lenstr.size() < min_len)
      lenstr = "0" + lenstr; //krazy:exclude=doublequote_chars

   return lenstr;
   }

/*************************************************
* Convert an integer into a string               *
*************************************************/
u32bit to_u32bit(const std::string& number)
   {
   u32bit n = 0;

   for(std::string::const_iterator j = number.begin(); j != number.end(); j++)
      {
      const u32bit OVERFLOW_MARK = 0xFFFFFFFF / 10;

      byte digit = char2digit(*j);

      if((n > OVERFLOW_MARK) || (n == OVERFLOW_MARK && digit > 5))
         throw Decoding_Error("to_u32bit: Integer overflow");
      n *= 10;
      n += digit;
      }
   return n;
   }

/*************************************************
* Check if a character represents a digit        *
*************************************************/
bool is_digit(char c)
   {
   if(c == '0' || c == '1' || c == '2' || c == '3' || c == '4' ||
      c == '5' || c == '6' || c == '7' || c == '8' || c == '9')
      return true;
   return false;
   }

/*************************************************
* Check if a character represents whitespace     *
*************************************************/
bool is_space(char c)
   {
   if(c == ' ' || c == '\t' || c == '\n' || c == '\r')
      return true;
   return false;
   }

/*************************************************
* Convert a character to a digit                 *
*************************************************/
byte char2digit(char c)
   {
   if(c == '0') return 0;
   if(c == '1') return 1;
   if(c == '2') return 2;
   if(c == '3') return 3;
   if(c == '4') return 4;
   if(c == '5') return 5;
   if(c == '6') return 6;
   if(c == '7') return 7;
   if(c == '8') return 8;
   if(c == '9') return 9;
   throw Invalid_Argument("char2digit: Invalid decimal char " + c);
   }

/*************************************************
* Convert a digit to a character                 *
*************************************************/
char digit2char(byte b)
   {
   if(b == 0) return '0';
   if(b == 1) return '1';
   if(b == 2) return '2';
   if(b == 3) return '3';
   if(b == 4) return '4';
   if(b == 5) return '5';
   if(b == 6) return '6';
   if(b == 7) return '7';
   if(b == 8) return '8';
   if(b == 9) return '9';
   throw Invalid_Argument("digit2char: Input is not a digit");
   }

#ifndef BOTAN_TOOLS_ONLY

/*************************************************
* Estimate the entropy of the buffer             *
*************************************************/
u32bit entropy_estimate(const byte buffer[], u32bit length)
   {
   if(length <= 4)
      return 0;

   u32bit estimate = 0;
   byte last = 0, last_delta = 0, last_delta2 = 0;

   for(u32bit j = 0; j != length; j++)
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

#endif // BOTAN_TOOLS_ONLY

}
}

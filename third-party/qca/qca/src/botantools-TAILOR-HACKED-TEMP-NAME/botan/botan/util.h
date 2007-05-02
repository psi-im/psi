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
* Utility Functions Header File                  *
* (C) 1999-2004 The Botan Project                *
*************************************************/

#ifndef BOTAN_UTIL_H__
#define BOTAN_UTIL_H__

}
#include <cstring>
namespace QCA {
}
#include <string>
namespace QCA {

#ifdef BOTAN_TOOLS_ONLY
}
# include <botan/types.h>
namespace QCA {
#else
}
# include <botan/secmem.h>
namespace QCA {
}
# include <botan/enums.h>
namespace QCA {
}
# include <vector>
namespace QCA {
#endif

namespace Botan {

/*************************************************
* Rotation Functions                             *
*************************************************/
template<typename T> inline T rotate_left(T input, u32bit rot)
   { return (T)((input << rot) | (input >> (8*sizeof(T)-rot))); }

template<typename T> inline T rotate_right(T input, u32bit rot)
   { return (T)((input >> rot) | (input << (8*sizeof(T)-rot))); }

/*************************************************
* Byte Extraction Function                       *
*************************************************/
template<typename T> inline byte get_byte(u32bit byte_num, T input)
   { return (byte)(input >> ((sizeof(T)-1-(byte_num&(sizeof(T)-1))) << 3)); }

/*************************************************
* Byte to Word Conversions                       *
*************************************************/
inline u16bit make_u16bit(byte input0, byte input1)
   { return (u16bit)(((u16bit)input0 << 8) | input1); }

inline u32bit make_u32bit(byte input0, byte input1, byte input2, byte input3)
   { return (u32bit)(((u32bit)input0 << 24) | ((u32bit)input1 << 16) |
                     ((u32bit)input2 <<  8) | input3); }

inline u64bit make_u64bit(byte input0, byte input1, byte input2, byte input3,
                          byte input4, byte input5, byte input6, byte input7)
   {
   return (u64bit)(((u64bit)input0 << 56) | ((u64bit)input1 << 48) |
                   ((u64bit)input2 << 40) | ((u64bit)input3 << 32) |
                   ((u64bit)input4 << 24) | ((u64bit)input5 << 16) |
                   ((u64bit)input6 <<  8) | input7);
   }

#ifndef BOTAN_TOOLS_ONLY

/*************************************************
* XOR Functions                                  *
*************************************************/
void xor_buf(byte[], const byte[], u32bit);
void xor_buf(byte[], const byte[], const byte[], u32bit);

/*************************************************
* Byte Swapping Functions                        *
*************************************************/
u16bit reverse_bytes(u16bit);
u32bit reverse_bytes(u32bit);
u64bit reverse_bytes(u64bit);

/*************************************************
* Bit Swapping Functions                         *
*************************************************/
byte reverse_bits(byte);
u16bit reverse_bits(u16bit);
u32bit reverse_bits(u32bit);
u64bit reverse_bits(u64bit);

/*************************************************
* Timer Access Functions                         *
*************************************************/
u64bit system_time();
u64bit system_clock();

#endif // BOTAN_TOOLS_ONLY

/*************************************************
* Memory Locking Functions                       *
*************************************************/
void lock_mem(void*, u32bit);
void unlock_mem(void*, u32bit);

#ifndef BOTAN_TOOLS_ONLY

/*************************************************
* Parsing functions                              *
*************************************************/
std::vector<std::string> parse_algorithm_name(const std::string&);
std::vector<std::string> split_on(const std::string&, char);
std::vector<u32bit> parse_asn1_oid(const std::string&);
bool x500_name_cmp(const std::string&, const std::string&);

/*************************************************
* Misc Utility Functions                         *
*************************************************/
bool power_of_2(u64bit);
u32bit high_bit(u64bit);
u32bit low_bit(u64bit);
u32bit significant_bytes(u64bit);
u32bit hamming_weight(u64bit);
#endif // BOTAN_TOOLS_ONLY
u32bit round_up(u32bit, u32bit);
u32bit round_down(u32bit, u32bit);
#ifndef BOTAN_TOOLS_ONLY
u64bit combine_timers(u32bit, u32bit, u32bit);

/*************************************************
* Work Factor Estimates                          *
*************************************************/
u32bit entropy_estimate(const byte[], u32bit);
u32bit dl_work_factor(u32bit);

#endif // BOTAN_TOOLS_ONLY

/*************************************************
* String/Integer Conversions                     *
*************************************************/
std::string to_string(u64bit, u32bit = 0);
u32bit to_u32bit(const std::string&);

/*************************************************
* Character Set Handling                         *
*************************************************/
bool is_digit(char);
bool is_space(char);
char to_lower(char);

byte char2digit(char);
char digit2char(byte);

#ifndef BOTAN_TOOLS_ONLY

std::string local2iso(const std::string&);
std::string iso2local(const std::string&);

std::string utf2iso(const std::string&);
std::string iso2utf(const std::string&);

/*************************************************
* Return the values of various defined HashIDs   *
*************************************************/
MemoryVector<byte> pkcs_hash_id(const std::string&);
byte ieee1363_hash_id(const std::string&);

#endif // BOTAN_TOOLS_ONLY

}

#endif
}

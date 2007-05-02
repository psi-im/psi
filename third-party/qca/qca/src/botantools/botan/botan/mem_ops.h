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
* Memory Operations Header File                  *
* (C) 1999-2007 The Botan Project                *
*************************************************/

#ifndef BOTAN_MEMORY_OPS_H__
#define BOTAN_MEMORY_OPS_H__

} // WRAPNS_LINE
#include <botan/types.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <cstring>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <string.h>
namespace QCA { // WRAPNS_LINE

namespace Botan {

/*************************************************
* Memory Manipulation Functions                  *
*************************************************/
template<typename T> inline void copy_mem(T* out, const T* in, u32bit n)
   { memmove(out, in, sizeof(T)*n); }

template<typename T> inline void clear_mem(T* ptr, u32bit n)
   { memset(ptr, 0, sizeof(T)*n); }

template<typename T> inline void set_mem(T* ptr, u32bit n, byte val)
   { memset(ptr, val, sizeof(T)*n); }

template<typename T> inline bool same_mem(const T* p1, const T* p2, u32bit n)
   { return (memcmp(p1, p2, sizeof(T)*n) == 0); }

}

#endif
} // WRAPNS_LINE

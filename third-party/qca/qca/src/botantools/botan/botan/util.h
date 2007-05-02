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
* Utility Functions Header File                  *
* (C) 1999-2007 The Botan Project                *
*************************************************/

#ifndef BOTAN_UTIL_H__
#define BOTAN_UTIL_H__

} // WRAPNS_LINE
#include <botan/types.h>
namespace QCA { // WRAPNS_LINE

namespace Botan {

/*************************************************
* Timer Access Functions                         *
*************************************************/
#ifndef BOTAN_TOOLS_ONLY
u64bit system_time();
u64bit system_clock();
#endif

/*************************************************
* Memory Locking Functions                       *
*************************************************/
void lock_mem(void*, u32bit);
void unlock_mem(void*, u32bit);

/*************************************************
* Misc Utility Functions                         *
*************************************************/
u32bit round_up(u32bit, u32bit);
u32bit round_down(u32bit, u32bit);
#ifndef BOTAN_TOOLS_ONLY
u64bit combine_timers(u32bit, u32bit, u32bit);
#endif

/*************************************************
* Work Factor Estimates                          *
*************************************************/
#ifndef BOTAN_TOOLS_ONLY
u32bit entropy_estimate(const byte[], u32bit);
u32bit dl_work_factor(u32bit);
#endif

}

#endif
} // WRAPNS_LINE

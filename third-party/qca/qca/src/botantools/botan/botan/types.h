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
* Low Level Types Header File                    *
* (C) 1999-2007 The Botan Project                *
*************************************************/

#ifndef BOTAN_TYPES_H__
#define BOTAN_TYPES_H__

#ifdef BOTAN_TYPES_QT
} // WRAPNS_LINE
#include <QtGlobal>
namespace QCA { // WRAPNS_LINE
#else
} // WRAPNS_LINE
#include <botan/build.h>
namespace QCA { // WRAPNS_LINE
#endif

namespace Botan {

#ifdef BOTAN_TYPES_QT

typedef quint8 byte;
typedef quint16 u16bit;
typedef quint32 u32bit;
typedef qint32 s32bit;
typedef quint64 u64bit;

#else

typedef unsigned char byte;
typedef unsigned short u16bit;
typedef unsigned int u32bit;

typedef signed int s32bit;

#if defined(_MSC_VER) || defined(__BORLANDC__)
   typedef unsigned __int64 u64bit;
#elif defined(__KCC)
   typedef unsigned __long_long u64bit;
#elif defined(__GNUG__)
   __extension__ typedef unsigned long long u64bit;
#else
   typedef unsigned long long u64bit;
#endif

#endif // BOTAN_TYPES_QT

}

namespace Botan_types {

typedef Botan::byte byte;
typedef Botan::u32bit u32bit;

}

#endif
} // WRAPNS_LINE

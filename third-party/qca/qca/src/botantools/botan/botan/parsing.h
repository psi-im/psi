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
* Parser Functions Header File                   *
* (C) 1999-2007 The Botan Project                *
*************************************************/

#ifndef BOTAN_PARSER_H__
#define BOTAN_PARSER_H__

} // WRAPNS_LINE
#include <botan/types.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <string>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <vector>
namespace QCA { // WRAPNS_LINE

namespace Botan {

/*************************************************
* String Parsing Functions                       *
*************************************************/
#ifndef BOTAN_TOOLS_ONLY
std::vector<std::string> parse_algorithm_name(const std::string&);
std::vector<std::string> split_on(const std::string&, char);
std::vector<u32bit> parse_asn1_oid(const std::string&);
bool x500_name_cmp(const std::string&, const std::string&);
u32bit parse_expr(const std::string&);
#endif

/*************************************************
* String/Integer Conversions                     *
*************************************************/
std::string to_string(u64bit, u32bit = 0);
#ifndef BOTAN_TOOLS_ONLY
u32bit to_u32bit(const std::string&);
#endif

}

#endif
} // WRAPNS_LINE

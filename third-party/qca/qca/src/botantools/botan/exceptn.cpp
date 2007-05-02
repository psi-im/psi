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
* Exceptions Source File                         *
* (C) 1999-2007 The Botan Project                *
*************************************************/

} // WRAPNS_LINE
#include <botan/exceptn.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <botan/parsing.h>
namespace QCA { // WRAPNS_LINE

namespace Botan {

/*************************************************
* Constructor for Invalid_Key_Length             *
*************************************************/
Invalid_Key_Length::Invalid_Key_Length(const std::string& name, u32bit length)
   {
   set_msg(name + " cannot accept a key of length " + to_string(length));
   }

/*************************************************
* Constructor for Invalid_Block_Size             *
*************************************************/
Invalid_Block_Size::Invalid_Block_Size(const std::string& mode,
                                       const std::string& pad)
   {
   set_msg("Padding method " + pad + " cannot be used with " + mode);
   }

/*************************************************
* Constructor for Invalid_IV_Length              *
*************************************************/
Invalid_IV_Length::Invalid_IV_Length(const std::string& mode, u32bit bad_len)
   {
   set_msg("IV length " + to_string(bad_len) + " is invalid for " + mode);
   }

/*************************************************
* Constructor for Invalid_Message_Number         *
*************************************************/
Invalid_Message_Number::Invalid_Message_Number(const std::string& where,
                                               u32bit message_no)
   {
   set_msg("Pipe::" + where + ": Invalid message number " +
           to_string(message_no));
   }

/*************************************************
* Constructor for Algorithm_Not_Found            *
*************************************************/
Algorithm_Not_Found::Algorithm_Not_Found(const std::string& name)
   {
   set_msg("Could not find any algorithm named \"" + name + "\"");
   }

/*************************************************
* Constructor for Invalid_Algorithm_Name         *
*************************************************/
Invalid_Algorithm_Name::Invalid_Algorithm_Name(const std::string& name)
   {
   set_msg("Invalid algorithm name: " + name);
   }

/*************************************************
* Constructor for Config_Error                   *
*************************************************/
Config_Error::Config_Error(const std::string& err, u32bit line)
   {
   set_msg("Config error at line " + to_string(line) + ": " + err);
   }

}
} // WRAPNS_LINE

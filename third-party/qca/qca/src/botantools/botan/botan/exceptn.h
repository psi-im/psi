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
* Exceptions Header File                         *
* (C) 1999-2007 The Botan Project                *
*************************************************/

#ifndef BOTAN_EXCEPTION_H__
#define BOTAN_EXCEPTION_H__

} // WRAPNS_LINE
#include <botan/types.h>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <exception>
namespace QCA { // WRAPNS_LINE
} // WRAPNS_LINE
#include <string>
namespace QCA { // WRAPNS_LINE

namespace Botan {

/*************************************************
* Exception Base Class                           *
*************************************************/
class Exception : public std::exception
   {
   public:
      const char* what() const throw() { return msg.c_str(); }
      Exception(const std::string& m = "Unknown error") { set_msg(m); }
      virtual ~Exception() throw() {}
   protected:
      void set_msg(const std::string& m) { msg = "Botan: " + m; }
   private:
      std::string msg;
   };

/*************************************************
* Invalid_Argument Exception                     *
*************************************************/
struct Invalid_Argument : public Exception
   {
   Invalid_Argument(const std::string& err = "") : Exception(err) {}
   };

/*************************************************
* Invalid_Key_Length Exception                   *
*************************************************/
struct Invalid_Key_Length : public Invalid_Argument
   {
   Invalid_Key_Length(const std::string&, u32bit);
   };

/*************************************************
* Invalid_Block_Size Exception                   *
*************************************************/
struct Invalid_Block_Size : public Invalid_Argument
   {
   Invalid_Block_Size(const std::string&, const std::string&);
   };

/*************************************************
* Invalid_IV_Length Exception                    *
*************************************************/
struct Invalid_IV_Length : public Invalid_Argument
   {
   Invalid_IV_Length(const std::string&, u32bit);
   };

/*************************************************
* Invalid_Message_Number Exception               *
*************************************************/
struct Invalid_Message_Number : public Invalid_Argument
   {
   Invalid_Message_Number(const std::string&, u32bit);
   };

/*************************************************
* Invalid_State Exception                        *
*************************************************/
struct Invalid_State : public Exception
   {
   Invalid_State(const std::string& err) : Exception(err) {}
   };

/*************************************************
* PRNG_Unseeded Exception                        *
*************************************************/
struct PRNG_Unseeded : public Invalid_State
   {
   PRNG_Unseeded(const std::string& algo) :
      Invalid_State("PRNG not seeded: " + algo) {}
   };

/*************************************************
* Policy_Violation Exception                     *
*************************************************/
struct Policy_Violation : public Invalid_State
   {
   Policy_Violation(const std::string& err) :
      Invalid_State("Policy violation: " + err) {}
   };

/*************************************************
* Lookup_Error Exception                         *
*************************************************/
struct Lookup_Error : public Exception
   {
   Lookup_Error(const std::string& err) : Exception(err) {}
   };

/*************************************************
* Algorithm_Not_Found Exception                  *
*************************************************/
struct Algorithm_Not_Found : public Exception
   {
   Algorithm_Not_Found(const std::string&);
   };

/*************************************************
* Format_Error Exception                         *
*************************************************/
struct Format_Error : public Exception
   {
   Format_Error(const std::string& err = "") : Exception(err) {}
   };

/*************************************************
* Invalid_Algorithm_Name Exception               *
*************************************************/
struct Invalid_Algorithm_Name : public Format_Error
   {
   Invalid_Algorithm_Name(const std::string&);
   };

/*************************************************
* Encoding_Error Exception                       *
*************************************************/
struct Encoding_Error : public Format_Error
   {
   Encoding_Error(const std::string& name) :
      Format_Error("Encoding error: " + name) {}
   };

/*************************************************
* Decoding_Error Exception                       *
*************************************************/
struct Decoding_Error : public Format_Error
   {
   Decoding_Error(const std::string& name) :
      Format_Error("Decoding error: " + name) {}
   };

/*************************************************
* Invalid_OID Exception                          *
*************************************************/
struct Invalid_OID : public Decoding_Error
   {
   Invalid_OID(const std::string& oid) :
      Decoding_Error("Invalid ASN.1 OID: " + oid) {}
   };

/*************************************************
* Stream_IO_Error Exception                      *
*************************************************/
struct Stream_IO_Error : public Exception
   {
   Stream_IO_Error(const std::string& err) :
      Exception("I/O error: " + err) {}
   };

/*************************************************
* Configuration Error Exception                  *
*************************************************/
struct Config_Error : public Format_Error
   {
   Config_Error(const std::string& err) :
      Format_Error("Config error: " + err) {}
   Config_Error(const std::string&, u32bit);
   };

/*************************************************
* Integrity Failure Exception                    *
*************************************************/
struct Integrity_Failure : public Exception
   {
   Integrity_Failure(const std::string& err) :
      Exception("Integrity failure: " + err) {}
   };

/*************************************************
* Internal_Error Exception                       *
*************************************************/
struct Internal_Error : public Exception
   {
   Internal_Error(const std::string& err) :
      Exception("Internal error: " + err) {}
   };

/*************************************************
* Self Test Failure Exception                    *
*************************************************/
struct Self_Test_Failure : public Internal_Error
   {
   Self_Test_Failure(const std::string& err) :
      Internal_Error("Self test failed: " + err) {}
   };

/*************************************************
* Memory Allocation Exception                    *
*************************************************/
struct Memory_Exhaustion : public Exception
   {
   Memory_Exhaustion() :
      Exception("Ran out of memory, allocation failed") {}
   };

}

#endif
} // WRAPNS_LINE

/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Because global constant initialization order is undefined
// globals cannot depend on other objects to be instantiated.
// This class creates string objects within static methods 
// such that globals may refer to these constants by the
// accessor function and they are guaranteed to be initialized.

#ifndef TALK_XMLLITE_CONSTANTS_H_
#define TALK_XMLLITE_CONSTANTS_H_

#include <string>

#define STR_EMPTY    XmlConstants::str_empty()
#define NS_XML       XmlConstants::ns_xml()
#define NS_XMLNS     XmlConstants::ns_xmlns()
#define STR_XMLNS    XmlConstants::str_xmlns()
#define STR_XML      XmlConstants::str_xml()
#define STR_VERSION  XmlConstants::str_version()
#define STR_ENCODING XmlConstants::str_encoding()
namespace buzz {
	
class XmlConstants {
 public:
  static const std::string & str_empty();
  static const std::string & ns_xml();
  static const std::string & ns_xmlns();
  static const std::string & str_xmlns();
  static const std::string & str_xml();
  static const std::string & str_version();
  static const std::string & str_encoding();
};

}

#endif  // TALK_XMLLITE_CONSTANTS_H_

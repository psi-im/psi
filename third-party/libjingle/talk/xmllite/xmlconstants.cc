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

#include "xmlconstants.h"

using namespace buzz;

const std::string & XmlConstants::str_empty() {
  static const std::string str_empty_;
  return str_empty_;
}

const std::string & XmlConstants::ns_xml() {
  static const std::string ns_xml_("http://www.w3.org/XML/1998/namespace");
  return ns_xml_;
}

const std::string & XmlConstants::ns_xmlns() {
  static const std::string ns_xmlns_("http://www.w3.org/2000/xmlns/");
  return ns_xmlns_;
}

const std::string & XmlConstants::str_xmlns() {
  static const std::string str_xmlns_("xmlns");
  return str_xmlns_;
}

const std::string & XmlConstants::str_xml() {
  static const std::string str_xml_("xml");
  return str_xml_;
}

const std::string & XmlConstants::str_version() {
  static const std::string str_version_("version");
  return str_version_;
}

const std::string & XmlConstants::str_encoding() {
  static const std::string str_encoding_("encoding");
  return str_encoding_;
}

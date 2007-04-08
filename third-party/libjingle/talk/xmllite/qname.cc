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

#include <string>
#include "talk/base/common.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlconstants.h"

//#define new TRACK_NEW

namespace buzz {

static int QName_Hash(const std::string & ns, const char * local) {
  int result = ns.size() * 101;
  while (*local) {
    result *= 19;
    result += *local;
    local += 1;
  }
  return result;
}

static const int bits = 9;
static QName::Data * get_qname_table() {
  static QName::Data qname_table[1 << bits];
  return qname_table;
}

static QName::Data *
AllocateOrFind(const std::string & ns, const char * local) {
  int index = QName_Hash(ns, local);
  int increment = index >> (bits - 1) | 1;
  QName::Data * qname_table = get_qname_table();
  for (;;) {
    index &= ((1 << bits) - 1);
    if (!qname_table[index].Occupied()) {
      return new QName::Data(ns, local);
    }
    if (qname_table[index].localPart_ == local &&
        qname_table[index].namespace_ == ns) {
      qname_table[index].AddRef();
      return qname_table + index;
    }
    index += increment;
  }
}

static QName::Data *
Add(const std::string & ns, const char * local) {
  int index = QName_Hash(ns, local);
  int increment = index >> (bits - 1) | 1;
  QName::Data * qname_table = get_qname_table();
  for (;;) {
    index &= ((1 << bits) - 1);
    if (!qname_table[index].Occupied()) {
      qname_table[index].namespace_ = ns;
      qname_table[index].localPart_ = local;
      qname_table[index].AddRef(); // AddRef twice so it's never deleted
      qname_table[index].AddRef();
      return qname_table + index;
    }
    if (qname_table[index].localPart_ == local &&
        qname_table[index].namespace_ == ns) {
      qname_table[index].AddRef();
      return qname_table + index;
    }
    index += increment;
  }
}

QName::~QName() {
  data_->Release();
}

QName::QName() : data_(QN_EMPTY.data_) {
  data_->AddRef();
}

QName::QName(bool add, const std::string & ns, const char * local) :
  data_(add ? Add(ns, local) : AllocateOrFind(ns, local)) {}
  
QName::QName(bool add, const std::string & ns, const std::string & local) :
  data_(add ? Add(ns, local.c_str()) : AllocateOrFind(ns, local.c_str())) {}
  
QName::QName(const std::string & ns, const char * local) :
  data_(AllocateOrFind(ns, local)) {}

static std::string
QName_LocalPart(const std::string & name) {
  size_t i = name.rfind(':');
  if (i == std::string::npos)
    return name;
  return name.substr(i + 1);
}

static std::string
QName_Namespace(const std::string & name) {
  size_t i = name.rfind(':');
  if (i == std::string::npos)
    return STR_EMPTY;
  return name.substr(0, i);
}

QName::QName(const std::string & mergedOrLocal) :
  data_(AllocateOrFind(QName_Namespace(mergedOrLocal),
                 QName_LocalPart(mergedOrLocal).c_str())) {}

std::string
QName::Merged() const {
  if (data_->namespace_ == STR_EMPTY)
    return data_->localPart_;

  std::string result(data_->namespace_);
  result.reserve(result.length() + 1 + data_->localPart_.length());
  result += ':';
  result += data_->localPart_;
  return result;
}

bool
QName::operator==(const QName & other) const {
  return other.data_ == data_ ||
    data_->localPart_ == other.data_->localPart_ &&
    data_->namespace_ == other.data_->namespace_;
}

int
QName::Compare(const QName & other) const {
  if (data_ == other.data_)
    return 0;
  
  int result = data_->localPart_.compare(other.data_->localPart_);
  if (result)
    return result;

  return data_->namespace_.compare(other.data_->namespace_);
}

}




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
#ifndef _jid_h_
#define _jid_h_

#include <string>
#include "talk/xmllite/xmlconstants.h"

namespace buzz {

//! The Jid class encapsulates and provides parsing help for Jids
//! A Jid consists of three parts. The node, the domain and the resource.
//!
//! node@domain/resource
//!
//! The node and resource are both optional.  A valid jid is defined to have
//! a domain.  A bare jid is defined to not have a resource and a full jid
//! *does* have a resource.
class Jid {
public:
  explicit Jid();
  explicit Jid(const std::string & jid_string);
  explicit Jid(const std::string & node_name,
               const std::string & domain_name,
               const std::string & resource_name);
  explicit Jid(bool special, const std::string & special_string);
  Jid(const Jid & jid) : data_(jid.data_) {
    if (data_ != NULL) {
      data_->AddRef();
    }
  }
  Jid & operator=(const Jid & jid) {
    if (jid.data_ != NULL) {
      jid.data_->AddRef();
    }
    if (data_ != NULL) {
      data_->Release();
    }
    data_ = jid.data_;
    return *this;
  }
  ~Jid() {
    if (data_ != NULL) {
      data_->Release();
    }
  }
  

  const std::string & node() const { return !data_ ? STR_EMPTY : data_->node_name_; }
  // void set_node(const std::string & node_name);
  const std::string & domain() const { return !data_ ? STR_EMPTY : data_->domain_name_; }
  // void set_domain(const std::string & domain_name);
  const std::string & resource() const { return !data_ ? STR_EMPTY : data_->resource_name_; }
  // void set_resource(const std::string & res_name);

  std::string Str() const;
  Jid BareJid() const;

  bool IsValid() const;
  bool IsBare() const;
  bool IsFull() const;

  bool BareEquals(const Jid & other) const;

  bool operator==(const Jid & other) const;
  bool operator!=(const Jid & other) const { return !operator==(other); }

  bool operator<(const Jid & other) const { return Compare(other) < 0; };
  bool operator>(const Jid & other) const { return Compare(other) > 0; };
  
  int Compare(const Jid & other) const;


private:

  static std::string prepNode(const std::string str, 
      std::string::const_iterator start, std::string::const_iterator end, 
      bool *valid);
  static char prepNodeAscii(char ch, bool *valid);
  static std::string prepResource(const std::string str, 
      std::string::const_iterator start, std::string::const_iterator end, 
      bool *valid);
  static char prepResourceAscii(char ch, bool *valid);
  static std::string prepDomain(const std::string str, 
      std::string::const_iterator start,  std::string::const_iterator end, 
      bool *valid);
  static void prepDomain(const std::string str, 
      std::string::const_iterator start, std::string::const_iterator end, 
      std::string *buf, bool *valid);
  static void prepDomainLabel(const std::string str, 
      std::string::const_iterator start, std::string::const_iterator end, 
      std::string *buf, bool *valid);
  static char prepDomainLabelAscii(char ch, bool *valid);

  class Data {
  public:
    Data() : refcount_(1) {}
    Data(const std::string & node, const std::string &domain, const std::string & resource) :
      node_name_(node),
      domain_name_(domain),
      resource_name_(resource),
      refcount_(1) {}
    const std::string node_name_;
    const std::string domain_name_;
    const std::string resource_name_;

    void AddRef() { refcount_++; }
    void Release() { if (!--refcount_) delete this; }
  private:
    int refcount_;
  };

  Data * data_;
};

}



#endif

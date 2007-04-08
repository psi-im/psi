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

#ifndef _qname_h_
#define _qname_h_

#include <string>

namespace buzz {


class QName
{
public:
  explicit QName();
  QName(const QName & qname) : data_(qname.data_) { data_->AddRef(); }
  explicit QName(bool add, const std::string & ns, const char * local);
  explicit QName(bool add, const std::string & ns, const std::string & local);
  explicit QName(const std::string & ns, const char * local);
  explicit QName(const std::string & mergedOrLocal);
  QName & operator=(const QName & qn) {
    qn.data_->AddRef();
    data_->Release();
    data_ = qn.data_;
    return *this;
  }
  ~QName();
  
  const std::string & Namespace() const { return data_->namespace_; }
  const std::string & LocalPart() const { return data_->localPart_; }
  std::string Merged() const;
  int Compare(const QName & other) const;
  bool operator==(const QName & other) const;
  bool operator!=(const QName & other) const { return !operator==(other); }
  bool operator<(const QName & other) const { return Compare(other) < 0; }
  
  class Data {
  public:
    Data(const std::string & ns, const std::string & local) :
      refcount_(1),
      namespace_(ns),
      localPart_(local) {}

    Data() : refcount_(0) {}
      
    std::string namespace_;
    std::string localPart_;
    void AddRef() { refcount_++; }
    void Release() { if (!--refcount_) { delete this; } }
    bool Occupied() { return !!refcount_; }

  private:
    int refcount_;
  };

private:
  Data * data_;
};


}

#endif

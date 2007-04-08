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

#ifndef _XMPPPASSWORD_H_
#define _XMPPPASSWORD_H_

#include "talk/base/linked_ptr.h"
#include "talk/base/scoped_ptr.h"

namespace buzz {

class XmppPasswordImpl {
public:
  virtual ~XmppPasswordImpl() {}
  virtual size_t GetLength() const = 0;
  virtual void CopyTo(char * dest, bool nullterminate) const = 0;
  virtual std::string UrlEncode() const = 0;
  virtual XmppPasswordImpl * Copy() const = 0;
};

class EmptyXmppPasswordImpl : public XmppPasswordImpl {
public:
  virtual ~EmptyXmppPasswordImpl() {}
  virtual size_t GetLength() const { return 0; }
  virtual void CopyTo(char * dest, bool nullterminate) const {
    if (nullterminate) {
      *dest = '\0';
    }
  }
  virtual std::string UrlEncode() const { return ""; }
  virtual XmppPasswordImpl * Copy() const { return new EmptyXmppPasswordImpl(); }
};

class XmppPassword {
public:
  XmppPassword() : impl_(new EmptyXmppPasswordImpl()) {}
  size_t GetLength() const { return impl_->GetLength(); }
  void CopyTo(char * dest, bool nullterminate) const { impl_->CopyTo(dest, nullterminate); }
  XmppPassword(const XmppPassword & other) : impl_(other.impl_->Copy()) {}
  explicit XmppPassword(const XmppPasswordImpl & impl) : impl_(impl.Copy()) {}
  XmppPassword & operator=(const XmppPassword & other) {
    if (this != &other) {
      impl_.reset(other.impl_->Copy());
    }
    return *this;
  }
  void Clear() { impl_.reset(new EmptyXmppPasswordImpl()); }
  std::string UrlEncode() const { return impl_->UrlEncode(); }
  
private:
  scoped_ptr<const XmppPasswordImpl> impl_;
};


// Used for constructing strings where a password is involved and we
// need to ensure that we zero memory afterwards
class FormatXmppPassword {
public:
  FormatXmppPassword() {
    storage_ = new char[32];
    capacity_ = 32;
    length_ = 0;
    storage_[0] = 0;
  }
  
  void Append(const std::string & text) {
    Append(text.data(), text.length());
  }

  void Append(const char * data, size_t length) {
    EnsureStorage(length_ + length + 1);
    memcpy(storage_ + length_, data, length);
    length_ += length;
    storage_[length_] = '\0';
  }
  
  void Append(const XmppPassword * password) {
    size_t len = password->GetLength();
    EnsureStorage(length_ + len + 1);
    password->CopyTo(storage_ + length_, true);
    length_ += len;
  }

  size_t GetLength() {
    return length_;
  }

  const char * GetData() {
    return storage_;
  }


  // Ensures storage of at least n bytes
  void EnsureStorage(size_t n) {
    if (capacity_ >= n) {
      return;
    }

    size_t old_capacity = capacity_;
    char * old_storage = storage_;

    for (;;) {
      capacity_ *= 2;
      if (capacity_ >= n)
        break;
    }

    storage_ = new char[capacity_];

    if (old_capacity) {
      memcpy(storage_, old_storage, length_);
    
      // zero memory in a way that an optimizer won't optimize it out
      old_storage[0] = 0;
      for (size_t i = 1; i < old_capacity; i++) {
        old_storage[i] = old_storage[i - 1];
      }
      delete[] old_storage;
    }
  }  

  ~FormatXmppPassword() {
    if (capacity_) {
      storage_[0] = 0;
      for (size_t i = 1; i < capacity_; i++) {
        storage_[i] = storage_[i - 1];
      }
    }
    delete[] storage_;
  }
private:
  char * storage_;
  size_t capacity_;
  size_t length_;
};

}

#endif

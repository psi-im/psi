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

#ifndef _SESSIONID_H_
#define _SESSIONID_H_

#include "talk/base/basictypes.h"
#include <string>
#include <sstream>

namespace cricket {

// Each session is identified by a pair (from,id), where id is only
// assumed to be unique to the machine identified by from.
class SessionID {
public:
  SessionID() : id_str_("0") {
  }
  SessionID(const std::string& initiator, uint32 id)
    : initiator_(initiator) {
    set_id(id);
  }
  SessionID(const SessionID& sid) 
    : id_str_(sid.id_str_), initiator_(sid.initiator_) {
  }

  void set_id(uint32 id) {
    std::stringstream st;
    st << id;
    st >> id_str_;
  }
  const std::string id_str() const {
    return id_str_;
  }
  void set_id_str(const std::string &id_str) {
    id_str_ = id_str;
  }

  const std::string &initiator() const {
    return initiator_;
  }
  void set_initiator(const std::string &initiator) {
    initiator_ = initiator;
  }

  bool operator <(const SessionID& sid) const {
    int r = initiator_.compare(sid.initiator_);
    if (r == 0)
      r = id_str_.compare(sid.id_str_);
    return r < 0;
  }

  bool operator ==(const SessionID& sid) const {
    return (id_str_ == sid.id_str_) && (initiator_ == sid.initiator_);
  }

  SessionID& operator =(const SessionID& sid) {
    id_str_ = sid.id_str_;
    initiator_ = sid.initiator_;
    return *this;
  }

private:
  std::string id_str_;
  std::string initiator_;
};

} // namespace cricket

#endif // _SESSIONID_H_

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

#ifndef _SESSIONMESSAGE_H_
#define _SESSIONMESSAGE_H_

#include "talk/p2p/base/candidate.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/p2p/base/sessionid.h"
#include "talk/p2p/base/sessionresponsemessage.h"
#include "talk/base/basictypes.h"
#include "talk/base/scoped_ptr.h"
#include <string>
#include <vector>
#include <sstream>

namespace cricket {

class SessionMessage {
public:
  enum Type {
    TYPE_INITIATE = 0,          // Initiate message
    TYPE_ACCEPT,                // Accept message
    TYPE_MODIFY,                // Modify message
    TYPE_CANDIDATES,            // Candidates message
    TYPE_REJECT,                // Reject message
    TYPE_REDIRECT,              // Reject message
    TYPE_TERMINATE,             // Terminate message
  };

  class Cookie {
  public:
    virtual ~Cookie() {}

    // Returns a copy of this cookie.
    virtual Cookie* Copy() = 0;
  };


  virtual SessionResponseMessage *CreateResponse(
      SessionResponseMessage::Type type) const = 0;

  virtual SessionResponseMessage *CreateErrorResponse(
      SessionResponseMessage::Error error) const = 0;

  virtual SessionMessage *asClass(const std::string &) { return NULL; }
  virtual const SessionMessage *asClass(const std::string &) const { return NULL; }

  Type type() const {
    return type_;
  }
  void set_type(Type type) {
    type_ = type;
  }
  const SessionID& session_id() const {
    return id_;
  }
  SessionID& session_id() {
    return id_;
  }
  void set_session_id(const SessionID& id) {
    id_ = id;
  }
  const std::string& message_id() const {
    return message_id_;
  }
  void set_message_id(const std::string& id) {
    message_id_ = id;
  }
  const std::string &from() const {
    return from_;
  }
  void set_from(const std::string &from) {
    from_ = from;
  }
  const std::string &to() const {
    return to_;
  }
  void set_to(const std::string &to) {
    to_ = to;
  }
  const std::string &name() const {
    return name_;
  }
  void set_name(const std::string &name) {
    name_ = name;
  }
  const std::string &redirect_target() const {
    return redirect_target_;
  }
  void set_redirect_target(const std::string &redirect_target) {
    redirect_target_ = redirect_target;
  }
  Cookie *redirect_cookie() const {
    return redirect_cookie_;
  }
  void set_redirect_cookie(Cookie* redirect_cookie) {
    redirect_cookie_ = redirect_cookie;
  }
  const SessionDescription *description() const {
    return description_;
  }
  void set_description(const SessionDescription *description) {
    description_ = description;
  }
  const std::vector<Candidate> &candidates() const {
    return candidates_;
  }
  void set_candidates(const std::vector<Candidate> &candidates) {
    candidates_ = candidates;
  }


protected:
  Type type_;
  SessionID id_;
  std::string message_id_;
  std::string from_;
  std::string to_;
  std::string name_;
  const SessionDescription *description_;
  std::vector<Candidate> candidates_;
  std::string redirect_target_;
  Cookie* redirect_cookie_;

};

} // namespace cricket

#endif // _SESSIONMESSAGE_H_

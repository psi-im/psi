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

#ifndef _CANDIDATE_H_
#define _CANDIDATE_H_

#include <string>
#include <sstream>
#include "talk/base/socketaddress.h"

namespace cricket {

// Candidate for ICE based connection discovery.

class Candidate {
public:

  const std::string & name() const { return name_; }
  void set_name(const std::string & name) { name_ = name; }

  const std::string & protocol() const { return protocol_; }
  void set_protocol(const std::string & protocol) { protocol_ = protocol; }

  const SocketAddress & address() const { return address_; }
  void set_address(const SocketAddress & address) { address_ = address; }

  const float preference() const { return preference_; }
  void set_preference(const float preference) { preference_ = preference; }
  const std::string preference_str() const {
    std::ostringstream ost;
    ost << preference_;
    return ost.str();
  }
  void set_preference_str(const std::string & preference) {
    std::istringstream ist(preference);
    ist >> preference_;
  }

  const std::string & username() const { return username_; }
  void set_username(const std::string & username) { username_ = username; }

  const std::string & password() const { return password_; }
  void set_password(const std::string & password) { password_ = password; }

  const std::string & type() const { return type_; }
  void set_type(const std::string & type) { type_ = type; }

  const std::string & network_name() const { return network_name_; }
  void set_network_name(const std::string & network_name) {
    network_name_ = network_name;
  }

  // Candidates in a new generation replace those in the old generation.
  uint32 generation() const { return generation_; }
  void set_generation(uint32 generation) { generation_ = generation; }
  const std::string generation_str() const {
    std::ostringstream ost;
    ost << generation_;
    return ost.str();
  }
  void set_generation_str(const std::string& str) {
    std::istringstream ist(str);
    ist >> generation_;
  }

  // Determines whether this candidate is equivalent to the given one.
  bool IsEquivalent(const Candidate& c) const {
    // We ignore the network name, since that is just debug information, and
    // the preference, since that should be the same if the rest is (and it's
    // a float so equality checking is always worrisome).
    return (name_ == c.name_) &&
           (protocol_ == c.protocol_) &&
           (address_ == c.address_) &&
           (username_ == c.username_) &&
           (password_ == c.password_) &&
           (type_ == c.type_) &&
           (generation_ == c.generation_);
  }

private:
  std::string name_;
  std::string protocol_;
  SocketAddress address_;
  float preference_;
  std::string username_;
  std::string password_;
  std::string type_;
  std::string network_name_;
  uint32 generation_;
};

} // namespace cricket

#endif // _CANDIDATE_H_

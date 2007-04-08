/*
 * Jingle call example
 * Copyright 2004--2005, Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _STATUS_H_
#define _STATUS_H_

#include "talk/xmpp/jid.h"
#include "talk/xmpp/constants.h"

#define GOOGLE_CLIENT_NODE "http://www.google.com/xmpp/client/caps"

namespace buzz {

class Status {
public:
  Status() :
    pri_(0),
    show_(SHOW_NONE),
    available_(false),
    invisible_(false),
    e_code_(0),
    phone_capability_(false),
    know_capabilities_(false),
    is_google_client_(false),
    feedback_probation_(false) {};

  ~Status() {}

  // These are arranged in "priority order", i.e., if we see
  // two statuses at the same priority but with different Shows,
  // we will show the one with the highest show in the following
  // order.
  enum Show {
    SHOW_NONE     = 0,
    SHOW_INVISIBLE = 1,
    SHOW_OFFLINE  = 2,
    SHOW_XA       = 3,
    SHOW_AWAY     = 4,
    SHOW_DND      = 5,
    SHOW_ONLINE   = 6,
    SHOW_CHAT     = 7,
  };

  const Jid & jid() const { return jid_; }
  int priority() const { return pri_; }
  Show show() const { return show_; }
  const std::string & status() const { return status_; }
  bool available() const { return available_ ; }
  bool invisible() const { return invisible_; }
  int error_code() const { return e_code_; }
  const std::string & error_string() const { return e_str_; }
  bool know_capabilities() const { return know_capabilities_; }
  bool phone_capability() const { return phone_capability_; }
  bool is_google_client() const { return is_google_client_; }
  const std::string & version() const { return version_; }
  bool feedback_probation() const { return feedback_probation_; }
  const std::string& sent_time() const { return sent_time_; }

  void set_jid(const Jid & jid) { jid_ = jid; }
  void set_priority(int pri) { pri_ = pri; }
  void set_show(Show show) { show_ = show; }
  void set_status(const std::string & status) { status_ = status; }
  void set_available(bool a) { available_ = a; }
  void set_invisible(bool i) { invisible_ = i; }
  void set_error(int e_code, const std::string e_str)
      { e_code_ = e_code; e_str_ = e_str; }
  void set_know_capabilities(bool f) { know_capabilities_ = f; }
  void set_phone_capability(bool f) { phone_capability_ = f; }
  void set_is_google_client(bool f) { is_google_client_ = f; }
  void set_version(const std::string & v) { version_ = v; }
  void set_feedback_probation(bool f) { feedback_probation_ = f; }
  void set_sent_time(const std::string& time) { sent_time_ = time; }

  void UpdateWith(const Status & new_value) {
    if (!new_value.know_capabilities()) {
       bool k = know_capabilities();
       bool i = is_google_client();
       bool p = phone_capability();
       std::string v = version();

       *this = new_value;

       set_know_capabilities(k);
       set_is_google_client(i);
       set_phone_capability(p);
       set_version(v);
    }
    else {
      *this = new_value;
    }
  }

  bool HasQuietStatus() const {
    if (status_.empty())
      return false;
    return !(QuietStatus().empty());
  }

  // Knowledge of other clients' silly automatic status strings -
  // Don't show these.
  std::string QuietStatus() const {
    if (jid_.resource().find("Psi") != std::string::npos) {
      if (status_ == "Online" ||
          status_.find("Auto Status") != std::string::npos)
        return STR_EMPTY;
    }
    if (jid_.resource().find("Gaim") != std::string::npos) {
      if (status_ == "Sorry, I ran out for a bit!")
        return STR_EMPTY;
    }
    return TrimStatus(status_);
  }

  std::string ExplicitStatus() const {
    std::string result = QuietStatus();
    if (result.empty()) {
      result = ShowStatus();
    }
    return result;
  }

  std::string ShowStatus() const {
    std::string result;
    if (!available()) {
      result = "Offline";
    }
    else {
      switch (show()) {
        case SHOW_AWAY:
        case SHOW_XA:
          result = "Idle";
          break;
        case SHOW_DND:
          result = "Busy";
          break;
        case SHOW_CHAT:
          result = "Chatty";
          break;
        default:
          result = "Available";
          break;
      }
    }
    return result;
  }

  static std::string TrimStatus(const std::string & st) {
    std::string s(st);
    int j = 0;
    bool collapsing = true;
    for (unsigned int i = 0; i < s.length(); i+= 1) {
      if (s[i] <= ' ' && s[i] >= 0) {
        if (collapsing) {
          continue;
        }
        else {
          s[j] = ' ';
          j += 1;
          collapsing = true;
        }
      }
      else {
        s[j] = s[i];
        j += 1;
        collapsing = false;
      }
    }
    if (collapsing && j > 0) {
      j -= 1;
    }
    s.erase(j, s.length());
    return s;
  }

private:
  Jid jid_;
  int pri_;
  Show show_;
  std::string status_;
  bool available_;
  bool invisible_;
  int e_code_;
  std::string e_str_;
  bool feedback_probation_;

  // capabilities (valid only if know_capabilities_
  bool know_capabilities_;
  bool phone_capability_;
  bool is_google_client_;
  std::string version_;

  std::string sent_time_; // from the jabber:x:delay element
};

}


#endif

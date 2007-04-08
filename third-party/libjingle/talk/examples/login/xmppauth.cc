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

#include <algorithm>
#include "talk/examples/login/xmppauth.h"
#include "talk/xmpp/saslcookiemechanism.h"
#include "talk/xmpp/saslplainmechanism.h"

XmppAuth::XmppAuth() : done_(false), error_(false) {
}

XmppAuth::~XmppAuth() {
}
  
void XmppAuth::StartPreXmppAuth(const buzz::Jid & jid,
                                const cricket::SocketAddress & server,
                                const buzz::XmppPassword & pass,
                                const std::string & auth_cookie) {
  jid_ = jid;
  passwd_ = pass;
  auth_cookie_ = auth_cookie;
  error_ = auth_cookie.empty();
  done_ = true;

  SignalAuthDone();
}
  
std::string XmppAuth::ChooseBestSaslMechanism(
    const std::vector<std::string> & mechanisms,
    bool encrypted) {
  std::vector<std::string>::const_iterator it;

  // a token is the weakest auth - 15s, service-limited, so prefer it.
  it = std::find(mechanisms.begin(), mechanisms.end(), "X-GOOGLE-TOKEN");
  if (it != mechanisms.end())
    return "X-GOOGLE-TOKEN";

  // a cookie is the next weakest - 14 days
  it = std::find(mechanisms.begin(), mechanisms.end(), "X-GOOGLE-COOKIE");
  if (it != mechanisms.end())
    return "X-GOOGLE-COOKIE";

  // never pass @google.com passwords without encryption!!
  if (!encrypted && (jid_.domain() == "google.com"))
    return "";

  // as a last resort, use plain authentication
  if (jid_.domain() != "google.com") {
    it = std::find(mechanisms.begin(), mechanisms.end(), "PLAIN");
    if (it != mechanisms.end())
      return "PLAIN";
  }

  // No good mechanism found
 return "";
}

buzz::SaslMechanism* XmppAuth::CreateSaslMechanism(
    const std::string & mechanism) {
  if (mechanism == "X-GOOGLE-TOKEN") {
    return new buzz::SaslCookieMechanism(mechanism, jid_.Str(), auth_cookie_);
  //} else if (mechanism == "X-GOOGLE-COOKIE") {
  //  return new buzz::SaslCookieMechanism(mechanism, jid.Str(), sid_);
  } else if (mechanism == "PLAIN") {
    return new buzz::SaslPlainMechanism(jid_, passwd_);
  } else {
    return NULL;
  }
}

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

#ifndef _PREXMPPAUTH_H_
#define _PREXMPPAUTH_H_

#include "talk/base/sigslot.h"
#include "talk/xmpp/saslhandler.h"
#include "talk/xmpp/xmpppassword.h"

namespace cricket {
  class SocketAddress;
}

namespace buzz {

class Jid;
class SaslMechanism;

class CaptchaChallenge {
 public:
  CaptchaChallenge() : captcha_needed_(false) {}
  CaptchaChallenge(const std::string& token, const std::string& url) 
    : captcha_needed_(true), captcha_token_(token), captcha_image_url_(url) {
  }

  bool captcha_needed() const { return captcha_needed_; }
  const std::string& captcha_token() const { return captcha_token_; }

  // This url is relative to the gaia server.  Once we have better tools
  // for cracking URLs, we should probably make this a full URL
  const std::string& captcha_image_url() const { return captcha_image_url_; }

 private:
  bool captcha_needed_;
  std::string captcha_token_;
  std::string captcha_image_url_;
};

class PreXmppAuth : public SaslHandler {
public:
  virtual ~PreXmppAuth() {}
  
  virtual void StartPreXmppAuth(
    const Jid & jid,
    const cricket::SocketAddress & server,
    const XmppPassword & pass,
    const std::string & auth_cookie) = 0;
  
  sigslot::signal0<> SignalAuthDone;
  
  virtual bool IsAuthDone() = 0;
  virtual bool IsAuthorized() = 0;
  virtual bool HadError() = 0;
  virtual CaptchaChallenge GetCaptchaChallenge() = 0;
  virtual std::string GetAuthCookie() = 0;
};

}

#endif

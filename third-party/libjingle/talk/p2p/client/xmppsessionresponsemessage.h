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

#ifndef _XMPPSESSIONRESPONSE_H_
#define _XMPPSESSIONRESPONSE_H_

#include "talk/base/scoped_ptr.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/p2p/base/sessionresponsemessage.h"
#include "talk/p2p/client/sessionclient.h"

namespace buzz {
class XmlElement;
}

namespace cricket {

/**
 * Instances of this class can be created either by XmppSessionMessage
 * (to construct a response to a message we received) or by
 * SessionClient::OnIncomingResponse.
 */
class XmppSessionResponseMessage : public SessionResponseMessage {
 public:
  XmppSessionResponseMessage() : SessionResponseMessage() {}

  virtual SessionResponseMessage *asClass(const std::string &s) {
    return (s == SessionClient::XMPP_CLIENT_CLASS_NAME) ? this : NULL;
  }

  virtual const SessionResponseMessage *asClass(const std::string &s) const {
    return (s == SessionClient::XMPP_CLIENT_CLASS_NAME) ? this : NULL;
  }

  void set_original_stanza(const buzz::XmlElement *stanza) {
    original_stanza_copy_.reset(new buzz::XmlElement(*stanza));
  }

  const buzz::XmlElement *original_stanza() const {
    return original_stanza_copy_.get();
  }

 private:
  std::string iqId_;
  buzz::scoped_ptr<const buzz::XmlElement> original_stanza_copy_;
};

}

#endif

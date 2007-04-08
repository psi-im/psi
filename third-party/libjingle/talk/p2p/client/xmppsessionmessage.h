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

#ifndef _XMPPSESSIONMESSAGE_H_
#define _XMPPSESSIONMESSAGE_H_

#include "talk/p2p/base/sessionmessage.h"
#include "talk/p2p/client/sessionclient.h"
#include "talk/p2p/client/xmppsessionresponsemessage.h"

namespace cricket {

class XmppSessionMessage : public SessionMessage {
 public:
  virtual SessionResponseMessage *CreateResponse(
      SessionResponseMessage::Type type) const {
    XmppSessionResponseMessage *response = new XmppSessionResponseMessage();
    response->set_type(type);
    response->set_original_stanza(stanza_);
    response->set_message_id(message_id_);
    return response;
  }

  virtual SessionResponseMessage *CreateErrorResponse(
      SessionResponseMessage::Error error) const {
    XmppSessionResponseMessage *response = new XmppSessionResponseMessage();
    response->set_type(SessionResponseMessage::TYPE_ERROR);
    response->set_original_stanza(stanza_);
    response->set_message_id(message_id_);
    response->set_error(error);
    return response;
  }

  virtual SessionMessage *asClass(const std::string &s) {
    return (s == SessionClient::XMPP_CLIENT_CLASS_NAME) ? this : NULL;
  }

  virtual const SessionMessage *asClass(const std::string &s) const {
    return (s == SessionClient::XMPP_CLIENT_CLASS_NAME) ? this : NULL;
  }

  const buzz::XmlElement *stanza() { return stanza_; }
  void set_stanza(const buzz::XmlElement *stanza) { stanza_ = stanza; }

 private:
  const buzz::XmlElement *stanza_;
};

}

#endif

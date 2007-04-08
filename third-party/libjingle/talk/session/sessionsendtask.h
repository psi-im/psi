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

#ifndef _CRICKET_PHONE_SESSIONSENDTASK_H_
#define _CRICKET_PHONE_SESSIONSENDTASK_H_

#include "talk/xmpp/xmppengine.h"
#include "talk/xmpp/xmpptask.h"
#include "talk/p2p/client/sessionclient.h"

namespace cricket {

// The job of this task is to send an IQ stanza out (after stamping it with
// an ID attribute) and then wait for a response.  If not response happens
// within 5 seconds, it will signal failure on a SessionClient.  If an error
// happens it will also signal failure.  If, however, the send succeeds this
// task will quietly go away.

// It is safe for this to hold on to the session client.  In the case where
// the xmpp client goes away, this task will automatically be aborted.  The
// session_client is guaranteed to outlive the xmpp session.
class SessionSendTask : public buzz::XmppTask {
public:
  SessionSendTask(Task *parent, SessionClient *session_client)
    : buzz::XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
      session_client_(session_client) {
    set_timeout_seconds(15);
  }

  void Send(const buzz::XmlElement* stanza) {
    assert(stanza_.get() == NULL);
    stanza_.reset(new buzz::XmlElement(*stanza));
    if (stanza_->HasAttr(buzz::QN_ID)) {
      set_task_id(stanza_->Attr(buzz::QN_ID));
    } else {
      stanza_->SetAttr(buzz::QN_ID, task_id());
    }
  }

protected:
  virtual int OnTimeout() {
    session_client_->OnFailedSend(stanza_.get(), NULL);

    return XmppTask::OnTimeout();
  }

  virtual int ProcessStart() {
    SendStanza(stanza_.get());
    return STATE_RESPONSE;
  }

  virtual int ProcessResponse() {
    const buzz::XmlElement* next = NextStanza();
    if (next == NULL)
      return STATE_BLOCKED;

    if (next->Attr(buzz::QN_TYPE) == buzz::STR_RESULT) {
      session_client_->OnIncomingResponse(stanza_.get(), next);
      return STATE_DONE;
    } else {
      session_client_->OnFailedSend(stanza_.get(), next);
      return STATE_DONE;
    }
  }

  virtual bool HandleStanza(const buzz::XmlElement *stanza) {
    if (!MatchResponseIq(stanza,
                         buzz::Jid(stanza_->Attr(buzz::QN_TO)), task_id()))
      return false;
    if (stanza->Attr(buzz::QN_TYPE) == buzz::STR_RESULT ||
        stanza->Attr(buzz::QN_TYPE) == buzz::STR_ERROR) {
      QueueStanza(stanza);
      return true;
    }
    return false;
  }

private:
  SessionClient *session_client_;
  buzz::scoped_ptr<buzz::XmlElement> stanza_;
};

}

#endif // _CRICKET_PHONE_SESSIONSENDTASK_H_

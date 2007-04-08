/*
 * libjingle
 * Copyright 2004--2006, Google Inc.
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

#ifndef _XMPPTASK_H_
#define _XMPPTASK_H_

#include <string>
#include <deque>
#include "talk/base/sigslot.h"
#include "talk/xmpp/xmppengine.h"
#include "talk/base/task.h"

namespace buzz {

/////////////////////////////////////////////////////////////////////
//
// XMPPTASK
//
/////////////////////////////////////////////////////////////////////
//
// See Task and XmppClient first.
//
// XmppTask is a task that is designed to go underneath XmppClient and be
// useful there.  It has a way of finding its XmppClient parent so you
// can have it nested arbitrarily deep under an XmppClient and it can
// still find the XMPP services.
//
// Tasks register themselves to listen to particular kinds of stanzas
// that are sent out by the client.  Rather than processing stanzas
// right away, they should decide if they own the sent stanza,
// and if so, queue it and Wake() the task, or if a stanza does not belong
// to you, return false right away so the next XmppTask can take a crack.
// This technique (synchronous recognize, but asynchronous processing)
// allows you to have arbitrary logic for recognizing stanzas yet still,
// for example, disconnect a client while processing a stanza -
// without reentrancy problems.
//
/////////////////////////////////////////////////////////////////////

class XmppClient;

class XmppTask :
  public Task,
  public XmppStanzaHandler,
  public sigslot::has_slots<>
{
 public:
  XmppTask(Task * parent, XmppEngine::HandlerLevel level = XmppEngine::HL_NONE);
  virtual ~XmppTask();

  virtual XmppClient * GetClient() const { return client_; }
  std::string task_id() const { return id_; }
  void set_task_id(std::string id) { id_ = id; }

#ifdef _DEBUG
  void set_debug_force_timeout(const bool f) { debug_force_timeout_ = f; }
#endif

 protected:
  friend class XmppClient;

  XmppReturnStatus SendStanza(const XmlElement * stanza);
  XmppReturnStatus SetResult(const std::string & code);
  XmppReturnStatus SendStanzaError(const XmlElement * element_original,
                                XmppStanzaError code,
                                const std::string & text);

  virtual void Stop();
  virtual bool HandleStanza(const XmlElement * stanza) { return false; }
  virtual void OnDisconnect();
  virtual int ProcessReponse() { return STATE_DONE; }

  virtual void QueueStanza(const XmlElement * stanza);
  const XmlElement * NextStanza();

  bool MatchResponseIq(const XmlElement * stanza, const Jid & to, const std::string & task_id);
  bool MatchResponseIqFromArbitrary(const XmlElement * stanza,
                                    const Jid & to,
                                    const std::string & task_id);

  bool MatchRequestIq(const XmlElement * stanza, const std::string & type, const QName & qn);
  XmlElement *MakeIqResult(const XmlElement * query);
  XmlElement *MakeIq(const std::string & type,
    const Jid & to, const std::string task_id);

private:
  void StopImpl();

  XmppClient * client_;
  std::deque<XmlElement *> stanza_queue_;
  scoped_ptr<XmlElement> next_stanza_;
  std::string id_;

#ifdef _DEBUG
  bool debug_force_timeout_;
#endif
};

}

#endif

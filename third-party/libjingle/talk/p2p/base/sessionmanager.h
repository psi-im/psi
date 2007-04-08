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

#ifndef _SESSIONMANAGER_H_
#define _SESSIONMANAGER_H_

#include "talk/base/thread.h"
#include "talk/p2p/base/portallocator.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/base/sessionmessage.h"
#include "talk/base/sigslot.h"

#include <string>
#include <utility>
#include <map>

namespace cricket {

class Session;

// SessionManager manages session instances

class SessionManager : public sigslot::has_slots<> {
public:
  SessionManager(PortAllocator *allocator, Thread *worker_thread = NULL);
  virtual ~SessionManager();

  Session *CreateSession(const std::string &name, const std::string& initiator);
  void DestroySession(Session *session);
  Session *GetSession(const SessionID& id);
  void TerminateAll();
  void OnIncomingMessage(const SessionMessage &m);
  void OnIncomingError(const SessionMessage &m);
  void OnSignalingReady();

  PortAllocator *port_allocator() const;
  Thread *worker_thread() const;
  Thread *signaling_thread() const;
  int session_timeout();
  void set_session_timeout(int timeout);

  sigslot::signal2<Session *, bool> SignalSessionCreate;
  sigslot::signal1<Session *> SignalSessionDestroy;

  // Note: you can connect this directly to OnSignalingReady(), if a signalling
  // check is not required.
  sigslot::signal0<> SignalRequestSignaling;
  sigslot::signal2<Session *,
                   const SessionResponseMessage *> SignalSendResponse;

private:
  Session *CreateSession(const std::string &name, const SessionID& id,
                         bool received_initiate);
  void OnRequestSignaling();

  int timeout_;
  Thread *worker_thread_;
  Thread *signaling_thread_;
  PortAllocator *allocator_;
  std::map<SessionID, Session *> session_map_;
};

} // namespace cricket

#endif // _SESSIONMANAGER_H_

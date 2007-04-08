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

#include "talk/base/common.h"
#include "talk/p2p/base/helpers.h"
#include "sessionmanager.h"

namespace cricket {

SessionManager::SessionManager(PortAllocator *allocator, Thread *worker) {
  allocator_ = allocator;
  signaling_thread_ = Thread::Current();
  if (worker == NULL) {
    worker_thread_ = Thread::Current();
  } else {
    worker_thread_ = worker;
  }
  timeout_ = 50;
}

SessionManager::~SessionManager() {
  // Note: Session::Terminate occurs asynchronously, so it's too late to
  // delete them now.  They better be all gone.
  ASSERT(session_map_.empty());
  //TerminateAll();
}

Session *SessionManager::CreateSession(const std::string &name, const std::string& initiator) {
  return CreateSession(name, SessionID(initiator, CreateRandomId()), false);
}

Session *SessionManager::CreateSession(const std::string &name, const SessionID& id, bool received_initiate) {
  Session *session = new Session(this, name, id);
  session_map_[session->id()] = session;
  session->SignalRequestSignaling.connect(this, &SessionManager::OnRequestSignaling);
  SignalSessionCreate(session, received_initiate);
  return session;
}

void SessionManager::DestroySession(Session *session) {
  if (session != NULL) {
    std::map<SessionID, Session *>::iterator it = session_map_.find(session->id());
    if (it != session_map_.end()) {
      SignalSessionDestroy(session);
      session_map_.erase(it);
      delete session;
    }
  }
}

Session *SessionManager::GetSession(const SessionID& id) {
  // If the id isn't present, the [] operator will make a NULL entry
  std::map<SessionID, Session *>::iterator it = session_map_.find(id);
  if (it != session_map_.end())
    return (*it).second;
  return NULL;
}

void SessionManager::TerminateAll() {
  while (session_map_.begin() != session_map_.end()) {
    Session *session = session_map_.begin()->second;
    session->Terminate();
  }
}

void SessionManager::OnIncomingError(const SessionMessage &m) {
  // Incoming signaling error. This means, as the result of trying
  // to send message m, and error was generated. In all cases, a
  // session should already exist

  Session *session = NULL;
  switch (m.type()) {
  case SessionMessage::TYPE_INITIATE:
  case SessionMessage::TYPE_ACCEPT:
  case SessionMessage::TYPE_MODIFY:
  case SessionMessage::TYPE_CANDIDATES:
  case SessionMessage::TYPE_REJECT:
  case SessionMessage::TYPE_TERMINATE:
    session = GetSession(m.session_id());
    break;

  default:
    return;
  }

  if (session != NULL)
    session->OnIncomingError(m);

}

void SessionManager::OnIncomingMessage(const SessionMessage &m) {
  // In the case of an incoming initiate, there is no session yet, and one needs to be created.
  // The other cases have sessions already.

  Session *session;
  switch (m.type()) {
  case SessionMessage::TYPE_INITIATE:
    session = CreateSession(m.name(), m.session_id(), true);
    break;

  case SessionMessage::TYPE_ACCEPT:
  case SessionMessage::TYPE_MODIFY:
  case SessionMessage::TYPE_CANDIDATES:
  case SessionMessage::TYPE_REJECT:
  case SessionMessage::TYPE_REDIRECT:
  case SessionMessage::TYPE_TERMINATE:
    session = GetSession(m.session_id());
    break;

  default:
    SessionResponseMessage *response =
      m.CreateErrorResponse(SessionResponseMessage::ERROR_UNKNOWN_METHOD);
    SignalSendResponse(NULL, response);
    delete response;
    return;
  }

  if (session == NULL) {
    SessionResponseMessage *response =
      m.CreateErrorResponse(SessionResponseMessage::ERROR_UNKNOWN_SESSION);
    SignalSendResponse(NULL, response);
    delete response;
  } else {
    session->OnIncomingMessage(m);
  }
}

void SessionManager::OnSignalingReady() {
  for (std::map<SessionID, Session *>::iterator it = session_map_.begin();
      it != session_map_.end(); ++it) {
    it->second->OnSignalingReady();
  }
}

void SessionManager::OnRequestSignaling() {
  SignalRequestSignaling();
}

PortAllocator *SessionManager::port_allocator() const {
  return allocator_;
}

Thread *SessionManager::worker_thread() const {
  return worker_thread_;
}

Thread *SessionManager::signaling_thread() const {
  return signaling_thread_;
}

int SessionManager::session_timeout() {
  return timeout_;
}

void SessionManager::set_session_timeout(int timeout) {
  timeout_ = timeout;
}

} // namespace cricket

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
#include "talk/base/logging.h"
#include "talk/p2p/base/helpers.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/base/sessionmessage.h"
#include "talk/p2p/base/sessionmessagefactory.h"

namespace cricket {

const uint32 MSG_TIMEOUT = 1;
const uint32 MSG_ERROR = 2;
const uint32 MSG_STATE = 3;

Session::Session(SessionManager *session_manager, const std::string &name,
    const SessionID& id) {
  session_manager_ = session_manager;
  ASSERT(session_manager_->signaling_thread()->IsCurrent());
  name_ = name;
  id_ = id;
  error_ = ERROR_NONE;
  state_ = STATE_INIT;
  initiator_ = false;
  description_ = NULL;
  remote_description_ = NULL;
  socket_manager_ = new SocketManager(session_manager_);
  socket_manager_->SignalCandidatesReady.connect(this, &Session::OnCandidatesReady);
  socket_manager_->SignalNetworkError.connect(this, &Session::OnNetworkError);
  socket_manager_->SignalState.connect(this, &Session::OnSocketState);
  socket_manager_->SignalRequestSignaling.connect(this, &Session::OnRequestSignaling);
}

Session::~Session() {
  ASSERT(session_manager_->signaling_thread()->IsCurrent());
  delete description_;
  delete remote_description_;
  delete socket_manager_;
  session_manager_->signaling_thread()->Clear(this);
}

P2PSocket *Session::CreateSocket(const std::string &name) {
  return socket_manager_->CreateSocket(name);
}

void Session::DestroySocket(P2PSocket *socket) {
  socket_manager_->DestroySocket(socket);
}

void Session::OnCandidatesReady(const std::vector<Candidate>& candidates) {
  SendSessionMessage(SessionMessage::TYPE_CANDIDATES, NULL, &candidates, NULL);
}

void Session::OnNetworkError() {
  // Socket manager is experiencing a network error trying to allocate
  // network resources (usually port allocation)

  set_error(ERROR_NETWORK);
}

void Session::OnSocketState() {
  // If the call is not in progress, then we don't care about writability.
  // We have separate timers for making sure we transition back to the in-
  // progress state in time.
  if (state_ != STATE_INPROGRESS)
    return;

  // Put the timer into the write state.  This is called when the state changes,
  // so we will restart the timer each time we lose writability.
  if (socket_manager_->writable()) {
    session_manager_->signaling_thread()->Clear(this, MSG_TIMEOUT);
  } else {
    session_manager_->signaling_thread()->PostDelayed(
        session_manager_->session_timeout() * 1000, this, MSG_TIMEOUT);
  }
}

void Session::OnRequestSignaling() {
  SignalRequestSignaling();
}

void Session::OnSignalingReady() {
  socket_manager_->OnSignalingReady();
}

std::string Session::SendSessionMessage(
    SessionMessage::Type type,
    const SessionDescription* description,
    const std::vector<Candidate>* candidates,
    SessionMessage::Cookie* redirect_cookie) {
  assert(session_message_factory_ != NULL);
  SessionMessage *m = session_message_factory_->CreateSessionMessage();
  m->set_type(type);
  m->set_to(remote_address_);
  m->set_name(name_);
  m->set_description(description);
  m->set_session_id(id_);
  if (candidates)
    m->set_candidates(*candidates);
  m->set_redirect_target(redirect_target_);
  m->set_redirect_cookie(redirect_cookie);
  SignalOutgoingMessage(this, *m);

  std::string message_id = m->message_id();
  delete m;
  return message_id;
}

void Session::SendAcknowledgementMessage(const SessionMessage *message) {
  SessionResponseMessage *m =
    message->CreateResponse(SessionResponseMessage::TYPE_OK);
  SignalSendResponse(this, m);
  delete m;
}

void Session::SendInitiateResponseMessage(const SessionMessage *message) {
  SessionResponseMessage *m =
    message->CreateResponse(SessionResponseMessage::TYPE_INITIATERESPONSE);
  SignalSendResponse(this, m);
  delete m;
}

bool Session::Initiate(const std::string &to, const SessionDescription *description) {
  ASSERT(session_manager_->signaling_thread()->IsCurrent());

  // Only from STATE_INIT
  if (state_ != STATE_INIT)
    return false;

  // Setup for signaling. Initiate is asynchronous. It occurs once the address
  // candidates are ready.
  initiator_ = true;
  remote_address_ = to;
  description_ = description;
  initiate_msgid_ = SendSessionMessage(SessionMessage::TYPE_INITIATE, description, NULL, NULL);
  set_state(Session::STATE_SENTINITIATE);

  // Let the socket manager know we now want the candidates.
  socket_manager_->StartProcessingCandidates();

  // Start the session timeout
  session_manager_->signaling_thread()->Clear(this, MSG_TIMEOUT);
  session_manager_->signaling_thread()->PostDelayed(session_manager_->session_timeout() * 1000, this, MSG_TIMEOUT);
  return true;
}

bool Session::Accept(const SessionDescription *description) {
  ASSERT(session_manager_->signaling_thread()->IsCurrent());

  // Only if just received initiate
  if (state_ != STATE_RECEIVEDINITIATE)
    return false;

  // Setup for signaling. Accept is asynchronous. It occurs once the address
  // candidates are ready.
  initiator_ = false;
  description_ = description;
  SendSessionMessage(SessionMessage::TYPE_ACCEPT, description, NULL, NULL);
  set_state(Session::STATE_SENTACCEPT);

  return true;
}

bool Session::Modify(const SessionDescription *description) {
  ASSERT(session_manager_->signaling_thread()->IsCurrent());

  // Only if session already STATE_INPROGRESS
  if (state_ != STATE_INPROGRESS)
    return false;

  // Modify is asynchronous. It occurs once the address candidates are ready.
  // Either side can send a modify. It is only valid in an already accepted
  // session.
  description_ = description;
  SendSessionMessage(SessionMessage::TYPE_MODIFY, description, NULL, NULL);
  set_state(Session::STATE_SENTMODIFY);

  // Start the session timeout
  session_manager_->signaling_thread()->Clear(this, MSG_TIMEOUT);
  session_manager_->signaling_thread()->PostDelayed(session_manager_->session_timeout() * 1000, this, MSG_TIMEOUT);
  return true;
}

bool Session::Redirect(const std::string& target) {
  ASSERT(session_manager_->signaling_thread()->IsCurrent());

  // Redirect is sent in response to an initiate or modify, to redirect the
  // request
  if (state_ != STATE_RECEIVEDINITIATE)
    return false;

  initiator_ = false;
  redirect_target_ = target;
  SendSessionMessage(SessionMessage::TYPE_REDIRECT, NULL, NULL, NULL);

  // A redirect puts us in the same state as reject.  It just sends a different
  // kind of reject message, if you like.
  set_state(STATE_SENTREDIRECT);

  return true;
}

bool Session::Reject() {
  ASSERT(session_manager_->signaling_thread()->IsCurrent());

  // Reject is sent in response to an initiate or modify, to reject the
  // request
  if (state_ != STATE_RECEIVEDINITIATE && state_ != STATE_RECEIVEDMODIFY)
    return false;

  initiator_ = false;
  SendSessionMessage(SessionMessage::TYPE_REJECT, NULL, NULL, NULL);
  set_state(STATE_SENTREJECT);

  return true;
}

bool Session::Terminate() {
  ASSERT(session_manager_->signaling_thread()->IsCurrent());

  // Either side can terminate, at any time.
  if (state_ == STATE_SENTTERMINATE && state_ != STATE_RECEIVEDTERMINATE)
    return false;

  // But we don't need to terminate if we already rejected.  The other client
  // already knows that we're done with this session.
  if (state_ != STATE_SENTREDIRECT)
    SendSessionMessage(SessionMessage::TYPE_TERMINATE, NULL, NULL, NULL);

  set_state(STATE_SENTTERMINATE);

  return true;
}

void Session::OnIncomingError(const SessionMessage &m) {
  ASSERT(session_manager_->signaling_thread()->IsCurrent());

  // If a candidate message errors out or gets dropped for some reason we
  // ignore the error.
  if (m.type() != SessionMessage::TYPE_CANDIDATES) {
    set_error(ERROR_RESPONSE);
  }
}

void Session::OnIncomingMessage(const SessionMessage &m) {
  ASSERT(session_manager_->signaling_thread()->IsCurrent());

  switch (m.type()) {
  case SessionMessage::TYPE_INITIATE:
    remote_description_ = m.description();
    remote_address_ = m.from();
    name_ = m.name();
    initiator_ = false;
    set_state(STATE_RECEIVEDINITIATE);
    SendInitiateResponseMessage(&m);

    // Let the socket manager know we now want the initial candidates
    socket_manager_->StartProcessingCandidates();
    break;

  case SessionMessage::TYPE_ACCEPT:
    remote_description_ = m.description();
    SendAcknowledgementMessage(&m);
    set_state(STATE_RECEIVEDACCEPT);
    break;

  case SessionMessage::TYPE_MODIFY:
    remote_description_ = m.description();
    SendAcknowledgementMessage(&m);
    set_state(STATE_RECEIVEDMODIFY);
    break;

  case SessionMessage::TYPE_CANDIDATES:
    socket_manager_->AddRemoteCandidates(m.candidates());
    SendAcknowledgementMessage(&m);
    break;

  case SessionMessage::TYPE_REJECT:
    SendAcknowledgementMessage(&m);
    set_state(STATE_RECEIVEDREJECT);
    break;

  case SessionMessage::TYPE_REDIRECT:
    OnRedirectMessage(m);
    SendAcknowledgementMessage(&m);
    break;

  case SessionMessage::TYPE_TERMINATE:
    SendAcknowledgementMessage(&m);
    set_state(STATE_RECEIVEDTERMINATE);
    break;
  }
}

void Session::OnRedirectMessage(const SessionMessage &m) {
  ASSERT(state_ == STATE_SENTINITIATE);
  if (state_ != STATE_SENTINITIATE)
    return;

  ASSERT(m.redirect_target().size() != 0);
  remote_address_ = m.redirect_target();

  SendSessionMessage(SessionMessage::TYPE_INITIATE, description_, NULL,
                     m.redirect_cookie()->Copy());

  // Restart the session timeout.
  session_manager_->signaling_thread()->Clear(this, MSG_TIMEOUT);
  session_manager_->signaling_thread()->PostDelayed(session_manager_->session_timeout() * 1000, this, MSG_TIMEOUT);

  // Reset all of the sockets back into the initial state.
  socket_manager_->ResetSockets();
}

void Session::OnIncomingResponse(const SessionResponseMessage &m) {
  if ((state_ == STATE_SENTINITIATE || state_ == STATE_RECEIVEDACCEPT) &&
      m.message_id() == initiate_msgid_) {
    //shhh TODO: Handle any response from the initiate as necessary
    initiate_msgid_.clear();
  }
}

Session::State Session::state() {
  return state_;
}

void Session::set_state(State state) {
  ASSERT(session_manager_->signaling_thread()->IsCurrent());
  if (state != state_) {
    state_ = state;
    SignalState(this, state);
    session_manager_->signaling_thread()->Post(this, MSG_STATE);
  }
}

Session::Error Session::error() {
  return error_;
}

void Session::set_error(Error error) {
  ASSERT(session_manager_->signaling_thread()->IsCurrent());
  if (error != error_) {
    error_ = error;
    SignalError(this, error);
    session_manager_->signaling_thread()->Post(this, MSG_ERROR);
  }
}

const std::string &Session::name() {
  return name_;
}

const std::string &Session::remote_address() {
  return remote_address_;
}

bool Session::initiator() {
  return initiator_;
}

const SessionID& Session::id() {
  return id_;
}

const SessionDescription *Session::description() {
  return description_;
}

const SessionDescription *Session::remote_description() {
  return remote_description_;
}

SessionManager *Session::session_manager() {
  return session_manager_;
}

void Session::set_session_message_factory(SessionMessageFactory *factory) {
  session_message_factory_ = factory;
}

void Session::OnMessage(Message *pmsg) {
  switch(pmsg->message_id) {
  case MSG_TIMEOUT:
    // Session timeout has occured. Check to see if the session is still trying
    // to signal. If so, the session has timed out.
    // The Sockets have their own timeout for connectivity.
    set_error(ERROR_TIME);
    break;

  case MSG_ERROR:
    switch (error_) {
    case ERROR_RESPONSE:
      // This state could be reached if we get an error in response to an IQ
      // or if the network is so slow we time out on an individual IQ exchange.
      // In either case, Terminate (send more messages) and ignore the likely
      // cascade of more errors.

      // fall through
    case ERROR_NETWORK:
    case ERROR_TIME:
      // Time ran out - no response
      Terminate();
      break;

    default:
      break;
    }
    break;

  case MSG_STATE:
    switch (state_) {
    case STATE_SENTACCEPT:
    case STATE_RECEIVEDACCEPT:
      set_state(STATE_INPROGRESS);
      session_manager_->signaling_thread()->Clear(this, MSG_TIMEOUT);
      OnSocketState(); // Update the writability timeout state.
      break;

    case STATE_SENTREJECT:
    case STATE_SENTREDIRECT:
    case STATE_RECEIVEDREJECT:
      Terminate();
      break;

    case STATE_SENTTERMINATE:
    case STATE_RECEIVEDTERMINATE:
      session_manager_->DestroySession(this);
      break;

    default:
      // explicitly ignoring some states here
      break;
    }
    break;
  }
}

} // namespace cricket

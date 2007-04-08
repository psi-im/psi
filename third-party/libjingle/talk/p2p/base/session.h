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

#ifndef _SESSION_H_
#define _SESSION_H_

#include "talk/base/socketaddress.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/p2p/base/sessionmanager.h"
#include "talk/p2p/base/sessionmessage.h"
#include "talk/p2p/base/socketmanager.h"
#include "talk/p2p/base/p2psocket.h"
#include "talk/p2p/base/port.h"
#include <string>

namespace cricket {

class SessionManager;
class SessionMessageFactory;
class SocketManager;

// A specific Session created by the SessionManager
// A Session manages signaling for session setup and tear down, and connectivity
// with P2PSockets

class Session : public MessageHandler, public sigslot::has_slots<> {
public:
  enum State {
    STATE_INIT = 0,
    STATE_SENTINITIATE, // sent initiate, waiting for Accept or Reject
    STATE_RECEIVEDINITIATE, // received an initiate. Call Accept or Reject
    STATE_SENTACCEPT, // sent accept. begin connectivity establishment
    STATE_RECEIVEDACCEPT, // received accept. begin connectivity establishment
    STATE_SENTMODIFY, // sent modify, waiting for Accept or Reject
    STATE_RECEIVEDMODIFY, // received modify, call Accept or Reject
    STATE_SENTREJECT, // sent reject after receiving initiate
    STATE_RECEIVEDREJECT, // received reject after sending initiate
    STATE_SENTREDIRECT, // sent direct after receiving initiate
    STATE_SENTTERMINATE, // sent terminate (any time / either side)
    STATE_RECEIVEDTERMINATE, // received terminate (any time / either side)
    STATE_INPROGRESS, // session accepted and in progress
  };

  enum Error {
    ERROR_NONE = 0, // no error
    ERROR_TIME, // no response to signaling
    ERROR_RESPONSE, // error during signaling
    ERROR_NETWORK, // network error, could not allocate network resources
  };

  Session(SessionManager *session_manager, const std::string &name,
          const SessionID& id);
  ~Session();

  // From MessageHandler
  void OnMessage(Message *pmsg);

  P2PSocket *CreateSocket(const std::string & name);
  void DestroySocket(P2PSocket *socket);

  bool Initiate(const std::string &to, const SessionDescription *description);
  bool Accept(const SessionDescription *description);
  bool Modify(const SessionDescription *description);
  bool Reject();
  bool Redirect(const std::string& target);
  bool Terminate();

  SessionManager *session_manager();
  void set_session_message_factory(SessionMessageFactory *factory);
  const std::string &name();
  const std::string &remote_address();
  bool initiator();
  const SessionID& id();
  const SessionDescription *description();
  const SessionDescription *remote_description();

  State state();
  Error error();

  void OnSignalingReady();
  void OnIncomingMessage(const SessionMessage &m);
  void OnIncomingError(const SessionMessage &m);
  void OnIncomingResponse(const SessionResponseMessage &m);

  sigslot::signal2<Session *, State> SignalState;
  sigslot::signal2<Session *, Error> SignalError;
  sigslot::signal2<Session *, const SessionMessage &> SignalOutgoingMessage;
  sigslot::signal2<Session *,
                   const SessionResponseMessage *> SignalSendResponse;
  sigslot::signal0<> SignalRequestSignaling;

private:
  std::string SendSessionMessage(SessionMessage::Type type,
                                 const SessionDescription* description,
                                 const std::vector<Candidate>* candidates,
                                 SessionMessage::Cookie* redirect_cookie);
  void SendAcknowledgementMessage(const SessionMessage *message);

  void SendInitiateResponseMessage(const SessionMessage *message);
  void OnCandidatesReady(const std::vector<Candidate>& candidates);
  void OnNetworkError();
  void OnSocketState();
  void OnRequestSignaling();
  void OnRedirectMessage(const SessionMessage &m);

  void set_state(State state);
  void set_error(Error error);

  bool initiator_;
  SessionManager *session_manager_;
  SessionMessageFactory *session_message_factory_;
  SessionID id_;
  SocketManager *socket_manager_;
  std::string name_;
  std::string remote_address_;
  const SessionDescription *description_;
  const SessionDescription *remote_description_;
  std::string redirect_target_;
  State state_;
  Error error_;
  CriticalSection crit_;
  std::string initiate_msgid_;
};

} // namespace cricket

#endif // _SESSION_H_

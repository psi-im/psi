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

#ifndef _SOCKETMANAGER_H_
#define _SOCKETMANAGER_H_

#include "talk/base/criticalsection.h"
#include "talk/base/messagequeue.h"
#include "talk/base/sigslot.h"
#include "talk/p2p/base/sessionmanager.h"
#include "talk/p2p/base/candidate.h"
#include "talk/p2p/base/p2psocket.h"
#include "talk/p2p/base/socketmanager.h"

#include <string>
#include <vector>

namespace cricket {

class SessionManager;

// Manages P2PSocket creation/destruction/readiness.
// Provides thread separation between session and sockets.
// This allows session to execute on the signaling thread,
// and sockets to execute on the worker thread, if desired,
// which is good for some media types (audio/video for example).

class SocketManager : public MessageHandler, public sigslot::has_slots<> {
public:
  SocketManager(SessionManager *session_manager);
  virtual ~SocketManager();

  // Determines whether any of the created sockets are currently writable.
  bool writable() { return writable_; }

  P2PSocket *CreateSocket(const std::string & name);
  void DestroySocket(P2PSocket *socket);

  // Start discovering local candidates
  void StartProcessingCandidates();
  
  // Adds the given candidates that were sent by the remote side.
  void AddRemoteCandidates(const std::vector<Candidate>& candidates);

  // signaling channel is up, ready to transmit candidates as they are discovered
  void OnSignalingReady();

  // Put all of the sockets back into the initial state.
  void ResetSockets();
  
  sigslot::signal1<const std::vector<Candidate>&> SignalCandidatesReady;
  sigslot::signal0<> SignalNetworkError;
  sigslot::signal0<> SignalState;
  sigslot::signal0<> SignalRequestSignaling;

private:
  P2PSocket *CreateSocket_w(const std::string &name);
  void DestroySocket_w(P2PSocket *socket);
  void OnSignalingReady_w();
  void AddRemoteCandidates_w(const std::vector<Candidate> &candidates);
  virtual void OnMessage(Message *message);
  void OnCandidatesReady(P2PSocket *socket, const std::vector<Candidate>&);
  void OnSocketState(P2PSocket* socket, P2PSocket::State state);
  void OnRequestSignaling(void);
  void ResetSockets_w();

  SessionManager *session_manager_;
  std::vector<Candidate> candidates_;
  CriticalSection critSM_;
  std::vector<P2PSocket *> sockets_;
  bool candidates_requested_;
  bool writable_;
};

}

#endif // _SOCKETMANAGER_H_

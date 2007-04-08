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

#if defined(_MSC_VER) && _MSC_VER < 1300
#pragma warning(disable:4786)
#endif
#include "socketmanager.h"
#include <cassert>

namespace cricket {

const uint32 MSG_CREATESOCKET = 1;
const uint32 MSG_DESTROYSOCKET = 2;
const uint32 MSG_ONSIGNALINGREADY = 3;
const uint32 MSG_CANDIDATESREADY = 4;
const uint32 MSG_ADDREMOTECANDIDATES = 5;
const uint32 MSG_ONREQUESTSIGNALING = 6;
const uint32 MSG_RESETSOCKETS = 7;

struct CreateParams {
  CreateParams() {}
  P2PSocket *socket;
  std::string name;
};

SocketManager::SocketManager(SessionManager *session_manager) {
  session_manager_ = session_manager;
  candidates_requested_ = false;
  writable_ = false;
}

SocketManager::~SocketManager() {
  assert(Thread::Current() == session_manager_->signaling_thread());

  // Are the sockets destroyed? If not, destroy them

  critSM_.Enter();
  while (sockets_.size() != 0) {
    P2PSocket *socket = sockets_[0];
    critSM_.Leave();
    DestroySocket(socket);
    critSM_.Enter();
  }
  critSM_.Leave();

  // Clear queues

  session_manager_->signaling_thread()->Clear(this);
  session_manager_->worker_thread()->Clear(this);
}

P2PSocket *SocketManager::CreateSocket(const std::string &name) {
  // Can occur on any thread
  CreateParams params;
  params.name = name;
  params.socket = NULL;
  TypedMessageData<CreateParams *> data(&params);
  session_manager_->worker_thread()->Send(this, MSG_CREATESOCKET, &data);
  return data.data()->socket;
}

P2PSocket *SocketManager::CreateSocket_w(const std::string &name) {
  // Only on worker thread
  assert(Thread::Current() == session_manager_->worker_thread());
  CritScope cs(&critSM_);
  P2PSocket *socket = new P2PSocket(name, session_manager_->port_allocator());
  socket->SignalCandidatesReady.connect(this, &SocketManager::OnCandidatesReady);
  socket->SignalState.connect(this, &SocketManager::OnSocketState);
  socket->SignalRequestSignaling.connect(this, &SocketManager::OnRequestSignaling);
  sockets_.push_back(socket);
  socket->StartProcessingCandidates();
  return socket;
}

void SocketManager::DestroySocket(P2PSocket *socket) {
  // Can occur on any thread
  TypedMessageData<P2PSocket *> data(socket);
  session_manager_->worker_thread()->Send(this, MSG_DESTROYSOCKET, &data);
}

void SocketManager::DestroySocket_w(P2PSocket *socket) {
  // Only on worker thread
  assert(Thread::Current() == session_manager_->worker_thread());

  // Only if socket exists
  CritScope cs(&critSM_);
  std::vector<P2PSocket *>::iterator it;
  it = std::find(sockets_.begin(), sockets_.end(), socket);
  if (it == sockets_.end())
    return;
  sockets_.erase(it);
  delete socket;
}

void SocketManager::StartProcessingCandidates() {
  // Only on signaling thread
  assert(Thread::Current() == session_manager_->signaling_thread());  

  // When sockets are created, their candidates are requested. 
  // When the candidates are ready, the client is signaled
  // on the signaling thread
  candidates_requested_ = true;
  session_manager_->signaling_thread()->Post(this, MSG_CANDIDATESREADY);
}

void SocketManager::OnSignalingReady() {
  session_manager_->worker_thread()->Post(this, MSG_ONSIGNALINGREADY);
}

void SocketManager::OnSignalingReady_w() {
  // Only on worker thread
  assert(Thread::Current() == session_manager_->worker_thread());
  for (uint32 i = 0; i < sockets_.size(); ++i) {
    sockets_[i]->OnSignalingReady();
  }
}

void SocketManager::OnCandidatesReady(
    P2PSocket *socket, const std::vector<Candidate>& candidates) {
  // Only on worker thread
  assert(Thread::Current() == session_manager_->worker_thread());

  // Remember candidates
  CritScope cs(&critSM_);
  std::vector<Candidate>::const_iterator it;
  for (it = candidates.begin(); it != candidates.end(); it++)
    candidates_.push_back(*it);

  // If candidates requested, tell signaling thread 
  if (candidates_requested_)
    session_manager_->signaling_thread()->Post(this, MSG_CANDIDATESREADY);
}

void SocketManager::ResetSockets() {
  assert(Thread::Current() == session_manager_->signaling_thread());  
  session_manager_->worker_thread()->Post(this, MSG_RESETSOCKETS);
}

void SocketManager::ResetSockets_w() {
  assert(Thread::Current() == session_manager_->worker_thread());  

  for (size_t i = 0; i < sockets_.size(); ++i)
    sockets_[i]->Reset();
}

void SocketManager::OnSocketState(P2PSocket* socket, P2PSocket::State state) {
  assert(Thread::Current() == session_manager_->worker_thread());

  bool writable = false;
  for (uint32 i = 0; i < sockets_.size(); ++i)
    if (sockets_[i]->writable())
      writable = true;

  if (writable_ != writable) {
    writable_ = writable;
    SignalState();
  }
}

void SocketManager::OnRequestSignaling() {
  assert(Thread::Current() == session_manager_->worker_thread());
  session_manager_->signaling_thread()->Post(this, MSG_ONREQUESTSIGNALING);
}


void SocketManager::AddRemoteCandidates(const std::vector<Candidate> &remote_candidates) {
  assert(Thread::Current() == session_manager_->signaling_thread());
  TypedMessageData<std::vector<Candidate> > *data = new TypedMessageData<std::vector<Candidate> >(remote_candidates);
  session_manager_->worker_thread()->Post(this, MSG_ADDREMOTECANDIDATES, data);
}

void SocketManager::AddRemoteCandidates_w(const std::vector<Candidate> &remote_candidates) {
  assert(Thread::Current() == session_manager_->worker_thread());

  // Local and remote candidates now exist, so connectivity checking can
  // commence. Tell the P2PSockets about the remote candidates.
  // Group candidates by socket name

  CritScope cs(&critSM_);
  std::vector<P2PSocket *>::iterator it_socket;
  for (it_socket = sockets_.begin(); it_socket != sockets_.end(); it_socket++) {
    // Create a vector of remote candidates for each socket
    std::string name = (*it_socket)->name();
    std::vector<Candidate> candidate_bundle;
    std::vector<Candidate>::const_iterator it_candidate;
    for (it_candidate = remote_candidates.begin(); it_candidate != remote_candidates.end(); it_candidate++) {
      if ((*it_candidate).name() == name)
        candidate_bundle.push_back(*it_candidate);
    }
    if (candidate_bundle.size() != 0)
      (*it_socket)->AddRemoteCandidates(candidate_bundle);
  }
}

void SocketManager::OnMessage(Message *message) {
  switch (message->message_id) {
  case MSG_CREATESOCKET:
    {
      assert(Thread::Current() == session_manager_->worker_thread());
      TypedMessageData<CreateParams *> *params = static_cast<TypedMessageData<CreateParams *> *>(message->pdata);
      params->data()->socket = CreateSocket_w(params->data()->name);
    }
    break;

  case MSG_DESTROYSOCKET:
    {
      assert(Thread::Current() == session_manager_->worker_thread());
      TypedMessageData<P2PSocket *> *data = static_cast<TypedMessageData<P2PSocket *> *>(message->pdata);
      DestroySocket_w(data->data());
    }
    break;

  case MSG_ONSIGNALINGREADY:
    assert(Thread::Current() == session_manager_->worker_thread());
    OnSignalingReady_w();
    break;

  case MSG_ONREQUESTSIGNALING:
    assert(Thread::Current() == session_manager_->signaling_thread());  
    SignalRequestSignaling();
    break;

  case MSG_CANDIDATESREADY:
    assert(Thread::Current() == session_manager_->signaling_thread());  
    if (candidates_requested_) {
      CritScope cs(&critSM_);
      if (candidates_.size() > 0) {
        SignalCandidatesReady(candidates_);
        candidates_.clear();
      }
    }
    break;

  case MSG_ADDREMOTECANDIDATES:
    {
      assert(Thread::Current() == session_manager_->worker_thread());
      TypedMessageData<const std::vector<Candidate> > *data = static_cast<TypedMessageData<const std::vector<Candidate> > *>(message->pdata);
      AddRemoteCandidates_w(data->data());
      delete data;
    }
    break;

  case MSG_RESETSOCKETS:
    ResetSockets_w();
    break;
  }
}

}

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

#include "talk/base/thread.h"
#include "talk/p2p/base/helpers.h"
#include "talk/session/phone/call.h"

namespace cricket {

const uint32 MSG_CHECKAUTODESTROY = 1;

Call::Call(PhoneSessionClient *session_client) : muted_(false) {
  session_client_ = session_client;
  id_ = CreateRandomId();
}

Call::~Call() {
  while (sessions_.begin() != sessions_.end()) {
    Session *session = sessions_[0];
    RemoveSession(session);
    session_client_->session_manager()->DestroySession(session);
  }
  Thread::Current()->Clear(this);
}

Session *Call::InitiateSession(const buzz::Jid &jid) {
  Session *session = session_client_->CreateSession(this);
  AddSession(session);
  session->Initiate(jid.Str(), session_client_->CreateOfferSessionDescription());
  return session;
}

void Call::AcceptSession(Session *session) {
  std::vector<Session *>::iterator it;
  it = std::find(sessions_.begin(), sessions_.end(), session);
  assert(it != sessions_.end());
  if (it != sessions_.end())
    session->Accept(session_client_->CreateAcceptSessionDescription(session->remote_description()));
}

void Call::RedirectSession(Session *session, const buzz::Jid &to) {
  std::vector<Session *>::iterator it;
  it = std::find(sessions_.begin(), sessions_.end(), session);
  assert(it != sessions_.end());
  if (it != sessions_.end())
    session->Redirect(to.Str());
}

void Call::RejectSession(Session *session) {
  std::vector<Session *>::iterator it;
  it = std::find(sessions_.begin(), sessions_.end(), session);
  assert(it != sessions_.end());
  if (it != sessions_.end())
    session->Reject();
}

void Call::TerminateSession(Session *session) {
  assert(std::find(sessions_.begin(), sessions_.end(), session) != sessions_.end());
  std::vector<Session *>::iterator it;
  it = std::find(sessions_.begin(), sessions_.end(), session);
  if (it != sessions_.end())
    (*it)->Terminate();
}

void Call::Terminate() {
  // Copy the list so that we can iterate over it in a stable way
  std::vector<Session *> sessions = sessions_;

  // There may be more than one session to terminate
  std::vector<Session *>::iterator it;
  for (it = sessions.begin(); it != sessions.end(); it++)
    TerminateSession(*it);
}

void Call::OnMessage(Message *message) {
  switch (message->message_id) {
  case MSG_CHECKAUTODESTROY:
    // If no more sessions for this call, delete it
    if (sessions_.size() == 0)
      session_client_->DestroyCall(this);
    break;
  }
}

const std::vector<Session *> &Call::sessions() {
  return sessions_;
}

void Call::AddSession(Session *session) {
  // Add session to list, create voice channel for this session
  sessions_.push_back(session);
  session->SignalState.connect(this, &Call::OnSessionState);
  session->SignalError.connect(this, &Call::OnSessionError);

  VoiceChannel *channel = session_client_->channel_manager()->CreateVoiceChannel(session);
  channel_map_[session->id()] = channel;

  // If this call has the focus, enable this channel
  if (session_client_->GetFocus() == this)
    channel->Enable(true);

  // Signal client
  SignalAddSession(this, session);
}

void Call::RemoveSession(Session *session) {
  // Remove session from list
  std::vector<Session *>::iterator it_session;
  it_session = std::find(sessions_.begin(), sessions_.end(), session);
  if (it_session == sessions_.end())
    return;
  sessions_.erase(it_session);

  // Destroy session channel
  std::map<SessionID, VoiceChannel *>::iterator it_channel;
  it_channel = channel_map_.find(session->id());
  if (it_channel != channel_map_.end()) {
    VoiceChannel *channel = it_channel->second;
    channel_map_.erase(it_channel);
    session_client_->channel_manager()->DestroyVoiceChannel(channel);
  }

  // Signal client
  SignalRemoveSession(this, session);

  // The call auto destroys when the lass session is removed
  Thread::Current()->Post(this, MSG_CHECKAUTODESTROY);
}

VoiceChannel* Call::GetChannel(Session* session) {
  std::map<SessionID, VoiceChannel *>::iterator it = channel_map_.find(session->id());
  assert(it != channel_map_.end());
  return it->second;
}

void Call::EnableChannels(bool enable) {
  std::vector<Session *>::iterator it;
  for (it = sessions_.begin(); it != sessions_.end(); it++) {
    VoiceChannel *channel = channel_map_[(*it)->id()];
    if (channel != NULL)
      channel->Enable(enable);
  }
}

void Call::Mute(bool mute) {
  muted_ = mute;
  std::vector<Session *>::iterator it;
  for (it = sessions_.begin(); it != sessions_.end(); it++) {
    VoiceChannel *channel = channel_map_[(*it)->id()];
    if (channel != NULL)
      channel->Mute(mute);
  }
}

void Call::Join(Call *call, bool enable) {
  while (call->sessions_.size() != 0) {
    // Move session
    Session *session = call->sessions_[0];
    call->sessions_.erase(call->sessions_.begin());
    sessions_.push_back(session);
    session->SignalState.connect(this, &Call::OnSessionState);
    session->SignalError.connect(this, &Call::OnSessionError);

    // Move channel
    std::map<SessionID, VoiceChannel *>::iterator it_channel;
    it_channel = call->channel_map_.find(session->id());
    if (it_channel != call->channel_map_.end()) {
      VoiceChannel *channel = (*it_channel).second;
      call->channel_map_.erase(it_channel);
      channel_map_[session->id()] = channel;
      channel->Enable(enable);
    }
  }
}

void Call::StartConnectionMonitor(Session *session, int cms) {
  std::map<SessionID, VoiceChannel *>::iterator it_channel;
  it_channel = channel_map_.find(session->id());
  if (it_channel != channel_map_.end()) {
    VoiceChannel *channel = (*it_channel).second;
    channel->SignalConnectionMonitor.connect(this, &Call::OnConnectionMonitor);
    channel->StartConnectionMonitor(cms);
  }
}

void Call::StopConnectionMonitor(Session *session) {
  std::map<SessionID, VoiceChannel *>::iterator it_channel;
  it_channel = channel_map_.find(session->id());
  if (it_channel != channel_map_.end()) {
    VoiceChannel *channel = (*it_channel).second;
    channel->StopConnectionMonitor();
    channel->SignalConnectionMonitor.disconnect(this);
  }
}

void Call::StartAudioMonitor(Session *session, int cms) {
  std::map<SessionID, VoiceChannel *>::iterator it_channel;
  it_channel = channel_map_.find(session->id());
  if (it_channel != channel_map_.end()) {
    VoiceChannel *channel = (*it_channel).second;
    channel->SignalAudioMonitor.connect(this, &Call::OnAudioMonitor);
    channel->StartAudioMonitor(cms);
  }
}

void Call::StopAudioMonitor(Session *session) {
  std::map<SessionID, VoiceChannel *>::iterator it_channel;
  it_channel = channel_map_.find(session->id());
  if (it_channel != channel_map_.end()) {
    VoiceChannel *channel = (*it_channel).second;
    channel->StopAudioMonitor();
    channel->SignalAudioMonitor.disconnect(this);
  }
}


void Call::OnConnectionMonitor(VoiceChannel *channel, const std::vector<ConnectionInfo> &infos) {
  SignalConnectionMonitor(this, channel->session(), infos);
}

void Call::OnAudioMonitor(VoiceChannel *channel, const AudioInfo& info) {
  SignalAudioMonitor(this, channel->session(), info);
}

uint32 Call::id() {
  return id_;
}

void Call::OnSessionState(Session *session, Session::State state) {
  SignalSessionState(this, session, state);
}

void Call::OnSessionError(Session *session, Session::Error error) {
  SignalSessionError(this, session, error);
}

}

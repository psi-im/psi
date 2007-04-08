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

#include "talk/session/phone/voicechannel.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/session/phone/phonesessionclient.h"
#include "talk/base/logging.h"
#include <cassert>
#undef SetPort

namespace {

// Delay before quality estimate is meaningful.
uint32 kQualityDelay = 5000; // in ms

}

namespace cricket {

VoiceChannel::VoiceChannel(ChannelManager *manager, Session *session, MediaChannel *channel) {
  channel_manager_ = manager;
  assert(channel_manager_->worker_thread() == Thread::Current());
  channel_ = channel;
  session_ = session;
  socket_monitor_ = NULL;
  audio_monitor_ = NULL;
  socket_ = session_->CreateSocket("rtp");
  socket_->SignalState.connect(this, &VoiceChannel::OnSocketState);
  socket_->SignalReadPacket.connect(this, &VoiceChannel::OnSocketRead);
  channel->SetInterface(this);
  enabled_ = false;
  paused_ = false;
  socket_writable_ = false;
  muted_ = false;
  LOG(INFO) << "Created voice channel";
  start_time_ = 0xFFFFFFFF - kQualityDelay;

  session->SignalState.connect(this, &VoiceChannel::OnSessionState);
  OnSessionState(session, session->state());
}

VoiceChannel::~VoiceChannel() {
  assert(channel_manager_->worker_thread() == Thread::Current());
  enabled_ = false;
  ChangeState();
  delete socket_monitor_;
  delete audio_monitor_;
  Thread::Current()->Clear(this);
  if (socket_ != NULL)
    session_->DestroySocket(socket_);
  LOG(INFO) << "Destroyed voice channel";
}

void VoiceChannel::OnMessage(Message *pmsg) {
  switch (pmsg->message_id) {
  case MSG_ENABLE:
    EnableMedia_w();
    break;

  case MSG_DISABLE:
    DisableMedia_w();
    break;

  case MSG_MUTE:
    MuteMedia_w();
    break;

  case MSG_UNMUTE:
    UnmuteMedia_w();
    break;

  case MSG_SETSENDCODEC:
    SetSendCodec_w();
    break;
  }
}

void VoiceChannel::Enable(bool enable) {
  // Can be called from thread other than worker thread
  channel_manager_->worker_thread()->Post(this, enable ? MSG_ENABLE : MSG_DISABLE);
}

void VoiceChannel::Mute(bool mute) {
  // Can be called from thread other than worker thread
  channel_manager_->worker_thread()->Post(this, mute ? MSG_MUTE : MSG_UNMUTE);
}

MediaChannel * VoiceChannel::channel() {
  return channel_;
}

void VoiceChannel::OnSessionState(Session* session, Session::State state) {
  if ((state == Session::STATE_RECEIVEDACCEPT) ||
      (state == Session::STATE_RECEIVEDINITIATE)) {
    channel_manager_->worker_thread()->Post(this, MSG_SETSENDCODEC);
  }
}

void VoiceChannel::SetSendCodec_w() {
  assert(channel_manager_->worker_thread() == Thread::Current());

  const PhoneSessionDescription* desc =
      static_cast<const PhoneSessionDescription*>(session()->remote_description());

  const char *codec = NULL;

  if (desc->codecs().size() > 0)
   PhoneSessionClient::FindMediaCodec(channel_manager_->media_engine(), desc, &codec);

  // The other client should have returned one of the codecs that we offered.
  // If they could not, they should have rejected the session.  So, if we get
  // into this state, we're dealing with a bad client, so we may as well just
  // pick the mostt common format there is: payload type zero.
  if (codec == NULL)
    codec = "PCMU";

  channel_->SetCodec(codec);
}

void VoiceChannel::OnSocketState(P2PSocket *socket, P2PSocket::State state) {
  switch (state) {
  case P2PSocket::STATE_WRITABLE:
    SocketWritable_w();
    break;

  default:
    SocketNotWritable_w();
    break;
  }
}

void VoiceChannel::OnSocketRead(P2PSocket *socket, const char *data, size_t len) {
  assert(channel_manager_->worker_thread() == Thread::Current());
  // OnSocketRead gets called from P2PSocket; now pass data to MediaEngine
  channel_->OnPacketReceived(data, (int)len);
}

void VoiceChannel::SendPacket(const void *data, size_t len) {
  // SendPacket gets called from MediaEngine; send to socket
  // MediaEngine will call us on a random thread.  The Send operation on the socket is
  // special in that it can handle this.
  socket_->Send(static_cast<const char *>(data), (unsigned int) len);
}

void VoiceChannel::ChangeState() {
  if (paused_ || !enabled_ || !socket_writable_) {
    channel_->SetPlayout(false);
    channel_->SetSend(false);
  } else {
    if (muted_) {
      channel_->SetSend(false);
      channel_->SetPlayout(true);
    } else {
      channel_->SetSend(true);
      channel_->SetPlayout(true);
    }
  }
}

void VoiceChannel::PauseMedia_w() {
  assert(channel_manager_->worker_thread() == Thread::Current());
  assert(!paused_);

  LOG(INFO) << "Voice channel paused";
  paused_ = true;
  ChangeState();
}

void VoiceChannel::UnpauseMedia_w() {
  assert(channel_manager_->worker_thread() == Thread::Current());
  assert(paused_);

  LOG(INFO) << "Voice channel unpaused";
  paused_ = false;
  ChangeState();
}

void VoiceChannel::EnableMedia_w() {
  assert(channel_manager_->worker_thread() == Thread::Current());
  if (enabled_)
    return;

  LOG(INFO) << "Voice channel enabled";
  enabled_ = true;
  start_time_ = Time();
  ChangeState();
}

void VoiceChannel::DisableMedia_w() {
  assert(channel_manager_->worker_thread() == Thread::Current());
  if (!enabled_)
    return;

  LOG(INFO) << "Voice channel disabled";
  enabled_ = false;
  ChangeState();
}

void VoiceChannel::MuteMedia_w() {
  assert(channel_manager_->worker_thread() == Thread::Current());
  if (muted_)
    return;

  LOG(INFO) << "Voice channel muted";
  muted_ = true;
  ChangeState();
}

void VoiceChannel::UnmuteMedia_w() {
  assert(channel_manager_->worker_thread() == Thread::Current());
  if (!muted_)
    return;

  LOG(INFO) << "Voice channel unmuted";
  muted_ = false;
  ChangeState();
}

void VoiceChannel::SocketWritable_w() {
  assert(channel_manager_->worker_thread() == Thread::Current());
  if (socket_writable_)
    return;

  LOG(INFO) << "Voice channel socket writable";
  socket_writable_ = true;
  ChangeState();
}

void VoiceChannel::SocketNotWritable_w() {
  assert(channel_manager_->worker_thread() == Thread::Current());
  if (!socket_writable_)
    return;

  LOG(INFO) << "Voice channel socket not writable";
  socket_writable_ = false;
  ChangeState();
}

void VoiceChannel::StartConnectionMonitor(int cms) {
  delete socket_monitor_;
  socket_monitor_ = new SocketMonitor(socket_, Thread::Current());
  socket_monitor_
    ->SignalUpdate.connect(this, &VoiceChannel::OnConnectionMonitorUpdate);
  socket_monitor_->Start(cms);
}

void VoiceChannel::StopConnectionMonitor() {
  if (socket_monitor_ != NULL) {
    socket_monitor_->Stop();
    socket_monitor_->SignalUpdate.disconnect(this);
    delete socket_monitor_;
    socket_monitor_ = NULL;
  }
}

void VoiceChannel::OnConnectionMonitorUpdate(SocketMonitor *monitor,
                      const std::vector<ConnectionInfo> &infos) {
  SignalConnectionMonitor(this, infos);
}

void VoiceChannel::StartAudioMonitor(int cms) {
  delete audio_monitor_;
  audio_monitor_ = new AudioMonitor(this, Thread::Current());
  audio_monitor_
    ->SignalUpdate.connect(this, &VoiceChannel::OnAudioMonitorUpdate);
  audio_monitor_->Start(cms);
}

void VoiceChannel::StopAudioMonitor() {
  if (audio_monitor_ != NULL) {
    audio_monitor_ ->Stop();
    audio_monitor_ ->SignalUpdate.disconnect(this);
    delete audio_monitor_ ;
    audio_monitor_  = NULL;
  }
}

void VoiceChannel::OnAudioMonitorUpdate(AudioMonitor *monitor,
                                        const AudioInfo& info) {
  SignalAudioMonitor(this, info);
}

Session *VoiceChannel::session() {
  return session_;
}

bool VoiceChannel::HasQuality() {
  return Time() >= start_time_ + kQualityDelay;
}

float VoiceChannel::GetCurrentQuality() {
  return channel_->GetCurrentQuality();
}

int VoiceChannel::GetInputLevel_w() {
  return channel_manager_->media_engine()->GetInputLevel();
}

int VoiceChannel::GetOutputLevel_w() {
  return channel_->GetOutputLevel();
}

Thread* VoiceChannel::worker_thread() {
  return channel_manager_->worker_thread();
}

}

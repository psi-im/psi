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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_GIPS
#include "talk/session/phone/gipsmediaengine.h"
#elif HAVE_GIPSLITE
#include "talk/session/phone/gipslitemediaengine.h"
#else
#include "talk/session/phone/linphonemediaengine.h"
#endif
#include "channelmanager.h"
#include <cassert>
#include <iostream>
namespace cricket {

const uint32 MSG_CREATEVOICECHANNEL = 1;
const uint32 MSG_DESTROYVOICECHANNEL = 2;
const uint32 MSG_SETAUDIOOPTIONS = 3;

ChannelManager::ChannelManager(Thread *worker_thread) {
#ifdef HAVE_GIPS
  media_engine_ = new GipsMediaEngine();
#elif HAVE_GIPSLITE
  media_engine_ = new GipsLiteMediaEngine();
#else
  media_engine_ = new LinphoneMediaEngine();
#endif
  worker_thread_ = worker_thread;
  initialized_ = false;
  Init();
}

ChannelManager::~ChannelManager() {
  Exit();
}

MediaEngine *ChannelManager::media_engine() {
  return media_engine_;
}

bool ChannelManager::Init() {
  initialized_ =  media_engine_->Init();
  return initialized_;
}

void ChannelManager::Exit() {
  if (!initialized_)
    return;

  // Need to destroy the voice channels

  while (true) {
    crit_.Enter();
    VoiceChannel *channel = NULL;
    if (channels_.begin() != channels_.end())
      channel = channels_[0];
    crit_.Leave();
    if (channel == NULL)
      break;
   delete channel;
  }
  media_engine_->Terminate();
}

struct CreateParams {
  Session *session;
  VoiceChannel *channel;
};

VoiceChannel *ChannelManager::CreateVoiceChannel(Session *session) {
  CreateParams params;
  params.session = session;
  params.channel = NULL;
  TypedMessageData<CreateParams *> data(&params);
  worker_thread_->Send(this, MSG_CREATEVOICECHANNEL, &data);
  return params.channel;
}

VoiceChannel *ChannelManager::CreateVoiceChannel_w(Session *session) {
  CritScope cs(&crit_);

  // This is ok to alloc from a thread other than the worker thread
  assert(initialized_);
  MediaChannel *channel = media_engine_->CreateChannel();
  if (channel == NULL)
    return NULL;

  VoiceChannel *voice_channel = new VoiceChannel(this, session, channel);
  channels_.push_back(voice_channel);
  return voice_channel;
}

void ChannelManager::DestroyVoiceChannel(VoiceChannel *voice_channel) {
  TypedMessageData<VoiceChannel *> data(voice_channel);
  worker_thread_->Send(this, MSG_DESTROYVOICECHANNEL, &data);
}

void ChannelManager::DestroyVoiceChannel_w(VoiceChannel *voice_channel) {
  CritScope cs(&crit_);
  // Destroy voice channel.
  assert(initialized_);
  std::vector<VoiceChannel *>::iterator it = std::find(channels_.begin(),
      channels_.end(), voice_channel);
  assert(it != channels_.end());
  if (it == channels_.end())
    return;
 
  channels_.erase(it);
  MediaChannel *channel = voice_channel->channel();
  delete voice_channel;
  delete channel;
}

void ChannelManager::SetAudioOptions(bool auto_gain_control, int wave_in_device,
                                     int wave_out_device) {
  AudioOptions options;
  options.auto_gain_control = auto_gain_control;
  options.wave_in_device = wave_in_device;
  options.wave_out_device = wave_out_device;
  TypedMessageData<AudioOptions> data(options);
  worker_thread_->Send(this, MSG_SETAUDIOOPTIONS, &data);
}

void ChannelManager::SetAudioOptions_w(AudioOptions options) {
  assert(worker_thread_ == Thread::Current());

  // Set auto gain control on
  if (media_engine_->SetAudioOptions(options.auto_gain_control?MediaEngine::AUTO_GAIN_CONTROL:0) != 0) {
    // TODO: We need to log these failures.
  }

  // Set the audio devices
  // This will fail if audio is already playing.  Stop all of the media
  // start it up again after changing the setting.
  {
    CritScope cs(&crit_);
    for (VoiceChannels::iterator it = channels_.begin();
         it < channels_.end();
         ++it) {
      (*it)->PauseMedia_w();
    }

    if (media_engine_->SetSoundDevices(options.wave_in_device, options.wave_out_device) == -1) {
      // TODO: We need to log these failures.
    }

    for (VoiceChannels::iterator it = channels_.begin();
         it < channels_.end();
         ++it) {
      (*it)->UnpauseMedia_w();
    }
  }
}

Thread *ChannelManager::worker_thread() {
  return worker_thread_;
}

void ChannelManager::OnMessage(Message *message) {
  switch (message->message_id) {
  case MSG_CREATEVOICECHANNEL:
    {
      TypedMessageData<CreateParams *> *data = static_cast<TypedMessageData<CreateParams *> *>(message->pdata);
      data->data()->channel = CreateVoiceChannel_w(data->data()->session);
    }
    break;

  case MSG_DESTROYVOICECHANNEL:
    {
      TypedMessageData<VoiceChannel *> *data = static_cast<TypedMessageData<VoiceChannel *> *>(message->pdata);
      DestroyVoiceChannel_w(data->data());
    }
    break;
  case MSG_SETAUDIOOPTIONS:
    {
      TypedMessageData<AudioOptions> *data = static_cast<TypedMessageData<AudioOptions> *>(message->pdata);
      SetAudioOptions_w(data->data());
    }
    break;
  }
}

}

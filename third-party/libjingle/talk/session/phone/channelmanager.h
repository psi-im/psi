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

#ifndef _CHANNELMANAGER_H_
#define _CHANNELMANAGER_H_

#include "talk/base/thread.h"
#include "talk/base/criticalsection.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/base/p2psocket.h"
#include "talk/session/phone/voicechannel.h"
#include "talk/session/phone/mediaengine.h"
#include <vector>

namespace cricket {

class VoiceChannel;

class ChannelManager : public MessageHandler {
public:
  ChannelManager(Thread *worker_thread);
  ~ChannelManager();

  VoiceChannel *CreateVoiceChannel(Session *session);
  void DestroyVoiceChannel(VoiceChannel *voice_channel);
  void SetAudioOptions(bool auto_gain_control, int wave_in_device,
                       int wave_out_device);

  MediaEngine *media_engine();
  Thread *worker_thread();

private:
  VoiceChannel *CreateVoiceChannel_w(Session *session);
  void DestroyVoiceChannel_w(VoiceChannel *voice_channel);
  void OnMessage(Message *message);
  bool Init();
  void Exit();

  struct AudioOptions {
    bool auto_gain_control;
    int wave_in_device;
    int wave_out_device;
  };
  void SetAudioOptions_w(AudioOptions options);

  Thread *worker_thread_;
  MediaEngine *media_engine_;
  bool initialized_;
  CriticalSection crit_;

  typedef std::vector<VoiceChannel*> VoiceChannels;
  VoiceChannels channels_;
};

}

#endif // _CHANNELMANAGER_H_

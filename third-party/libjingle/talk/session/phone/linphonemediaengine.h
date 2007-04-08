/*
 * Jingle call example
 * Copyright 2004--2005, Google Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// LinphoneMediaEngine is a Linphone implementation of MediaEngine

#ifndef TALK_SESSION_PHONE_LINPHONEMEDIAENGINE_H_
#define TALK_SESSION_PHONE_LINPHONEMEDIAENGINE_H_

extern "C" {
#include "talk/third_party/mediastreamer/mediastream.h"
}
#include "talk/session/phone/mediaengine.h"

namespace cricket {

class LinphoneMediaChannel : public MediaChannel {
 public:
  LinphoneMediaChannel();
  virtual ~LinphoneMediaChannel();
  virtual void SetCodec(const char *codec);
  virtual void OnPacketReceived(const void *data, int len);

  virtual void SetPlayout(bool playout);
  virtual void SetSend(bool send);

  virtual float GetCurrentQuality();
  virtual int GetOutputLevel();
  int fd() {return fd_;}
  bool mute() {return mute_;}
  bool dying() {return dying_;}
 private:
  AudioStream *audio_stream_;
  pthread_t thread_;
  int fd_;
  int pt_;
  bool dying_;
  bool mute_;
  bool play_;
};

class LinphoneMediaEngine : public MediaEngine {
 public:
  LinphoneMediaEngine();
  ~LinphoneMediaEngine();
  virtual bool Init();
  virtual void Terminate();
  
  virtual MediaChannel *CreateChannel();

  virtual int SetAudioOptions(int options);
  virtual int SetSoundDevices(int wave_in_device, int wave_out_device);

  virtual float GetCurrentQuality();
  virtual int GetInputLevel();
};

}  // namespace cricket

#endif  // TALK_SESSION_PHONE_LINPHONEMEDIAENGINE_H_

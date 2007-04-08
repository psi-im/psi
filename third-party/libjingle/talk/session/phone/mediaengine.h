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

// MediaEngine is an abstraction of a media engine which can be subclassed
// to support different media componentry backends.

#ifndef TALK_SESSION_PHONE_MEDIAENGINE_H_
#define TALK_SESSION_PHONE_MEDIAENGINE_H_

#include <string>
#include <vector>
#include "mediachannel.h"

namespace cricket {

class MediaEngine {
 public: 
  
  struct Codec {
    int id;
    std::string name;
    int preference;
    // Creates a codec with the given parameters.
    Codec(int pt, const std::string& nm, int pr) : id(pt), name(nm), preference(pr) {}
    // Ranks codecs by their preferences.
    bool operator <(const Codec& c) const { return preference > c.preference; }
  };
  
  // Bitmask flags for options that may be supported by the media engine implementation
  enum MediaEngineOptions {
     AUTO_GAIN_CONTROL = 1 << 1,
  };

  MediaEngine() {}

  // Initialize 
  virtual bool Init() = 0;
  virtual void Terminate() = 0;
  virtual MediaChannel *CreateChannel() = 0;
 
  virtual int SetAudioOptions(int options) = 0;
  virtual int SetSoundDevices(int wave_in_device, int wave_out_device) = 0;
  virtual int GetInputLevel() = 0;

  std::vector<Codec> &codecs() { return codecs_; }

  bool FindCodec(const char* codec) {
    for (std::vector<Codec>::iterator i = codecs_.begin(); i < codecs_.end(); i++) {
      if ((*i).name == codec)
	return true;
    }
    return false;
  }
  
  bool GetCodecPreference (const char *codec, int & preference) {
    for (std::vector<Codec>::iterator i = codecs_.begin(); i < codecs_.end(); i++) {
      if ((*i).name == codec) {
	preference = (*i).preference;
	return true;
      }
    }
    return false;
  }

 protected:
  std::vector<Codec> codecs_;
};  

}  // namespace cricket

#endif  // TALK_SESSION_PHONE_MEDIAENGINE_H_

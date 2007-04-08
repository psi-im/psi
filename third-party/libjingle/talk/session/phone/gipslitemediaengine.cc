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

// GipsLiteMediaEngine is a GIPS Voice Engine Lite implementation of MediaEngine
#include "talk/base/logging.h"
#include <cassert>
#include <iostream>
#include "gipslitemediaengine.h"
using namespace cricket;

#if 0
#define TRACK(x) LOG(LS_VERBOSE) << x
#else
#define TRACK(x)
#endif

//#define GIPS_TRACING

namespace {
struct CodecPref { const char* name; int pref; };
const size_t kNumGIPSCodecs = 12;
const CodecPref kGIPSCodecPrefs[kNumGIPSCodecs] = {
  { "ISAC", 7 },
  { "IPCMWB", 6},
  { "speex", 5},
  { "iLBC", 1 },
  { "G723", 4 },
  { "EG711U", 3 },
  { "EG711A", 3 },
  { "PCMU", 2 },
  { "PCMA", 2 },
  { "CN", 2 },
  { "red", -1 },
  { "telephone-event", -1 }
};
}



bool GipsLiteMediaChannel::FindGIPSCodec(const std::string& name, GIPS_CodecInst* codec) {
  int ncodecs = gips_->GIPSVE_GetNofCodecs();
  for (int i = 0; i < ncodecs; ++i) {
    GIPS_CodecInst gips_codec;
    if (gips_->GIPSVE_GetCodec(i, &gips_codec) >= 0) {
      if (strcmp(name.c_str(), gips_codec.plname) == 0) {
        *codec = gips_codec;
        return true;
      }
    }
  }
  return false;
}

void GipsLiteMediaChannel::SetCodec(const char *codec) {
  GIPS_CodecInst c;
  LOG(LS_INFO) << "Using " << codec;
  if (FindGIPSCodec(codec, &c))
    gips_->GIPSVE_SetSendCodec(gips_channel_, &c);
}


void GipsLiteMediaChannel::OnPacketReceived(const void *data, int len) {
  gips_->GIPSVE_ReceivedRTPPacket(gips_channel_, data, (int)len);
}

void GipsLiteMediaChannel::SetPlayout(bool playout) {
  if (playout)
    gips_->GIPSVE_StartPlayout(gips_channel_);
  else
    gips_->GIPSVE_StopPlayout(gips_channel_);
}

void GipsLiteMediaChannel::SetSend(bool send) {
  if (send)
    gips_->GIPSVE_StartSend(gips_channel_);
  else
    gips_->GIPSVE_StopSend(gips_channel_);
}

GipsLiteMediaChannel::GipsLiteMediaChannel(GipsVoiceEngineLite *gips) {
  network_interface_ = NULL;
  gips_ = gips;
  gips_channel_ = gips_->GIPSVE_CreateChannel();
  gips_->GIPSVE_SetSendTransport(gips_channel_, *this);
}

int GipsLiteMediaEngine::GetGIPSCodecPreference(const char* name) {
	for (size_t i = 0; i < kNumGIPSCodecs; ++i) {
    if (strcmp(kGIPSCodecPrefs[i].name, name) == 0)
      return kGIPSCodecPrefs[i].pref;
  }
  assert(false);
  return -1;
}

GipsLiteMediaEngine::GipsLiteMediaEngine() :
  gips_(GetGipsVoiceEngineLite()) {}

bool GipsLiteMediaEngine::Init() {
#if defined(GIPS_TRACING)
  gips_.GIPSVE_SetTraceFileName("\\gips.log");
  gips_.GIPSVE_SetTrace(2);
#endif

  TRACK("GIPSVE_Init");
  if (gips_.GIPSVE_Init(GIPS_EXPIRATION_MONTH, GIPS_EXPIRATION_DAY, GIPS_EXPIRATION_YEAR) == -1)
    return false;

  char buffer[1024];
  TRACK("GIPSVE_GetVersion");
  int r = gips_.GIPSVE_GetVersion(buffer, sizeof(buffer));
  LOG(LS_INFO) << "GIPS Version: " << r << ": " << buffer;
  
  // Set auto gain control on
  TRACK("GIPSVE_SetAGCStatus");
  if (gips_.GIPSVE_SetAGCStatus(1) == -1)
    return false;
  
  TRACK("GIPSVE_GetNofCodecs");
  int ncodecs = gips_.GIPSVE_GetNofCodecs();
  for (int i = 0; i < ncodecs; ++i) {
    GIPS_CodecInst gips_codec;
    if (gips_.GIPSVE_GetCodec(i, &gips_codec) >= 0) {
      Codec codec(gips_codec.pltype, gips_codec.plname, GetGIPSCodecPreference(gips_codec.plname));
      LOG(LS_INFO) << gips_codec.plname << " " << gips_codec.pltype;
      codecs().push_back(codec);
    }
  }
  return true;
}

void GipsLiteMediaEngine::Terminate() {
  gips_.GIPSVE_Terminate();
}

MediaChannel * GipsLiteMediaEngine::CreateChannel() {
  return new GipsLiteMediaChannel(&gips_);
}

int GipsLiteMediaEngine::SetAudioOptions(int options) {
  // Set auto gain control on
  if (gips_.GIPSVE_SetAGCStatus(options & AUTO_GAIN_CONTROL ? 1 : 0) == -1) {
    return -1;
    // TODO: We need to log these failures.
  }
  return 0;
}
 
int GipsLiteMediaEngine::SetSoundDevices(int wave_in_device, int wave_out_device) {
  if (gips_.GIPSVE_SetSoundDevices(wave_in_device, wave_out_device) == -1) {
    int error = gips_.GIPSVE_GetLastError();
    // TODO: We need to log these failures.
    return error;
    }
  return 0;
}

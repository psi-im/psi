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
extern "C" {
#include "talk/third_party/mediastreamer/mediastream.h"
#ifdef HAVE_ILBC
#include "talk/third_party/mediastreamer/msilbcdec.h"
#endif
#ifdef HAVE_SPEEX
#include "talk/third_party/mediastreamer/msspeexdec.h"
#endif
}
#include <ortp/ortp.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include "talk/session/phone/linphonemediaengine.h"

using namespace cricket;

void *thread_function(void *data)
{
  LinphoneMediaChannel *mc =(LinphoneMediaChannel*) data;
  while (mc->dying() == false) {
    MediaChannel::NetworkInterface *iface = mc->network_interface();
    char *buf[2048];
    int len;
    len = read(mc->fd(), buf, sizeof(buf));
    if (iface && (mc->mute()==FALSE))
      iface->SendPacket(buf, len);
  }
}

LinphoneMediaChannel::LinphoneMediaChannel() {
  pt_ = 102;
  dying_ = false;
  pthread_attr_t attr;
  audio_stream_ = NULL;

  struct sockaddr_in sockaddr;
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = INADDR_ANY;
  sockaddr.sin_port = htons(3000);
  fd_ = socket(PF_INET, SOCK_DGRAM, 0);
  fcntl(fd_, F_SETFL, 0, O_NONBLOCK);
  bind (fd_,(struct sockaddr*)&sockaddr, sizeof(sockaddr));
  
  pthread_attr_init(&attr);
  pthread_create(&thread_, &attr, &thread_function, this);
}

LinphoneMediaChannel::~LinphoneMediaChannel() {
  dying_ = true;
  pthread_join(thread_, NULL);
  audio_stream_stop(audio_stream_);
  close(fd_);
}

void LinphoneMediaChannel::SetCodec(const char *codec) {
  if (!strcmp(codec, "iLBC"))
    pt_ = 102;
  else if (!strcmp(codec, "speex"))
    pt_ = 110; 
  else
    pt_ = 0;
  if (audio_stream_)
    audio_stream_stop(audio_stream_);
  audio_stream_ = audio_stream_start(&av_profile, 2000, "127.0.0.1", 3000, pt_, 250);
}

void LinphoneMediaChannel::OnPacketReceived(const void *data, int len) {
  struct sockaddr_in sockaddr;
  sockaddr.sin_family = AF_INET;
  struct hostent *host = gethostbyname("localhost");
  memcpy(&sockaddr.sin_addr.s_addr, host->h_addr, host->h_length);
  sockaddr.sin_port = htons(2000);
  
  char buf[2048];
  memcpy(buf, data, len);
  
  if (buf[1] == pt_) {
  } else if (buf[1] == 13) {
  } else if (buf[1] == 102) {
    SetCodec("iLBC");
  } else if (buf[1] == 110) {
    SetCodec("speex");
  } else if (buf[1] == 0) {
    SetCodec("PCMU");
  }
  
  if (play_ && buf[1] != 13)
    sendto(fd_, buf, len, 0, (struct sockaddr*)&sockaddr, sizeof(sockaddr));
}

void LinphoneMediaChannel::SetPlayout(bool playout) {
  play_ = playout;
}

void LinphoneMediaChannel::SetSend(bool send) {
  mute_ = !send;
}

float LinphoneMediaChannel::GetCurrentQuality() {}
int LinphoneMediaChannel::GetOutputLevel() {}

LinphoneMediaEngine::LinphoneMediaEngine() {}
LinphoneMediaEngine::~LinphoneMediaEngine() {}

static void null_log_handler(const gchar *log_domain,
	     		     GLogLevelFlags log_level,
			     const gchar *message,
			     gpointer user_data) {
}

bool LinphoneMediaEngine::Init() {
  g_log_set_handler("MediaStreamer", G_LOG_LEVEL_MASK, null_log_handler, NULL);
  g_log_set_handler("oRTP", G_LOG_LEVEL_MASK, null_log_handler, NULL);
  g_log_set_handler("oRTP-stats", G_LOG_LEVEL_MASK, null_log_handler, NULL);
  ortp_init();
  ms_init();
 
#ifdef HAVE_SPEEX
  ms_speex_codec_init();
  rtp_profile_set_payload(&av_profile, 110, &speex_wb);
  codecs_.push_back(Codec(110, "speex", 8));
#endif

#ifdef HAVE_ILBC
  ms_ilbc_codec_init();
  rtp_profile_set_payload(&av_profile, 102, &payload_type_ilbc);
  codecs_.push_back(Codec(102, "iLBC", 4));
#endif

  rtp_profile_set_payload(&av_profile, 0, &pcmu8000);
  codecs_.push_back(Codec(0, "PCMU", 2));
  
return true;
}

void LinphoneMediaEngine::Terminate() {
 
}
  
MediaChannel *LinphoneMediaEngine::CreateChannel() {
  return new LinphoneMediaChannel();
}

int LinphoneMediaEngine::SetAudioOptions(int options) {}
int LinphoneMediaEngine::SetSoundDevices(int wave_in_device, int wave_out_device) {}

float LinphoneMediaEngine::GetCurrentQuality() {}
int LinphoneMediaEngine::GetInputLevel() {}

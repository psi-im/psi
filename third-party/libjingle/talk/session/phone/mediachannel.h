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

#ifndef TALK_SESSION_PHONE_MEDIACHANNEL_H_
#define TALK_SESSION_PHONE_MEDIACHANNEL_H_

namespace cricket {

class MediaChannel {
 public:
  class NetworkInterface {
  public:
    virtual void SendPacket(const void *data, size_t len) = 0;
  };
  MediaChannel() {network_interface_ = NULL;}
  virtual ~MediaChannel() {};
  void SetInterface(NetworkInterface *iface) {network_interface_ = iface;}
  virtual void SetCodec(const char *codec) = 0;
  virtual void OnPacketReceived(const void *data, int len) = 0;
  virtual void SetPlayout(bool playout) = 0;
  virtual void SetSend(bool send) = 0;
  virtual float GetCurrentQuality() = 0;
  virtual int GetOutputLevel() = 0;
  NetworkInterface *network_interface() {return network_interface_;}
 protected:
  NetworkInterface *network_interface_;
};

};  // namespace cricket

#endif  // TALK_SESSION_PHONE_MEDIACHANNEL_H_

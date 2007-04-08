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

#ifndef _PHONESESSIONCLIENT_H_
#define _PHONESESSIONCLIENT_H_

#include "talk/session/phone/call.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/base/sigslot.h"
#include "talk/base/messagequeue.h"
#include "talk/base/thread.h"
#include "talk/p2p/client/sessionclient.h"
#include "talk/p2p/base/sessionmanager.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/xmpp/xmppclient.h"
#include <map>

namespace cricket {

class Call;
class PhoneSessionDescription;

class PhoneSessionClient : public SessionClient {
public:
  PhoneSessionClient(const buzz::Jid& jid, SessionManager *manager);
  ~PhoneSessionClient();

  const buzz::Jid &jid() const;

  Call *CreateCall();
  void DestroyCall(Call *call);

  Call *GetFocus();
  void SetFocus(Call *call);

  void JoinCalls(Call *call_to_join, Call *call);

  void SetAudioOptions(bool auto_gain_control, int wave_in_device,
                       int wave_out_device) {
    if (channel_manager_)
      channel_manager_->SetAudioOptions(auto_gain_control, wave_in_device,
                                        wave_out_device);
  }

  sigslot::signal2<Call *, Call *> SignalFocus;
  sigslot::signal1<Call *> SignalCallCreate;
  sigslot::signal1<Call *> SignalCallDestroy;

  PhoneSessionDescription* CreateOfferSessionDescription();

  PhoneSessionDescription* CreateAcceptSessionDescription(const SessionDescription* offer);

  // Returns our preference for the given codec.
  static int GetMediaCodecPreference(const char* name);

  // Returns the name of the first codec in the description that
  // is found.  Return value is false if none was found.
  static bool FindMediaCodec(MediaEngine* gips,
                            const PhoneSessionDescription* desc,
                            const char **codec);

private:
  void OnSessionCreate(Session *session, bool received_initiate);
  void OnSessionState(Session *session, Session::State state);
  void OnSessionDestroy(Session *session);
  const SessionDescription *CreateSessionDescription(const buzz::XmlElement *element);
  buzz::XmlElement *TranslateSessionDescription(const SessionDescription *description);
  const std::string &GetSessionDescriptionName();
  const buzz::Jid &GetJid() const;
  Session *CreateSession(Call *call);
  ChannelManager *channel_manager();

  buzz::Jid jid_;
  Call *focus_call_;
  ChannelManager *channel_manager_;
  std::map<uint32, Call *> calls_;
  std::map<SessionID, Call *> session_map_;

  friend class Call;
};

class PhoneSessionDescription: public SessionDescription {
public:
  // Returns the list of codecs sorted by our preference.
  const std::vector<MediaEngine::Codec>& codecs() const { return codecs_; }
  // Adds another codec to the list.
  void AddCodec(const MediaEngine::Codec& codec) { codecs_.push_back(codec); }
  // Sorts the list of codecs by preference.
  void Sort() { std::stable_sort(codecs_.begin(), codecs_.end()); }

private:
  std::vector<MediaEngine::Codec> codecs_;

#if defined(FEATURE_ENABLE_VOICEMAIL)
  std::string lang_;
#endif

};

}

#endif // _PHONESESSIONCLIENT_H_

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

#ifndef _CALL_H_
#define _CALL_H_

#include "talk/base/messagequeue.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/client/socketmonitor.h"
#include "talk/xmpp/jid.h"
#include "talk/session/phone/phonesessionclient.h"
#include "talk/session/phone/voicechannel.h"
#include "talk/session/phone/audiomonitor.h"

#include <map>
#include <vector>

namespace cricket {

class PhoneSessionClient;

class Call : public MessageHandler, public sigslot::has_slots<> {
public:
  Call(PhoneSessionClient *session_client);
  ~Call();

  Session *InitiateSession(const buzz::Jid &jid);
  void AcceptSession(Session *session);
  void RedirectSession(Session *session, const buzz::Jid &to);
  void RejectSession(Session *session);
  void TerminateSession(Session *session);
  void Terminate();
  void StartConnectionMonitor(Session *session, int cms);
  void StopConnectionMonitor(Session *session);
  void StartAudioMonitor(Session *session, int cms);
  void StopAudioMonitor(Session *session);
  void Mute(bool mute);

  const std::vector<Session *> &sessions();
  uint32 id();
  bool muted() const { return muted_; }

  sigslot::signal2<Call *, Session *> SignalAddSession;
  sigslot::signal2<Call *, Session *> SignalRemoveSession;
  sigslot::signal3<Call *, Session *, Session::State> SignalSessionState;
  sigslot::signal3<Call *, Session *, Session::Error> SignalSessionError;
  sigslot::signal3<Call *, Session *, const std::vector<ConnectionInfo> &> SignalConnectionMonitor;
  sigslot::signal3<Call *, Session *, const AudioInfo&> SignalAudioMonitor;

private:
  void OnMessage(Message *message);
  void OnSessionState(Session *session, Session::State state);
  void OnSessionError(Session *session, Session::Error error);
  void AddSession(Session *session);
  void RemoveSession(Session *session);
  void EnableChannels(bool enable);
  void Join(Call *call, bool enable);
  void OnConnectionMonitor(VoiceChannel *channel, const std::vector<ConnectionInfo> &infos);
  void OnAudioMonitor(VoiceChannel *channel, const AudioInfo& info);
  VoiceChannel* GetChannel(Session* session);

  uint32 id_;
  PhoneSessionClient *session_client_;
  std::vector<Session *> sessions_;
  std::map<SessionID, VoiceChannel *> channel_map_;
  bool muted_;

  friend class PhoneSessionClient;
};

}

#endif // _CALL_H_

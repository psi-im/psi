/*
 * libjingle
 * Copyright 2004--2006, Google Inc.
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

#ifndef __TUNNELSESSIONCLIENT_H__
#define __TUNNELSESSIONCLIENT_H__

#include <vector>
#include "talk/base/criticalsection.h"
#include "talk/base/stream.h"
#include "talk/p2p/base/pseudotcp.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/p2p/base/sessionmanager.h"
#include "talk/p2p/client/sessionclient.h"
#include "talk/xmllite/qname.h"
#include "talk/xmpp/constants.h"

namespace cricket {

class TunnelSession;
class TunnelStream;

///////////////////////////////////////////////////////////////////////////////
// TunnelSessionClient
///////////////////////////////////////////////////////////////////////////////

class TunnelSessionClient : public SessionClient, public MessageHandler {
public:
  TunnelSessionClient(const buzz::Jid& jid, SessionManager* manager);
  virtual ~TunnelSessionClient();

  const SessionDescription* CreateSessionDescription(
    const buzz::XmlElement* element);
  buzz::XmlElement* TranslateSessionDescription(
    const SessionDescription* description);
  const std::string& GetSessionDescriptionName();
  virtual const buzz::Jid& GetJid() const;

  void OnSessionCreate(Session* session, bool received);
  void OnSessionDestroy(Session* session);

  // This can be called on any thread.  The stream interface is thread-safe, but
  // notifications must be registered on the creating thread.
  StreamInterface* CreateTunnel(const buzz::Jid& to,
                                const std::string& description);

  // Signal arguments are this, initiator, description, session
  sigslot::signal4<TunnelSessionClient*, buzz::Jid, std::string, Session*>
    SignalIncomingTunnel;
  StreamInterface* AcceptTunnel(Session* session);
  void DeclineTunnel(Session* session);

private:
  void OnMessage(Message* pmsg);

  buzz::Jid jid_;
  std::vector<TunnelSession*> sessions_;
  bool shutdown_;
};

///////////////////////////////////////////////////////////////////////////////
// TunnelSession
// Note: The lifetime of TunnelSession is complicated.  It needs to survive
// until the following three conditions are true:
// 1) TunnelStream has called Close (tracked via non-null stream_)
// 2) PseudoTcp has completed (tracked via non-null tcp_)
// 3) Session has been destroyed (tracked via non-null session_)
// This is accomplished by calling CheckDestroy after these indicators change.
///////////////////////////////////////////////////////////////////////////////

class TunnelSession : public IPseudoTcpNotify, public MessageHandler,
  public sigslot::has_slots<> {
public:
  // Signalling thread methods
  TunnelSession(Thread* stream_thread, TunnelSessionClient* client,
                Session* session);
  virtual ~TunnelSession();

  StreamInterface* GetStream();
  bool CheckSession(Session* session, bool release);
  void ReleaseClient();

  // Event thread methods
  StreamState GetState() const;
  StreamResult Read(void* buffer, size_t buffer_len, size_t* read, int* error);
  StreamResult Write(const void* data, size_t data_len, size_t* written,
                     int* error);
  void Close();

private:
  // Multi-thread methods
  void OnMessage(Message* pmsg);
  void AdjustClock(bool clear = true);
  void CheckDestroy();

  // Signal thread methods
  void OnSessionState(Session* session, Session::State state);
  void OnInitiate();
  void OnAccept();
  void OnTerminate();

  // Worker thread methods
  void OnSocketState(P2PSocket* socket, P2PSocket::State state);
  void OnSocketRead(P2PSocket* socket, const char* data, size_t size);
  void OnSocketConnectionChanged(P2PSocket* socket, const SocketAddress& addr);

  virtual void OnTcpOpen(PseudoTcp* ptcp);
  virtual void OnTcpReadable(PseudoTcp* ptcp);
  virtual void OnTcpWriteable(PseudoTcp* ptcp);
  virtual void OnTcpClosed(PseudoTcp* ptcp, uint32 nError);
  virtual IPseudoTcpNotify::WriteResult TcpWritePacket(PseudoTcp* tcp,
                                                       const char* buffer,
                                                       size_t len);

  Thread* signal_thread_, * worker_thread_, * stream_thread_;
  TunnelSessionClient* client_;
  Session* session_;
  P2PSocket* socket_;
  PseudoTcp* tcp_;
  TunnelStream* stream_;
  mutable cricket::CriticalSection cs_;
};

///////////////////////////////////////////////////////////////////////////////
// TunnelStream
// Note: Because TunnelStream provides a stream interface, it's lifetime is
// controlled by the owner of the stream pointer.  As a result, we must support
// both the TunnelSession disappearing before TunnelStream, and vice versa.
///////////////////////////////////////////////////////////////////////////////

class TunnelStream : public StreamInterface {
public:
  TunnelStream(TunnelSession* tunnel);
  virtual ~TunnelStream();

  virtual StreamState GetState() const;
  virtual StreamResult Read(void* buffer, size_t buffer_len,
                            size_t* read, int* error);
  virtual StreamResult Write(const void* data, size_t data_len,
                             size_t* written, int* error);
  virtual void Close();

  virtual bool GetSize(size_t* size) const { return false; }
  virtual bool Rewind() { return false; }

private:
  // tunnel_ is accessed and modified exclusively on the event thread, to
  // avoid thread contention.  This means that the TunnelSession cannot go
  // away until after it receives a Close() from TunnelStream.
  TunnelSession* tunnel_;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace cricket

#endif // __TUNNELSESSIONCLIENT_H__

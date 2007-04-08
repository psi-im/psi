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

#include "talk/base/basicdefs.h"
#include "talk/base/basictypes.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/stringutils.h"
#include "talk/p2p/base/helpers.h"
#include "talk/xmllite/xmlelement.h"
#include "tunnelsessionclient.h"

namespace cricket {

const std::string NS_TUNNEL("http://www.google.com/talk/tunnel");
const buzz::QName QN_TUNNEL_DESCRIPTION(NS_TUNNEL, "description");
const buzz::QName QN_TUNNEL_TYPE(NS_TUNNEL, "type");

enum {
  MSG_CLOCK = 1,
  MSG_DESTROY,
  MSG_TERMINATE,
  MSG_EVENT,
  MSG_CREATE_TUNNEL,
};

struct EventData : public MessageData {
  int event, error;
  EventData(int ev, int err = 0) : event(ev), error(err) { }
};

struct CreateTunnelData : public MessageData {
  buzz::Jid jid;
  std::string description;
  Thread* thread;
  StreamInterface* stream;
};

const ConstantLabel SESSION_STATES[] = {
  KLABEL(Session::STATE_INIT),
  KLABEL(Session::STATE_SENTINITIATE),
  KLABEL(Session::STATE_RECEIVEDINITIATE),
  KLABEL(Session::STATE_SENTACCEPT),
  KLABEL(Session::STATE_RECEIVEDACCEPT),
  KLABEL(Session::STATE_SENTMODIFY),
  KLABEL(Session::STATE_RECEIVEDMODIFY),
  KLABEL(Session::STATE_SENTREJECT),
  KLABEL(Session::STATE_RECEIVEDREJECT),
  KLABEL(Session::STATE_SENTREDIRECT),
  KLABEL(Session::STATE_SENTTERMINATE),
  KLABEL(Session::STATE_RECEIVEDTERMINATE),
  KLABEL(Session::STATE_INPROGRESS),
  LASTLABEL
};

const ConstantLabel P2PSOCKET_STATES[] = {
  KLABEL(P2PSocket::STATE_CONNECTING),
  KLABEL(P2PSocket::STATE_WRITABLE),
  LASTLABEL
};

///////////////////////////////////////////////////////////////////////////////
// TunnelSessionDescription
///////////////////////////////////////////////////////////////////////////////

struct TunnelSessionDescription : public SessionDescription {
  std::string description;

  TunnelSessionDescription(const std::string& desc) : description(desc) { }
};

///////////////////////////////////////////////////////////////////////////////
// TunnelSessionClient
///////////////////////////////////////////////////////////////////////////////

TunnelSessionClient::TunnelSessionClient(const buzz::Jid& jid,
                                         SessionManager* manager)
  : SessionClient(manager), jid_(jid), shutdown_(false) {
}

TunnelSessionClient::~TunnelSessionClient() {
  shutdown_ = true;
  for (std::vector<TunnelSession*>::iterator it = sessions_.begin();
       it != sessions_.end();
       ++it) {
     (*it)->ReleaseClient();
  }
}

const SessionDescription*
TunnelSessionClient::CreateSessionDescription(const buzz::XmlElement* element) {
  if (const buzz::XmlElement* type_elem = element->FirstNamed(QN_TUNNEL_TYPE)) {
    return new TunnelSessionDescription(type_elem->BodyText());
  }
  ASSERT(false);
  return 0;
}

buzz::XmlElement*
TunnelSessionClient::TranslateSessionDescription(
    const SessionDescription* description) {
  const TunnelSessionDescription* desc =
      static_cast<const TunnelSessionDescription*>(description);

  buzz::XmlElement* root = new buzz::XmlElement(QN_TUNNEL_DESCRIPTION, true);
  buzz::XmlElement* type_elem = new buzz::XmlElement(QN_TUNNEL_TYPE);
  type_elem->SetBodyText(desc->description);
  root->AddElement(type_elem);
  return root;
}

const std::string&
TunnelSessionClient::GetSessionDescriptionName() {
  return NS_TUNNEL;
}

const buzz::Jid&
TunnelSessionClient::GetJid() const {
  return jid_;
}

void
TunnelSessionClient::OnSessionCreate(Session* session, bool received) {
  LOG(LS_INFO) << "TunnelSessionClient::OnSessionCreate: received=" << received;
  ASSERT(session_manager()->signaling_thread()->IsCurrent());
  if (received)
    sessions_.push_back(new TunnelSession(Thread::Current(), this, session));
}

void
TunnelSessionClient::OnSessionDestroy(Session* session) {
  LOG(LS_INFO) << "TunnelSessionClient::OnSessionDestroy";
  ASSERT(session_manager()->signaling_thread()->IsCurrent());
  if (shutdown_)
    return;
  for (std::vector<TunnelSession*>::iterator it = sessions_.begin();
       it != sessions_.end();
       ++it) {
    if ((*it)->CheckSession(session, true)) {
      sessions_.erase(it);
      return;
    }
  }
}

StreamInterface*
TunnelSessionClient::CreateTunnel(const buzz::Jid& to,
                                  const std::string& description) {
  // Valid from any thread
  CreateTunnelData data;
  data.jid = to;
  data.description = description;
  data.thread = Thread::Current();
  session_manager()->signaling_thread()->Send(this, MSG_CREATE_TUNNEL, &data);
  return data.stream;
}

StreamInterface *
TunnelSessionClient::AcceptTunnel(Session* session) {
  ASSERT(session_manager()->signaling_thread()->IsCurrent());
  TunnelSession* tunnel = NULL;
  for (std::vector<TunnelSession*>::iterator it = sessions_.begin();
       it != sessions_.end();
       ++it) {
    if ((*it)->CheckSession(session, false)) {
      tunnel = *it;
      break;
    }
  }
  ASSERT(tunnel != NULL);

  const TunnelSessionDescription* in_desc =
    static_cast<const TunnelSessionDescription*>(
      session->remote_description());
  TunnelSessionDescription* out_desc = new TunnelSessionDescription(
    in_desc->description);
  session->Accept(out_desc);
  return tunnel->GetStream();
}

void
TunnelSessionClient::DeclineTunnel(Session* session) {
  ASSERT(session_manager()->signaling_thread()->IsCurrent());
  session->Reject();
}

void
TunnelSessionClient::OnMessage(Message* pmsg) {
  if (pmsg->message_id == MSG_CREATE_TUNNEL) {
    ASSERT(session_manager()->signaling_thread()->IsCurrent());
    CreateTunnelData* data = static_cast<CreateTunnelData*>(pmsg->pdata);
    Session* session = session_manager()->CreateSession(NS_TUNNEL, jid_.Str());
    TunnelSession* tunnel = new TunnelSession(data->thread, this, session);
    sessions_.push_back(tunnel);
    TunnelSessionDescription* desc = new TunnelSessionDescription(data->description);
    session->Initiate(data->jid.Str(), desc);
    data->stream = tunnel->GetStream();
  }
}

///////////////////////////////////////////////////////////////////////////////
// TunnelSession
///////////////////////////////////////////////////////////////////////////////

//
// Signalling thread methods
//

TunnelSession::TunnelSession(cricket::Thread* stream_thread,
                             TunnelSessionClient* client, Session* session)
    : signal_thread_(client->session_manager()->signaling_thread()),
      worker_thread_(client->session_manager()->worker_thread()),
      stream_thread_(stream_thread),
      client_(client), session_(session),
      socket_(NULL), tcp_(NULL),
      stream_(NULL) {
  ASSERT(signal_thread_->IsCurrent());
  session_->SignalState.connect(this, &TunnelSession::OnSessionState);
}

TunnelSession::~TunnelSession() {
  ASSERT(signal_thread_->IsCurrent());
  ASSERT(session_ == NULL);
  ASSERT(stream_ == NULL);
  ASSERT(tcp_ == NULL);
}

StreamInterface*
TunnelSession::GetStream() {
  ASSERT(signal_thread_->IsCurrent());
  CritScope lock(&cs_);
  if (!stream_)
    stream_ = new TunnelStream(this);
  return stream_;
}

bool
TunnelSession::CheckSession(Session* session, bool release) {
  ASSERT(signal_thread_->IsCurrent());
  // Note: This is only called by TunnelSessionClient, which only tracks
  // tunnels with non-empty sessions.
  ASSERT(session_ != NULL);
  CritScope lock(&cs_);
  if (session_ != session)
    return false;
  if (release) {
    session_ = NULL;
    socket_ = NULL;
    CheckDestroy();
  }
  return true;
}

void
TunnelSession::ReleaseClient() {
  ASSERT(client_ != NULL);
  SessionClient* client = client_;
  client_ = NULL;
  if (session_)
    client->session_manager()->DestroySession(session_);
}

void
TunnelSession::OnSessionState(Session* session, Session::State state) {
  LOG(LS_INFO) << "TunnelSession::OnSessionState("
               << cricket::nonnull(FindLabel(state, SESSION_STATES), "Unknown")
               << ")";
  ASSERT(signal_thread_->IsCurrent());
  ASSERT(session == session_);

  switch (state) {
  case Session::STATE_RECEIVEDINITIATE:
    OnInitiate();
    break;
  case Session::STATE_SENTACCEPT:
  case Session::STATE_RECEIVEDACCEPT:
    OnAccept();
    break;
  case Session::STATE_SENTTERMINATE:
  case Session::STATE_RECEIVEDTERMINATE:
    OnTerminate();
    break;
  }
}

void
TunnelSession::OnInitiate() {
  ASSERT(signal_thread_->IsCurrent());
  const TunnelSessionDescription* in_desc =
    static_cast<const TunnelSessionDescription*>(
      session_->remote_description());

  if (client_) {
    client_->SignalIncomingTunnel(client_,
                                  buzz::Jid(session_->remote_address()),
                                  in_desc->description,
                                  session_);
  } else {
    session_->Reject();
  }
}

void
TunnelSession::OnAccept() {
  ASSERT(signal_thread_->IsCurrent());
  CritScope lock(&cs_);
  ASSERT(!socket_ && !tcp_);
  socket_ = session_->CreateSocket("tcp");
  socket_->SetOption(Socket::OPT_DONTFRAGMENT, 1);
  socket_->SignalState.connect(this, &TunnelSession::OnSocketState);
  socket_->SignalReadPacket.connect(this, &TunnelSession::OnSocketRead);
  socket_->SignalConnectionChanged.connect(this,
    &TunnelSession::OnSocketConnectionChanged);
  tcp_ = new PseudoTcp(this, 0);
}

void
TunnelSession::OnTerminate() {
  ASSERT(signal_thread_->IsCurrent());
  CritScope lock(&cs_);
  if ((stream_ != NULL)
      && ((tcp_ == NULL) || (tcp_->State() != PseudoTcp::TCP_CLOSED)))
    stream_thread_->Post(this, MSG_EVENT, new EventData(SE_CLOSE, 0));
}

//
// Event thread methods
//

StreamState
TunnelSession::GetState() const {
  ASSERT(stream_ != NULL && stream_thread_->IsCurrent());
  CritScope lock(&cs_);
  if (!tcp_)
    return SS_OPENING;
  switch (tcp_->State()) {
    case PseudoTcp::TCP_LISTEN:
    case PseudoTcp::TCP_SYN_SENT:
    case PseudoTcp::TCP_SYN_RECEIVED:
      return SS_OPENING;
    case PseudoTcp::TCP_ESTABLISHED:
      return SS_OPEN;
    case PseudoTcp::TCP_CLOSED:
    default:
      return SS_CLOSED;
  }
}

StreamResult
TunnelSession::Read(void* buffer, size_t buffer_len,
                    size_t* read, int* error) {
  ASSERT(stream_ != NULL && stream_thread_->IsCurrent());
  CritScope lock(&cs_);
  if (!tcp_)
    return SR_BLOCK;
  int result = tcp_->Recv(static_cast<char*>(buffer), buffer_len);
  LOG(LS_VERBOSE) << "TunnelSession::Read - Recv returned: " << result;
  if (result > 0) {
    if (read)
      *read = result;
    // PseudoTcp doesn't currently support repeated Readable signals.  Simulate
    // them here.
    stream_thread_->Post(this, MSG_EVENT, new EventData(SE_READ));
    return SR_SUCCESS;
  } else if (IsBlockingError(tcp_->GetError())) {
    return SR_BLOCK;
  } else {
    if (error)
      *error = tcp_->GetError();
    return SR_ERROR;
  }
  AdjustClock();
}

StreamResult
TunnelSession::Write(const void* data, size_t data_len,
                     size_t* written, int* error) {
  ASSERT(stream_ != NULL && stream_thread_->IsCurrent());
  CritScope lock(&cs_);
  if (!tcp_)
    return SR_BLOCK;
  int result = tcp_->Send(static_cast<const char*>(data), data_len);
  LOG(LS_VERBOSE) << "TunnelSession::Write - Send returned: " << result;
  if (result > 0) {
    if (written)
      *written = result;
    return SR_SUCCESS;
  } else if (IsBlockingError(tcp_->GetError())) {
    return SR_BLOCK;
  } else {
    if (error)
      *error = tcp_->GetError();
    return SR_ERROR;
  }
  AdjustClock();
}

void
TunnelSession::Close() {
  ASSERT(stream_ != NULL && stream_thread_->IsCurrent());
  CritScope lock(&cs_);
  stream_ = NULL;
  // Clear out any pending event notifications
  stream_thread_->Clear(this, MSG_EVENT);
  if (!tcp_) {
    CheckDestroy();
    return;
  }
  tcp_->Close();
  AdjustClock();
}

//
// Worker thread methods
//

void
TunnelSession::OnSocketState(P2PSocket* socket, P2PSocket::State state) {
  LOG(LS_INFO) << "TunnelSession::OnSocketState("
            << cricket::nonnull(FindLabel(state, P2PSOCKET_STATES), "Unknown")
            << ")";
  ASSERT(worker_thread_->IsCurrent());
  ASSERT(socket == socket_);
  if (state == P2PSocket::STATE_WRITABLE) {
    CritScope lock(&cs_);
    ASSERT(session_ != NULL);
    if (session_->initiator()) {
      ASSERT(tcp_ != NULL);
      tcp_->Connect();
      AdjustClock();
    }
  }
}

void
TunnelSession::OnSocketRead(P2PSocket* socket, const char* data, size_t size) {
  LOG(LS_VERBOSE) << "TunnelSession::OnSocketRead(" << size << ")";
  ASSERT(worker_thread_->IsCurrent());
  ASSERT(socket == socket_);
  CritScope lock(&cs_);
  ASSERT(tcp_ != NULL);
  tcp_->NotifyPacket(data, size);
  AdjustClock();
}

void
TunnelSession::OnSocketConnectionChanged(P2PSocket* socket,
                                         const SocketAddress& addr) {
  ASSERT(worker_thread_->IsCurrent());
  ASSERT(socket == socket_);
  CritScope lock(&cs_);
  ASSERT(tcp_ != NULL);

  scoped_ptr<Socket> mtu_socket(
    worker_thread_->socketserver()
           ->CreateSocket(SOCK_DGRAM));

  uint16 mtu = 65535;
  if (mtu_socket->Connect(addr) < 0) {
    LOG(LS_ERROR) << "Socket::Connect: " << socket_->GetError();
  } else if (mtu_socket->EstimateMTU(&mtu) < 0) {
    LOG(LS_ERROR) << "Socket::EstimateMTU: " << socket_->GetError();
  } else {
    tcp_->NotifyMTU(mtu);
    AdjustClock();
  }
}

void
TunnelSession::OnTcpOpen(PseudoTcp* tcp) {
  LOG(LS_VERBOSE) << "TunnelSession::OnTcpOpen";
  ASSERT(cs_.CurrentThreadIsOwner());
  ASSERT(worker_thread_->IsCurrent());
  ASSERT(tcp == tcp_);
  if (stream_)
    stream_thread_->Post(this, MSG_EVENT,
                        new EventData(SE_OPEN | SE_READ | SE_WRITE));
}

void
TunnelSession::OnTcpReadable(PseudoTcp* tcp) {
  LOG(LS_VERBOSE) << "TunnelSession::OnTcpReadable";
  ASSERT(cs_.CurrentThreadIsOwner());
  ASSERT(worker_thread_->IsCurrent());
  ASSERT(tcp == tcp_);
  if (stream_)
    stream_thread_->Post(this, MSG_EVENT, new EventData(SE_READ));
}

void
TunnelSession::OnTcpWriteable(PseudoTcp* tcp) {
  LOG(LS_VERBOSE) << "TunnelSession::OnTcpWriteable";
  ASSERT(cs_.CurrentThreadIsOwner());
  ASSERT(worker_thread_->IsCurrent());
  ASSERT(tcp == tcp_);
  if (stream_)
    stream_thread_->Post(this, MSG_EVENT, new EventData(SE_WRITE));
}

void
TunnelSession::OnTcpClosed(PseudoTcp* tcp, uint32 nError) {
  LOG(LS_VERBOSE) << "TunnelSession::OnTcpClosed";
  ASSERT(cs_.CurrentThreadIsOwner());
  ASSERT(worker_thread_->IsCurrent());
  ASSERT(tcp == tcp_);
  if (stream_)
    stream_thread_->Post(this, MSG_EVENT, new EventData(SE_CLOSE, nError));
}

//
// Multi-thread methods
//

void
TunnelSession::OnMessage(Message* pmsg) {
  if (pmsg->message_id == MSG_CLOCK) {
    //LOG(LS_INFO) << "TunnelSession::OnMessage(MSG_CLOCK)";
    ASSERT(worker_thread_->IsCurrent());
    uint32 now = Time();
    CritScope lock(&cs_);
    tcp_->NotifyClock(now);
    AdjustClock(false);
  } else if (pmsg->message_id == MSG_EVENT) {
    EventData* data = static_cast<EventData*>(pmsg->pdata);
    //LOG(LS_INFO) << "TunnelSession::OnMessage(MSG_EVENT, " << data->event << ", " << data->error << ")";
    ASSERT(stream_ != NULL && stream_thread_->IsCurrent());
    stream_->SignalEvent(stream_, data->event, data->error);
    delete data;
  } else if (pmsg->message_id == MSG_TERMINATE) {
    LOG(LS_INFO) << "TunnelSession::OnMessage(MSG_TERMINATE)";
    ASSERT(signal_thread_->IsCurrent());
    if (session_ != NULL)
      session_->Terminate();
  } else if (pmsg->message_id == MSG_DESTROY) {
    LOG(LS_INFO) << "TunnelSession::OnMessage(MSG_DESTROY)";
    ASSERT(signal_thread_->IsCurrent());
    delete this;
  } else {
    ASSERT(false);
  }
}

IPseudoTcpNotify::WriteResult
TunnelSession::TcpWritePacket(PseudoTcp* tcp, const char* buffer, size_t len) {
  ASSERT(cs_.CurrentThreadIsOwner());
  ASSERT(tcp == tcp_);
  if (!socket_)
    return IPseudoTcpNotify::WR_FAIL;
  int sent = socket_->Send(buffer, len);
  if (sent > 0) {
    LOG(LS_VERBOSE) << "TunnelSession::TcpWritePacket(" << sent << ") Sent";
    return IPseudoTcpNotify::WR_SUCCESS;
  } else if (cricket::IsBlockingError(socket_->GetError())) {
    LOG(LS_VERBOSE) << "TunnelSession::TcpWritePacket(" << sent << ") Blocking";
    return IPseudoTcpNotify::WR_BLOCKING;
  } else if (socket_->GetError() == EMSGSIZE) {
    LOG(LS_ERROR) << "TunnelSession::TcpWritePacket(" << sent << ") EMSGSIZE";
    ASSERT(false);
    return IPseudoTcpNotify::WR_TOO_LARGE;
  } else {
    PLOG(LS_ERROR, socket_->GetError()) << "TunnelSession::TcpWritePacket("
                                        << sent << ") Error";
    ASSERT(false);
    return IPseudoTcpNotify::WR_FAIL;
  }
}

void
TunnelSession::AdjustClock(bool clear) {
  ASSERT(cs_.CurrentThreadIsOwner());
  ASSERT(tcp_ != NULL);

  if (clear)
    worker_thread_->Clear(this, MSG_CLOCK);

  long timeout = 0;
  uint32 now = Time();
  if (tcp_->GetNextClock(now, timeout)) {
    //ASSERT(timeout > 0);
    worker_thread_->PostDelayed(_max(timeout, 0L),
                                                             this, MSG_CLOCK);
  } else {
    delete tcp_;
    tcp_ = NULL;
    if (session_)
      signal_thread_->Post(this, MSG_TERMINATE);
    CheckDestroy();
  }
}

void
TunnelSession::CheckDestroy() {
  ASSERT(cs_.CurrentThreadIsOwner());
  if ((session_ != NULL) || (stream_ != NULL) || (tcp_ != NULL))
    return;
  signal_thread_->Post(this, MSG_DESTROY);
}

///////////////////////////////////////////////////////////////////////////////

TunnelStream::TunnelStream(TunnelSession* tunnel) : tunnel_(tunnel) {
}

TunnelStream::~TunnelStream() {
  Close();
}

StreamState
TunnelStream::GetState() const {
  if (!tunnel_)
    return SS_CLOSED;
  return tunnel_->GetState();
}

StreamResult
TunnelStream::Read(void* buffer, size_t buffer_len,
                   size_t* read, int* error) {
  if (!tunnel_) {
    if (error)
      *error = ENOTCONN;
    return SR_ERROR;
  }
  return tunnel_->Read(buffer, buffer_len, read, error);
}

StreamResult
TunnelStream::Write(const void* data, size_t data_len,
                    size_t* written, int* error) {
  if (!tunnel_) {
    if (error)
      *error = ENOTCONN;
    return SR_ERROR;
  }
  return tunnel_->Write(data, data_len, written, error);
}

void
TunnelStream::Close() {
  if (!tunnel_)
    return;
  tunnel_->Close(); 
  tunnel_ = NULL;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace cricket

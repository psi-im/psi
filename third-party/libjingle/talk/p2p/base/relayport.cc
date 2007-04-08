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

#if defined(_MSC_VER) && _MSC_VER < 1300
#pragma warning(disable:4786)
#endif
#include "talk/base/logging.h"
#include "talk/base/asynctcpsocket.h"
#include "talk/p2p/base/relayport.h"
#include "talk/p2p/base/helpers.h"
#include <iostream>
#include <cassert>
#ifdef OSX
#include <errno.h>
#endif

#if defined(_MSC_VER) && _MSC_VER < 1300
namespace std {
  using ::strerror;
}
#endif

#ifdef POSIX
extern "C" {
#include <errno.h>
}
#endif // POSIX

namespace cricket {

const int KEEPALIVE_DELAY = 10 * 60 * 1000;
const int RETRY_DELAY = 50; // 50ms, from ICE spec
const uint32 RETRY_TIMEOUT = 50 * 1000; // ICE says 50 secs

const uint32 MSG_DISPOSE_SOCKET = 100; // needs to be more than ID used by Port
typedef TypedMessageData<AsyncPacketSocket *> DisposeSocketData;

class AsyncTCPSocket;

// Manages a single connection to the relayserver.  We aim to use each
// connection for only a specific destination address so that we can avoid
// wrapping every packet in a STUN send / data indication.
class RelayEntry : public sigslot::has_slots<> {
public:
  RelayEntry(RelayPort* port, const SocketAddress& ext_addr, const SocketAddress& local_addr);
  ~RelayEntry();

  RelayPort* port() { return port_; }

  const SocketAddress& address() { return ext_addr_; }
  void set_address(const SocketAddress& addr) { ext_addr_ = addr; }

  AsyncPacketSocket* socket() { return socket_; }

  bool connected() { return connected_; }
  void set_connected(bool connected) { connected_ = connected; }

  bool locked() { return locked_; }

  // Returns the last error on the socket of this entry.
  int GetError() { return socket_->GetError(); }

  // Sends the STUN requests to the server to initiate this connection.
  void Connect();

  // Called when this entry becomes connected.  The address given is the one
  // exposed to the outside world on the relay server.
  void OnConnect(const SocketAddress& mapped_addr);

  // Sends a packet to the given destination address using the socket of this
  // entry.  This will wrap the packet in STUN if necessary.
  int SendTo(const void* data, size_t size, const SocketAddress& addr);

  // Schedules a keep-alive allocate request.
  void ScheduleKeepAlive();

  void SetServerIndex(size_t sindex) { server_index_ = sindex; }
  size_t ServerIndex() const { return server_index_; }

  // Try a different server address
  void HandleConnectFailure();

private:
  RelayPort* port_;
  SocketAddress ext_addr_, local_addr_;
  size_t server_index_;
  AsyncPacketSocket* socket_;
  bool connected_;
  bool locked_;
  StunRequestManager requests_;

  // Called when a TCP connection is established or fails
  void OnSocketConnect(AsyncTCPSocket* socket);
  void OnSocketClose(AsyncTCPSocket* socket, int error);

  // Called when a packet is received on this socket.
  void OnReadPacket(
      const char* data, size_t size, const SocketAddress& remote_addr,
      AsyncPacketSocket* socket);

  // Called on behalf of a StunRequest to write data to the socket.  This is
  // already STUN intended for the server, so no wrapping is necessary.
  void OnSendPacket(const void* data, size_t size);

  // Sends the given data on the socket to the server with no wrapping.  This
  // returns the number of bytes written or -1 if an error occurred.
  int SendPacket(const void* data, size_t size);
};

// Handles an allocate request for a particular RelayEntry.
class AllocateRequest : public StunRequest {
public:
  AllocateRequest(RelayEntry* entry);
  virtual ~AllocateRequest() {}

  virtual void Prepare(StunMessage* request);

  virtual int GetNextDelay();

  virtual void OnResponse(StunMessage* response);
  virtual void OnErrorResponse(StunMessage* response);
  virtual void OnTimeout();

private:
  RelayEntry* entry_;
  uint32 start_time_;
};

const std::string RELAY_PORT_TYPE("relay");

RelayPort::RelayPort(
    Thread* thread, SocketFactory* factory, Network* network,
    const SocketAddress& local_addr, const std::string& username,
    const std::string& password, const std::string& magic_cookie)
  : Port(thread, RELAY_PORT_TYPE, factory, network), local_addr_(local_addr),
    ready_(false), magic_cookie_(magic_cookie), error_(0) {

  entries_.push_back(new RelayEntry(this, SocketAddress(), local_addr_));

  set_username_fragment(username);
  set_password(password);

  if (magic_cookie_.size() == 0)
    magic_cookie_.append(STUN_MAGIC_COOKIE_VALUE, 4);
}

RelayPort::~RelayPort() {
  for (unsigned i = 0; i < entries_.size(); ++i)
    delete entries_[i];
  thread_->Clear(this);
}

void RelayPort::AddServerAddress(const ProtocolAddress& addr) {
  // Since HTTP proxies usually only allow 443, let's up the priority on PROTO_SSLTCP
  if ((addr.proto == PROTO_SSLTCP)
      && ((proxy().type == PROXY_HTTPS) || (proxy().type == PROXY_UNKNOWN))) {
    server_addr_.push_front(addr);
  } else {
    server_addr_.push_back(addr);
  }
}

void RelayPort::AddExternalAddress(const ProtocolAddress& addr) {
  std::string proto_name = ProtoToString(addr.proto);
  for (std::vector<Candidate>::const_iterator it = candidates().begin(); it != candidates().end(); ++it) {
    if ((it->address() == addr.address) && (it->protocol() == proto_name)) {
      LOG(INFO) << "Redundant relay address: " << proto_name << " @ " << addr.address.ToString();
      return;
    }
  }
  add_address(addr.address, proto_name, false);
}

void RelayPort::SetReady() {
  if (!ready_) {
    ready_ = true;
    SignalAddressReady(this);
  }
}

const ProtocolAddress * RelayPort::ServerAddress(size_t index) const {
  if ((index >= 0) && (index < server_addr_.size()))
    return &server_addr_[index];
  return 0;
}

bool RelayPort::HasMagicCookie(const char* data, size_t size) {
  if (size < 24 + magic_cookie_.size()) {
    return false;
  } else {
    return 0 == std::memcmp(data + 24,
                            magic_cookie_.c_str(),
                            magic_cookie_.size());
  }
}

void RelayPort::PrepareAddress() {
  // We initiate a connect on the first entry.  If this completes, it will fill
  // in the server address as the address of this port.
  assert(entries_.size() == 1);
  entries_[0]->Connect();
  ready_ = false;
}

Connection* RelayPort::CreateConnection(const Candidate& address, CandidateOrigin origin) {
  // We only create connections to non-udp sockets if they are incoming on this port
  if ((address.protocol() != "udp") && (origin != ORIGIN_THIS_PORT))
    return 0;

  // We don't support loopback on relays
  if (address.type() == type())
    return 0;

  size_t index = 0;
  for (size_t i = 0; i < candidates().size(); ++i) {
    const Candidate& local = candidates()[i];
    if (local.protocol() == address.protocol()) {
      index = i;
      break;
    }
  }

  Connection * conn = new ProxyConnection(this, index, address);
  AddConnection(conn);
  return conn;
}

int RelayPort::SendTo(const void* data,
                      size_t size,
                      const SocketAddress& addr, bool payload) {

  // Try to find an entry for this specific address.  Note that the first entry
  // created was not given an address initially, so it can be set to the first
  // address that comes along.

  RelayEntry* entry = 0;

  for (unsigned i = 0; i < entries_.size(); ++i) {
    if (entries_[i]->address().IsAny() && payload) {
      entry = entries_[i];
      entry->set_address(addr);
      break;
    } else if (entries_[i]->address() == addr) {
      entry = entries_[i];
      break;
    }
  }

  // If we did not find one, then we make a new one.  This will not be useable
  // until it becomes connected, however.
  if (!entry && payload) {
    entry = new RelayEntry(this, addr, local_addr_);
    if (!entries_.empty()) {
      // Use the same port to connect to relay server
      entry->SetServerIndex(entries_[0]->ServerIndex());
    }
    entry->Connect();
    entries_.push_back(entry);
  }

  // If the entry is connected, then we can send on it (though wrapping may
  // still be necessary).  Otherwise, we can't yet use this connection, so we
  // default to the first one.
  if (!entry || !entry->connected()) {
    assert(!entries_.empty());
    entry = entries_[0];
    if (!entry->connected()) {
      error_ = EWOULDBLOCK;
      return SOCKET_ERROR;
    }
  }

  // Send the actual contents to the server using the usual mechanism.
  int sent = entry->SendTo(data, size, addr);
  if (sent <= 0) {
    assert(sent < 0);
    error_ = entry->GetError();
    return SOCKET_ERROR;
  }

  // The caller of the function is expecting the number of user data bytes,
  // rather than the size of the packet.
  return (int)size;
}

void RelayPort::OnMessage(Message *pmsg) {
  switch (pmsg->message_id) {
  case MSG_DISPOSE_SOCKET: {
    DisposeSocketData * data = static_cast<DisposeSocketData *>(pmsg->pdata);
    delete data->data();
    delete data;
    break; }
  default:
    Port::OnMessage(pmsg);
  }
}

int RelayPort::SetOption(Socket::Option opt, int value) {
  int result = 0;
  for (unsigned i = 0; i < entries_.size(); ++i) {
    if (entries_[i]->socket()->SetOption(opt, value) < 0) {
      result = -1;
      error_ = entries_[i]->socket()->GetError();
    }
  }
  options_.push_back(OptionValue(opt, value));
  return result;
}

int RelayPort::GetError() {
  return error_;
}

void RelayPort::OnReadPacket(
    const char* data, size_t size, const SocketAddress& remote_addr) {
  if (Connection* conn = GetConnection(remote_addr)) {
    conn->OnReadPacket(data, size);
  } else {
    Port::OnReadPacket(data, size, remote_addr);
  }
}

void RelayPort::DisposeSocket(AsyncPacketSocket * socket) {
  thread_->Post(this, MSG_DISPOSE_SOCKET, new DisposeSocketData(socket));
}

RelayEntry::RelayEntry(RelayPort* port, const SocketAddress& ext_addr,
                       const SocketAddress& local_addr)
  : port_(port), ext_addr_(ext_addr), local_addr_(local_addr), server_index_(0),
    socket_(0), connected_(false), locked_(false), requests_(port->thread()) {

  requests_.SignalSendPacket.connect(this, &RelayEntry::OnSendPacket);
}

RelayEntry::~RelayEntry() {
  delete socket_;
}

void RelayEntry::Connect() {
  assert(socket_ == 0);
  const ProtocolAddress * ra = port()->ServerAddress(server_index_);
  if (!ra) {
    LOG(INFO) << "Out of relay server connections";
    return;
  }

  LOG(INFO) << "Connecting to relay via " << ProtoToString(ra->proto) << " @ " << ra->address.ToString();

  socket_ = port_->CreatePacketSocket(ra->proto);
  assert(socket_ != 0);

  socket_->SignalReadPacket.connect(this, &RelayEntry::OnReadPacket);
  if (socket_->Bind(local_addr_) < 0)
    LOG(INFO) << "bind: " << std::strerror(socket_->GetError());

  for (unsigned i = 0; i < port_->options().size(); ++i)
    socket_->SetOption(port_->options()[i].first, port_->options()[i].second);

  if ((ra->proto == PROTO_TCP) || (ra->proto == PROTO_SSLTCP)) {
    AsyncTCPSocket * tcp = static_cast<AsyncTCPSocket *>(socket_);
    tcp->SignalClose.connect(this, &RelayEntry::OnSocketClose);
    tcp->SignalConnect.connect(this, &RelayEntry::OnSocketConnect);
    tcp->Connect(ra->address);
  } else {
    requests_.Send(new AllocateRequest(this));
  }
}

void RelayEntry::OnConnect(const SocketAddress& mapped_addr) {
  ProtocolType proto = PROTO_UDP;
  LOG(INFO) << "Relay allocate succeeded: " << ProtoToString(proto) << " @ " << mapped_addr.ToString();
  connected_ = true;

  port_->AddExternalAddress(ProtocolAddress(mapped_addr, proto));
  port_->SetReady();
}

int RelayEntry::SendTo(const void* data,
                        size_t size,
                        const SocketAddress& addr) {

  // If this connection is locked to the address given, then we can send the
  // packet with no wrapper.
  if (locked_ && (ext_addr_ == addr))
    return SendPacket(data, size);

  // Otherwise, we must wrap the given data in a STUN SEND request so that we
  // can communicate the destination address to the server.
  //
  // Note that we do not use a StunRequest here.  This is because there is
  // likely no reason to resend this packet. If it is late, we just drop it.
  // The next send to this address will try again.

  StunMessage request;
  request.SetType(STUN_SEND_REQUEST);
  request.SetTransactionID(CreateRandomString(16));

  StunByteStringAttribute* magic_cookie_attr =
      StunAttribute::CreateByteString(STUN_ATTR_MAGIC_COOKIE);
  magic_cookie_attr->CopyBytes(port_->magic_cookie().c_str(),
                               (uint16)port_->magic_cookie().size());
  request.AddAttribute(magic_cookie_attr);

  StunByteStringAttribute* username_attr =
      StunAttribute::CreateByteString(STUN_ATTR_USERNAME);
  username_attr->CopyBytes(port_->username_fragment().c_str(),
                           (uint16)port_->username_fragment().size());
  request.AddAttribute(username_attr);

  StunAddressAttribute* addr_attr =
      StunAttribute::CreateAddress(STUN_ATTR_DESTINATION_ADDRESS);
  addr_attr->SetFamily(1);
  addr_attr->SetIP(addr.ip());
  addr_attr->SetPort(addr.port());
  request.AddAttribute(addr_attr);

  // Attempt to lock
  if (ext_addr_ == addr) {
    StunUInt32Attribute* options_attr =
      StunAttribute::CreateUInt32(STUN_ATTR_OPTIONS);
    options_attr->SetValue(0x1);
    request.AddAttribute(options_attr);
  }

  StunByteStringAttribute* data_attr =
      StunAttribute::CreateByteString(STUN_ATTR_DATA);
  data_attr->CopyBytes(data, (uint16)size);
  request.AddAttribute(data_attr);

  // TODO: compute the HMAC.

  ByteBuffer buf;
  request.Write(&buf);

  return SendPacket(buf.Data(), buf.Length());
}

void RelayEntry::ScheduleKeepAlive() {
  requests_.SendDelayed(new AllocateRequest(this), KEEPALIVE_DELAY);
}

void RelayEntry::HandleConnectFailure() {
  //if (GetMillisecondCount() - start_time_ > RETRY_TIMEOUT)
  //  return;
  //ScheduleKeepAlive();

  connected_ = false;
  port()->DisposeSocket(socket_);
  socket_ = 0;
  requests_.Clear();

  server_index_ += 1;
  Connect();
}

void RelayEntry::OnSocketConnect(AsyncTCPSocket* socket) {
  assert(socket == socket_);
  LOG(INFO) << "relay tcp connected to " << socket->GetRemoteAddress().ToString();
  requests_.Send(new AllocateRequest(this));
}

void RelayEntry::OnSocketClose(AsyncTCPSocket* socket, int error) {
  assert(socket == socket_);
  PLOG(LERROR, error) << "relay tcp connect failed";
  HandleConnectFailure();
}

void RelayEntry::OnReadPacket(const char* data,
                              size_t size,
                              const SocketAddress& remote_addr,
                              AsyncPacketSocket* socket) {
  assert(socket == socket_);
  //assert(remote_addr == port_->server_addr()); TODO: are we worried about this?

  // If the magic cookie is not present, then this is an unwrapped packet sent
  // by the server,  The actual remote address is the one we recorded.
  if (!port_->HasMagicCookie(data, size)) {
    if (locked_) {
      port_->OnReadPacket(data, size, ext_addr_);
    } else {
      LOG(WARNING) << "Dropping packet: entry not locked";
    }
    return;
  }

  ByteBuffer buf(data, size);
  StunMessage msg;
  if (!msg.Read(&buf)) {
    LOG(INFO) << "Incoming packet was not STUN";
    return;
  }

  // The incoming packet should be a STUN ALLOCATE response, SEND response, or
  // DATA indication.
  if (requests_.CheckResponse(&msg)) {
    return;
  } else if (msg.type() == STUN_SEND_RESPONSE) {
    if (const StunUInt32Attribute* options_attr = msg.GetUInt32(STUN_ATTR_OPTIONS)) {
      if (options_attr->value() & 0x1) {
        locked_ = true;
      }
    }
    return;
  } else if (msg.type() != STUN_DATA_INDICATION) {
    LOG(INFO) << "Received BAD stun type from server: " << msg.type()
             ;
    return;
  }

  // This must be a data indication.

  const StunAddressAttribute* addr_attr =
      msg.GetAddress(STUN_ATTR_SOURCE_ADDRESS2);
  if (!addr_attr) {
    LOG(INFO) << "Data indication has no source address";
    return;
  } else if (addr_attr->family() != 1) {
    LOG(INFO) << "Source address has bad family";
    return;
  }

  SocketAddress remote_addr2(addr_attr->ip(), addr_attr->port());

  const StunByteStringAttribute* data_attr = msg.GetByteString(STUN_ATTR_DATA);
  if (!data_attr) {
    LOG(INFO) << "Data indication has no data";
    return;
  }

  // Process the actual data and remote address in the normal manner.
  port_->OnReadPacket(data_attr->bytes(), data_attr->length(), remote_addr2);
}

void RelayEntry::OnSendPacket(const void* data, size_t size) {
  SendPacket(data, size);
}

int RelayEntry::SendPacket(const void* data, size_t size) {
  const ProtocolAddress * ra = port_->ServerAddress(server_index_);
  if (!ra) {
    if (socket_)
      socket_->SetError(ENOTCONN);
    return SOCKET_ERROR;
  }
  int sent = socket_->SendTo(data, size, ra->address);
  if (sent <= 0) {
    LOG(LS_VERBOSE) << "sendto: " << std::strerror(socket_->GetError());
    assert(sent < 0);
  }
  return sent;
}

AllocateRequest::AllocateRequest(RelayEntry* entry) : entry_(entry) {
  start_time_ = GetMillisecondCount();
}

void AllocateRequest::Prepare(StunMessage* request) {
  request->SetType(STUN_ALLOCATE_REQUEST);

  StunByteStringAttribute* magic_cookie_attr =
      StunAttribute::CreateByteString(STUN_ATTR_MAGIC_COOKIE);
  magic_cookie_attr->CopyBytes(
      entry_->port()->magic_cookie().c_str(),
      (uint16)entry_->port()->magic_cookie().size());
  request->AddAttribute(magic_cookie_attr);

  StunByteStringAttribute* username_attr =
      StunAttribute::CreateByteString(STUN_ATTR_USERNAME);
  username_attr->CopyBytes(
      entry_->port()->username_fragment().c_str(),
      (uint16)entry_->port()->username_fragment().size());
  request->AddAttribute(username_attr);
}

int AllocateRequest::GetNextDelay() {
  int delay = 100 * _max(1 << count_, 2);
  count_ += 1;
  if (count_ == 5)
    timeout_ = true;
  return delay;
}

void AllocateRequest::OnResponse(StunMessage* response) {
  const StunAddressAttribute* addr_attr =
      response->GetAddress(STUN_ATTR_MAPPED_ADDRESS);
  if (!addr_attr) {
    LOG(INFO) << "Allocate response missing mapped address.";
  } else if (addr_attr->family() != 1) {
    LOG(INFO) << "Mapped address has bad family";
  } else {
    SocketAddress addr(addr_attr->ip(), addr_attr->port());
    entry_->OnConnect(addr);
  }

  // We will do a keep-alive regardless of whether this request suceeds.
  // This should have almost no impact on network usage.
  entry_->ScheduleKeepAlive();
}

void AllocateRequest::OnErrorResponse(StunMessage* response) {
  const StunErrorCodeAttribute* attr = response->GetErrorCode();
  if (!attr) {
    LOG(INFO) << "Bad allocate response error code";
  } else {
    LOG(INFO) << "Allocate error response:"
              << " code=" << static_cast<int>(attr->error_code())
              << " reason='" << attr->reason() << "'";
  }

  if (GetMillisecondCount() - start_time_ <= RETRY_TIMEOUT)
    entry_->ScheduleKeepAlive();
}

void AllocateRequest::OnTimeout() {
  LOG(INFO) << "Allocate request timed out";
  entry_->HandleConnectFailure();
}

} // namespace cricket

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
#include "talk/p2p/base/tcpport.h"
#include "talk/base/logging.h"
#ifdef WIN32
#include "talk/base/winfirewall.h"
#endif // WIN32
#include <iostream>
#include <cassert>

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

#ifdef WIN32
static WinFirewall win_firewall;
#endif  // WIN32

TCPPort::TCPPort(Thread* thread, SocketFactory* factory, Network* network,
                 const SocketAddress& address)
    : Port(thread, LOCAL_PORT_TYPE, factory, network), error_(0) {
  incoming_only_ = (address.port() != 0);
  socket_ = thread->socketserver()->CreateAsyncSocket(SOCK_STREAM);
  socket_->SignalReadEvent.connect(this, &TCPPort::OnAcceptEvent);
  if (socket_->Bind(address) < 0)
    LOG(INFO) << "bind: " << std::strerror(socket_->GetError());
}

TCPPort::~TCPPort() {
  delete socket_;
}

Connection* TCPPort::CreateConnection(const Candidate& address, CandidateOrigin origin) {
  // We only support TCP protocols
  if ((address.protocol() != "tcp") && (address.protocol() != "ssltcp"))
    return 0;

  // We can't accept TCP connections incoming on other ports
  if (origin == ORIGIN_OTHER_PORT)
    return 0;

  // Check if we are allowed to make outgoing TCP connections
  if (incoming_only_ && (origin == ORIGIN_MESSAGE))
    return 0;

  // We don't know how to act as an ssl server yet
  if ((address.protocol() == "ssltcp") && (origin == ORIGIN_THIS_PORT))
    return 0;

  TCPConnection* conn = 0;
  if (AsyncTCPSocket * socket = GetIncoming(address.address(), true)) {
    socket->SignalReadPacket.disconnect(this);
    conn = new TCPConnection(this, address, socket);
  } else {
    conn = new TCPConnection(this, address);
  }
  AddConnection(conn);
  return conn;
}

void TCPPort::PrepareAddress() {
  assert(socket_);

  bool allow_listen = true;
#ifdef WIN32
  if (win_firewall.Initialize()) {
    char module_path[MAX_PATH + 1] = { 0 };
    ::GetModuleFileNameA(NULL, module_path, MAX_PATH);
    if (win_firewall.Enabled() && !win_firewall.Authorized(module_path)) {
      allow_listen = false;
    }
  }
#endif // WIN32
  if (allow_listen) {
    if (socket_->Listen(5) < 0)
      LOG(INFO) << "listen: " << std::strerror(socket_->GetError());
  } else {
    LOG(INFO) << "not listening due to firewall restrictions";
  }
  // Note: We still add the address, since otherwise the remote side won't recognize
  // our incoming TCP connections.
  add_address(socket_->GetLocalAddress(), "tcp");
}

int TCPPort::SendTo(const void* data, size_t size, const SocketAddress& addr, bool payload) {
  AsyncTCPSocket * socket = 0;

  if (TCPConnection * conn = static_cast<TCPConnection*>(GetConnection(addr))) {
    socket = conn->socket();
  } else {
    socket = GetIncoming(addr);
  }
  if (!socket) {
    LOG(INFO) << "Unknown destination for SendTo: " << addr.ToString();
    return -1; // TODO: Set error_
  }

  //LOG(INFO) << "TCPPort::SendTo(" << size << ", " << addr.ToString() << ")";

  int sent = socket->Send(data, size);
  if (sent < 0)
    error_ = socket->GetError();
  return sent;
}

int TCPPort::SetOption(Socket::Option opt, int value) {
  return socket_->SetOption(opt, value);
}

int TCPPort::GetError() {
  assert(socket_);
  return error_;
}

void TCPPort::OnAcceptEvent(AsyncSocket* socket) {
  assert(socket == socket_);

  Incoming incoming;
  AsyncSocket * newsocket = static_cast<AsyncSocket *>(socket->Accept(&incoming.addr));
  if (!newsocket) {
    // TODO: Do something better like forwarding the error to the user.
    LOG(INFO) << "accept: " << socket_->GetError() << " " <<  std::strerror(socket_->GetError());
    return;
  }
  incoming.socket = new AsyncTCPSocket(newsocket);
  incoming.socket->SignalReadPacket.connect(this, &TCPPort::OnReadPacket);

  LOG(INFO) << "accepted incoming connection from " << incoming.addr.ToString();
  incoming_.push_back(incoming);

  // Prime a read event in case data is waiting
  newsocket->SignalReadEvent(newsocket);
}

AsyncTCPSocket * TCPPort::GetIncoming(const SocketAddress& addr, bool remove) {
  AsyncTCPSocket * socket = 0;
  for (std::list<Incoming>::iterator it = incoming_.begin(); it != incoming_.end(); ++it) {
    if (it->addr == addr) {
      socket = it->socket;
      if (remove)
        incoming_.erase(it);
      break;
    }
  }
  return socket;
}

void TCPPort::OnReadPacket(const char* data, size_t size, const SocketAddress& remote_addr,
      AsyncPacketSocket* socket) {
  Port::OnReadPacket(data, size, remote_addr);
}

TCPConnection::TCPConnection(TCPPort* port, const Candidate& candidate, AsyncTCPSocket* socket)
    : Connection(port, 0, candidate), socket_(socket), error_(0) {
  bool outgoing = (socket_ == 0);
  if (outgoing) {
    socket_ = static_cast<AsyncTCPSocket *>(port->CreatePacketSocket(
                (candidate.protocol() == "ssltcp") ? PROTO_SSLTCP : PROTO_TCP));
  }
  socket_->SignalReadPacket.connect(this, &TCPConnection::OnReadPacket);
  socket_->SignalClose.connect(this, &TCPConnection::OnClose);
  if (outgoing) {
    connected_ = false;
    socket_->SignalConnect.connect(this, &TCPConnection::OnConnect);
    socket_->Connect(candidate.address());
    LOG(INFO) << "Connecting to " << candidate.address().ToString();
  }
}

TCPConnection::~TCPConnection() {
}

int TCPConnection::Send(const void* data, size_t size) {
  if (write_state() != STATE_WRITABLE)
    return 0;

  int sent = socket_->Send(data, size);
  if (sent < 0) {
    error_ = socket_->GetError();
  } else {
    sent_total_bytes_ += sent;
  }
  return sent;
}

int TCPConnection::GetError() {
  return error_;
}

TCPPort* TCPConnection::tcpport() {
  return static_cast<TCPPort*>(port_);
}

void TCPConnection::OnConnect(AsyncTCPSocket* socket) {
  assert(socket == socket_);
  LOG(INFO) << "tcp connected to " << socket->GetRemoteAddress().ToString();
  set_connected(true);
}

void TCPConnection::OnClose(AsyncTCPSocket* socket, int error) {
  assert(socket == socket_);
  LOG(INFO) << "tcp closed with error: " << error;
  set_connected(false);
}

void TCPConnection::OnReadPacket(const char* data, size_t size, const SocketAddress& remote_addr,
      AsyncPacketSocket* socket) {
  assert(socket == socket_);
  Connection::OnReadPacket(data, size);
}

} // namespace cricket

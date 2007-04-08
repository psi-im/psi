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
#include "talk/p2p/base/udpport.h"
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

const std::string LOCAL_PORT_TYPE("local");

UDPPort::UDPPort(Thread* thread, SocketFactory* factory, Network* network,
                 const SocketAddress& address)
    : Port(thread, LOCAL_PORT_TYPE, factory, network), error_(0) {
  socket_ = CreatePacketSocket(PROTO_UDP);
  socket_->SignalReadPacket.connect(this, &UDPPort::OnReadPacketSlot);
  if (socket_->Bind(address) < 0)
    PLOG(LERROR, socket_->GetError()) << "bind";
}

UDPPort::UDPPort(Thread* thread, const std::string &type,
                 SocketFactory* factory, Network* network)
  : Port(thread, type, factory, network), socket_(0), error_(0) {
}

UDPPort::~UDPPort() {
  delete socket_;
}

void UDPPort::PrepareAddress() {
  assert(socket_);
  add_address(socket_->GetLocalAddress(), "udp");
}

Connection* UDPPort::CreateConnection(const Candidate& address, CandidateOrigin origin) {
  if (address.protocol() != "udp")
    return 0;

  Connection * conn = new ProxyConnection(this, 0, address);
  AddConnection(conn);
  return conn;
}

int UDPPort::SendTo(const void* data, size_t size, const SocketAddress& addr, bool payload) {
  assert(socket_);
  int sent = socket_->SendTo(data, size, addr);
  if (sent < 0)
    error_ = socket_->GetError();
  return sent;
}

int UDPPort::SetOption(Socket::Option opt, int value) {
  return socket_->SetOption(opt, value);
}

int UDPPort::GetError() {
  assert(socket_);
  return error_;
}

void UDPPort::OnReadPacketSlot(
    const char* data, size_t size, const SocketAddress& remote_addr,
    AsyncPacketSocket* socket) {
  assert(socket == socket_);
  OnReadPacket(data, size, remote_addr);
}

void UDPPort::OnReadPacket(
    const char* data, size_t size, const SocketAddress& remote_addr) {
  if (Connection* conn = GetConnection(remote_addr)) {
    conn->OnReadPacket(data, size);
  } else {
    Port::OnReadPacket(data, size, remote_addr);
  }
}

} // namespace cricket

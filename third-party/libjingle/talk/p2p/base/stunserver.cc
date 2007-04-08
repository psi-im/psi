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

#include "talk/base/bytebuffer.h"
#include "talk/p2p/base/stunserver.h"
#include <iostream>

#ifdef POSIX
extern "C" {
#include <errno.h>
}
#endif // POSIX

namespace cricket {

StunServer::StunServer(AsyncUDPSocket* socket) : socket_(socket) {
  socket_->SignalReadPacket.connect(this, &StunServer::OnPacket);
}

StunServer::~StunServer() {
  socket_->SignalReadPacket.disconnect(this);
}

void StunServer::OnPacket(
    const char* buf, size_t size, const SocketAddress& remote_addr,
    AsyncPacketSocket* socket) {

  // TODO: If appropriate, look for the magic cookie before parsing.

  // Parse the STUN message.
  ByteBuffer bbuf(buf, size);
  StunMessage msg;
  if (!msg.Read(&bbuf)) {
    SendErrorResponse(msg, remote_addr, 400, "Bad Request");
    return;
  }

  // TODO: If this is UDP, then we shouldn't allow non-fully-parsed messages.

  // TODO: If unknown non-optiional (<= 0x7fff) attributes are found, send a
  //       420 "Unknown Attribute" response.

  // TODO: Check that a message-integrity attribute was given (or send 401
  //       "Unauthorized").  Check that a username attribute was given (or send
  //       432 "Missing Username").  Look up the username and password.  If it
  //       is missing or the HMAC is wrong, send 431 "Integrity Check Failure".

  // Send the message to the appropriate handler function.
  switch (msg.type()) {
  case STUN_BINDING_REQUEST:
    OnBindingRequest(&msg, remote_addr);
    return;

  case STUN_ALLOCATE_REQUEST:
    OnAllocateRequest(&msg, remote_addr);
    return;

  default:
    SendErrorResponse(msg, remote_addr, 600, "Operation Not Supported");
  }
}

void StunServer::OnBindingRequest(
    StunMessage* msg, const SocketAddress& remote_addr) {
  StunMessage response;
  response.SetType(STUN_BINDING_RESPONSE);
  response.SetTransactionID(msg->transaction_id());

  // Tell the user the address that we received their request from.
  StunAddressAttribute* mapped_addr =
      StunAttribute::CreateAddress(STUN_ATTR_MAPPED_ADDRESS);
  mapped_addr->SetFamily(1);
  mapped_addr->SetPort(remote_addr.port());
  mapped_addr->SetIP(remote_addr.ip());
  response.AddAttribute(mapped_addr);

  // Tell the user the address that we are sending the response from.
  SocketAddress local_addr = socket_->GetLocalAddress();
  StunAddressAttribute* source_addr =
      StunAttribute::CreateAddress(STUN_ATTR_SOURCE_ADDRESS);
  source_addr->SetFamily(1);
  source_addr->SetPort(local_addr.port());
  source_addr->SetIP(local_addr.ip());
  response.AddAttribute(source_addr);

  // TODO: Add username and message-integrity.

  // TODO: Add changed-address.  (Keep information about three other servers.)

  SendResponse(response, remote_addr);
}

void StunServer::OnAllocateRequest(
    StunMessage* msg, const SocketAddress& addr) {
  SendErrorResponse(*msg, addr, 600, "Operation Not Supported");
}

void StunServer::OnSharedSecretRequest(
    StunMessage* msg, const SocketAddress& addr) {
  SendErrorResponse(*msg, addr, 600, "Operation Not Supported");
}

void StunServer::OnSendRequest(StunMessage* msg, const SocketAddress& addr) {
  SendErrorResponse(*msg, addr, 600, "Operation Not Supported");
}

void StunServer::SendErrorResponse(
    const StunMessage& msg, const SocketAddress& addr, int error_code,
    const char* error_desc) {

  StunMessage err_msg;
  err_msg.SetType(GetStunErrorResponseType(msg.type()));
  err_msg.SetTransactionID(msg.transaction_id());

  StunErrorCodeAttribute* err_code = StunAttribute::CreateErrorCode();
  err_code->SetErrorClass(error_code / 100);
  err_code->SetNumber(error_code % 100);
  err_code->SetReason(error_desc);
  err_msg.AddAttribute(err_code);

  SendResponse(err_msg, addr);
}

void StunServer::SendResponse(
    const StunMessage& msg, const SocketAddress& addr) {

  ByteBuffer buf;
  msg.Write(&buf);

  // TODO: Allow response addr attribute if sent from another stun server.

  if (socket_->SendTo(buf.Data(), buf.Length(), addr) < 0)
    std::cerr << "sendto: " << std::strerror(errno) << std::endl;
}

} // namespace cricket

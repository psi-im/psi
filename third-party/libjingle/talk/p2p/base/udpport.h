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

#ifndef __UDPPORT_H__
#define __UDPPORT_H__

#include "talk/base/asyncudpsocket.h"
#include "talk/p2p/base/port.h"

namespace cricket {

extern const std::string LOCAL_PORT_TYPE; // type of UDP ports

// Communicates using a local UDP port.
//
// This class is designed to allow subclasses to take advantage of the
// connection management provided by this class.  A subclass should take of all
// packet sending and preparation, but when a packet is received, it should
// call this UDPPort::OnReadPacket (3 arg) to dispatch to a connection.
class UDPPort : public Port {
public:
  UDPPort(Thread* thread, SocketFactory* factory, Network* network,
          const SocketAddress& address);
  virtual ~UDPPort();

  virtual void PrepareAddress();
  virtual Connection* CreateConnection(const Candidate& address, CandidateOrigin origin);

  virtual int SetOption(Socket::Option opt, int value);
  virtual int GetError();

protected:
  UDPPort(Thread* thread, const std::string &type, SocketFactory* factory,
          Network* network);

  // Handles sending using the local UDP socket.
  virtual int SendTo(const void* data, size_t size, const SocketAddress& addr, bool payload);

  // Dispatches the given packet to the port or connection as appropriate.
  void OnReadPacket(
      const char* data, size_t size, const SocketAddress& remote_addr);

  AsyncPacketSocket* socket() { return socket_; }

private:
  AsyncPacketSocket* socket_;
  int error_;

  // Receives packet signal from the local UDP Socket.
  void OnReadPacketSlot(
      const char* data, size_t size, const SocketAddress& remote_addr,
      AsyncPacketSocket* socket);
};

} // namespace cricket

#endif // __UDPPORT_H__

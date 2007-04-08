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

#ifndef __TCPPORT_H__
#define __TCPPORT_H__

#include <list>
#include "talk/base/asynctcpsocket.h"
#include "talk/p2p/base/port.h"

namespace cricket {

class TCPConnection;

extern const std::string LOCAL_PORT_TYPE; // type of TCP ports

// Communicates using a local TCP port.
//
// This class is designed to allow subclasses to take advantage of the
// connection management provided by this class.  A subclass should take of all
// packet sending and preparation, but when a packet is received, it should
// call this TCPPort::OnReadPacket (3 arg) to dispatch to a connection.
class TCPPort : public Port {
public:
  TCPPort(Thread* thread, SocketFactory* factory, Network* network,
          const SocketAddress& address);
  virtual ~TCPPort();

  virtual Connection* CreateConnection(const Candidate& address, CandidateOrigin origin);

  virtual void PrepareAddress();

  virtual int SetOption(Socket::Option opt, int value);
  virtual int GetError();

protected:
  // Handles sending using the local TCP socket.
  virtual int SendTo(const void* data, size_t size, const SocketAddress& addr, bool payload);

  // Creates TCPConnection for incoming sockets
  void OnAcceptEvent(AsyncSocket* socket);

  AsyncSocket* socket() { return socket_; }

private:
  bool incoming_only_;
  AsyncSocket* socket_;
  int error_;

  struct Incoming {
    SocketAddress addr;
    AsyncTCPSocket * socket;
  };
  std::list<Incoming> incoming_;

  AsyncTCPSocket * GetIncoming(const SocketAddress& addr, bool remove = false);

  // Receives packet signal from the local TCP Socket.
  void OnReadPacket(const char* data, size_t size, const SocketAddress& remote_addr,
      AsyncPacketSocket* socket);

  friend class TCPConnection;
};

class TCPConnection : public Connection {
public:
  // Connection is outgoing unless socket is specified
  TCPConnection(TCPPort* port, const Candidate& candidate, AsyncTCPSocket* socket = 0);
  virtual ~TCPConnection();

  virtual int Send(const void* data, size_t size);
  virtual int GetError();

  AsyncTCPSocket * socket() { return socket_; }

private:
  TCPPort* tcpport();
  AsyncTCPSocket* socket_;
  bool connected_;
  int error_;

  void OnConnect(AsyncTCPSocket* socket);
  void OnClose(AsyncTCPSocket* socket, int error);
  void OnReadPacket(const char* data, size_t size, const SocketAddress& remote_addr,
      AsyncPacketSocket* socket);

  friend class TCPPort;
};

} // namespace cricket

#endif // __TCPPORT_H__

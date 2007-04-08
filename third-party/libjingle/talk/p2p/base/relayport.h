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

#ifndef __RELAYPORT_H__
#define __RELAYPORT_H__

#include "talk/p2p/base/port.h"
#include "talk/p2p/base/stunrequest.h"
#include <vector>

namespace cricket {

extern const std::string RELAY_PORT_TYPE;
class RelayEntry;

// Communicates using an allocated port on the relay server.
class RelayPort: public Port {
public:
  RelayPort(
      Thread* thread, SocketFactory* factory, Network*,
      const SocketAddress& local_addr,
      const std::string& username, const std::string& password,
      const std::string& magic_cookie);
  virtual ~RelayPort();

  void AddServerAddress(const ProtocolAddress& addr);
  void AddExternalAddress(const ProtocolAddress& addr);

  typedef std::pair<Socket::Option, int> OptionValue;
  const std::vector<OptionValue>& options() const { return options_; }

  const std::string& magic_cookie() const { return magic_cookie_; }
  bool HasMagicCookie(const char* data, size_t size);

  virtual void PrepareAddress();
  virtual Connection* CreateConnection(const Candidate& address, CandidateOrigin origin);

  virtual int SetOption(Socket::Option opt, int value);
  virtual int GetError();

  const ProtocolAddress * ServerAddress(size_t index) const;

  void DisposeSocket(AsyncPacketSocket * socket);

protected:
  void SetReady();

  virtual int SendTo(const void* data, size_t size, const SocketAddress& addr, bool payload);
  virtual void OnMessage(Message *pmsg);

  // Dispatches the given packet to the port or connection as appropriate.
  void OnReadPacket(
      const char* data, size_t size, const SocketAddress& remote_addr);

private:
  friend class RelayEntry;

  SocketAddress local_addr_;
  std::deque<ProtocolAddress> server_addr_;
  bool ready_;
  std::vector<RelayEntry*> entries_;
  std::vector<OptionValue> options_;
  std::string magic_cookie_;
  int error_;
};

} // namespace cricket

#endif // __RELAYPORT_H__

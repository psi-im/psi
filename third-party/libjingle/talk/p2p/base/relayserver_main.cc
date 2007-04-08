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

#include <cassert>
#include <iostream>
#include "talk/base/host.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/relayserver.h"

#ifdef POSIX
extern "C" {
#include <errno.h>
}
#endif // POSIX

using namespace cricket;

int main(int argc, char **argv) {
  if (argc != 1) {
    std::cerr << "usage: relayserver" << std::endl;
    return 1;
  }

  assert(LocalHost().networks().size() >= 2);
  SocketAddress int_addr(LocalHost().networks()[1]->ip(), 5000);
  SocketAddress ext_addr(LocalHost().networks()[1]->ip(), 5001);
  
  Thread *pthMain = Thread::Current(); 
  
  AsyncUDPSocket* int_socket = CreateAsyncUDPSocket(pthMain->socketserver());
  if (int_socket->Bind(int_addr) < 0) {
    std::cerr << "bind: " << std::strerror(errno) << std::endl;
    return 1;
  }

  AsyncUDPSocket* ext_socket = CreateAsyncUDPSocket(pthMain->socketserver());
  if (ext_socket->Bind(ext_addr) < 0) {
    std::cerr << "bind: " << std::strerror(errno) << std::endl;
    return 1;
  }

  RelayServer server(pthMain);
  server.AddInternalSocket(int_socket);
  server.AddExternalSocket(ext_socket);

  std::cout << "Listening internally at " << int_addr.ToString() << std::endl;
  std::cout << "Listening externally at " << ext_addr.ToString() << std::endl;

  pthMain->Loop();
  return 0;
}

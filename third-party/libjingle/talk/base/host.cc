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

#include "talk/base/host.h"
#include "talk/base/logging.h"
#include "talk/base/network.h"
#include "talk/base/socket.h"
#include <string>
#include <iostream>
#include <cassert>
#include <errno.h>

#if defined(_MSC_VER) && _MSC_VER < 1300
namespace std {
  using ::strerror;
  using ::exit;
}
#endif

#ifdef POSIX
extern "C" {
#include <sys/utsname.h>
}
#endif // POSIX

namespace {

void FatalError(const std::string& name, int err) {
  PLOG(LERROR, err) << name;
  std::exit(1);
}

}

namespace cricket {

#ifdef POSIX
std::string GetHostName() {
  struct utsname nm;
  if (uname(&nm) < 0)
    FatalError("uname", errno);
  return std::string(nm.nodename);
}
#endif

#ifdef WIN32
std::string GetHostName() {
  // TODO: fix this
  return "cricket";
}
#endif

// Records information about the local host.
Host* gLocalHost = 0;

const Host& LocalHost() {
  if (!gLocalHost) {
    std::vector<Network*>* networks = new std::vector<Network*>;
    NetworkManager::CreateNetworks(*networks);
#ifdef WIN32
    // This is sort of problematic... one part of the code (the unittests) wants
    // 127.0.0.1 to be present and another part (port allocators) don't.  Right
    // now, they use different APIs, so we can have different behavior.  But
    // there is something wrong with this.
    networks->push_back(new Network("localhost",
                                    SocketAddress::StringToIP("127.0.0.1")));
#endif
    gLocalHost = new Host(GetHostName(), networks);
    assert(gLocalHost->networks().size() > 0);
  }

  return *gLocalHost;
}

} // namespace cricket

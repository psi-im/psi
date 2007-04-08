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

#include <cassert>

#ifdef POSIX
extern "C" {
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
}
#endif

#include "talk/base/basictypes.h"
#include "talk/base/byteorder.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/time.h"
#include "talk/base/winping.h"

#ifdef __linux 
#define IP_MTU 14 // Until this is integrated from linux/in.h to netinet/in.h
#endif  // __linux

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define _WINSOCKAPI_
#include <windows.h>
#undef SetPort

#include <algorithm>
#include <iostream>

class WinsockInitializer {
public:
  WinsockInitializer() {
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(1, 0);
    err_ = WSAStartup(wVersionRequested, &wsaData);
  }
  ~WinsockInitializer() {
    WSACleanup();
  }
  int error() {
    return err_;
  }
private:
  int err_;
};
WinsockInitializer g_winsockinit;
#endif

namespace cricket {

const int kfRead  = 0x0001;
const int kfWrite = 0x0002;
const int kfConnect = 0x0004;
const int kfClose = 0x0008;


// Standard MTUs
const uint16 PACKET_MAXIMUMS[] = {
  65535,    // Theoretical maximum, Hyperchannel
  32000,    // Nothing
  17914,    // 16Mb IBM Token Ring
  8166,     // IEEE 802.4
  //4464,   // IEEE 802.5 (4Mb max)
  4352,     // FDDI
  //2048,   // Wideband Network
  2002,     // IEEE 802.5 (4Mb recommended)
  //1536,   // Expermental Ethernet Networks
  //1500,   // Ethernet, Point-to-Point (default)
  1492,     // IEEE 802.3
  1006,     // SLIP, ARPANET
  //576,    // X.25 Networks
  //544,    // DEC IP Portal
  //512,    // NETBIOS
  508,      // IEEE 802/Source-Rt Bridge, ARCNET
  296,      // Point-to-Point (low delay)
  68,       // Official minimum
  0,        // End of list marker
};

const uint32 IP_HEADER_SIZE = 20;
const uint32 ICMP_HEADER_SIZE = 8;

class PhysicalSocket : public AsyncSocket {
public:
  PhysicalSocket(PhysicalSocketServer* ss, SOCKET s = INVALID_SOCKET)
    : ss_(ss), s_(s), enabled_events_(0), error_(0),
      state_((s == INVALID_SOCKET) ? CS_CLOSED : CS_CONNECTED) {
    if (s != INVALID_SOCKET)
      enabled_events_ = kfRead | kfWrite;
  }

  virtual ~PhysicalSocket() {
    Close();
  }

  // Creates the underlying OS socket (same as the "socket" function).
  virtual bool Create(int type) {
    Close();
    s_ = ::socket(AF_INET, type, 0);
    UpdateLastError();
    enabled_events_ = kfRead | kfWrite;
    return s_ != INVALID_SOCKET;
  }

  SocketAddress GetLocalAddress() const {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int result = ::getsockname(s_, (struct sockaddr*)&addr, &addrlen);
    assert(addrlen == sizeof(addr));
    if (result >= 0) {
      return SocketAddress(NetworkToHost32(addr.sin_addr.s_addr),
                           NetworkToHost16(addr.sin_port));
    } else {
      return SocketAddress();
    }
  }

  SocketAddress GetRemoteAddress() const {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int result = ::getpeername(s_, (struct sockaddr*)&addr, &addrlen);
    assert(addrlen == sizeof(addr));
    if (result >= 0) {
      return SocketAddress(
          NetworkToHost32(addr.sin_addr.s_addr),
          NetworkToHost16(addr.sin_port));
    } else {
      assert(errno == ENOTCONN);
      return SocketAddress();
    }
  }

  int Bind(const SocketAddress& addr) {
    struct sockaddr_in saddr;
    IP2SA(&addr, &saddr);
    int err = ::bind(s_, (struct sockaddr*)&saddr, sizeof(saddr));
    UpdateLastError();
    return err;
  }

  int Connect(const SocketAddress& addr) {
    // TODO: Implicit creation is required to reconnect...
    // ...but should we make it more explicit?
    if ((s_ == INVALID_SOCKET) && !Create(SOCK_STREAM))
      return SOCKET_ERROR;
    SocketAddress addr2(addr);
    if (addr2.IsUnresolved()) {
      LOG(INFO) << "Resolving addr in PhysicalSocket::Connect";
      addr2.Resolve(); // TODO: Do this async later?
    }
    struct sockaddr_in saddr;
    IP2SA(&addr2, &saddr);
    int err = ::connect(s_, (struct sockaddr*)&saddr, sizeof(saddr));
    UpdateLastError();
    //LOG(INFO) << "SOCK[" << static_cast<int>(s_) << "] Connect(" << addr2.ToString() << ") Ret: " << err << " Error: " << error_;
    if (err == 0) {
      state_ = CS_CONNECTED;
    } else if (IsBlockingError(error_)) {
      state_ = CS_CONNECTING;
      enabled_events_ |= kfConnect;
    }
    return err;
  }

  int GetError() const {
    return error_;
  }

  void SetError(int error) {
    error_ = error;
  }

  ConnState GetState() const {
    return state_;
  }

  int SetOption(Option opt, int value) {
    assert(opt == OPT_DONTFRAGMENT);
#ifdef WIN32
    value = (value == 0) ? 0 : 1;
    return ::setsockopt(
        s_, IPPROTO_IP, IP_DONTFRAGMENT, reinterpret_cast<char*>(&value),
        sizeof(value));
#endif
#ifdef __linux 
    value = (value == 0) ? IP_PMTUDISC_DONT : IP_PMTUDISC_DO;
    return ::setsockopt(
        s_, IPPROTO_IP, IP_MTU_DISCOVER, &value, sizeof(value));
#endif
#ifdef OSX
    // This is not possible on OSX.
    return -1;
#endif
  }

  int Send(const void *pv, size_t cb) {
    int sent = ::send(s_, reinterpret_cast<const char *>(pv), (int)cb, 0);
    UpdateLastError();
    //LOG(INFO) << "SOCK[" << static_cast<int>(s_) << "] Send(" << cb << ") Ret: " << sent << " Error: " << error_;
    ASSERT(sent <= static_cast<int>(cb));  // We have seen minidumps where this may be false
    if ((sent < 0) && IsBlockingError(error_)) {
      enabled_events_ |= kfWrite;
    }
    return sent;
  }

  int SendTo(const void *pv, size_t cb, const SocketAddress& addr) {
    struct sockaddr_in saddr;
    IP2SA(&addr, &saddr);
    int sent = ::sendto(
        s_, (const char *)pv, (int)cb, 0, (struct sockaddr*)&saddr,
        sizeof(saddr));
    UpdateLastError();
    ASSERT(sent <= static_cast<int>(cb));  // We have seen minidumps where this may be false
    if ((sent < 0) && IsBlockingError(error_)) {
      enabled_events_ |= kfWrite;
    }
    return sent;
  }

  int Recv(void *pv, size_t cb) {
    int received = ::recv(s_, (char *)pv, (int)cb, 0);
    if ((received == 0) && (cb != 0)) {
      // Note: on graceful shutdown, recv can return 0.  In this case, we
      // pretend it is blocking, and then signal close, so that simplifying
      // assumptions can be made about Recv.
      error_ = EWOULDBLOCK;
      return SOCKET_ERROR;
    }
    UpdateLastError();
    if ((received >= 0) || IsBlockingError(error_)) {
      enabled_events_ |= kfRead;
    }
    return received;
  }

  int RecvFrom(void *pv, size_t cb, SocketAddress *paddr) {
    struct sockaddr saddr;
    socklen_t cbAddr = sizeof(saddr);
    int received = ::recvfrom(s_, (char *)pv, (int)cb, 0, &saddr, &cbAddr);
    UpdateLastError();
    if ((received >= 0) && (paddr != NULL))
      SA2IP(&saddr, paddr);
    if ((received >= 0) || IsBlockingError(error_)) {
      enabled_events_ |= kfRead;
    }
    return received;
  }

  int Listen(int backlog) {
    int err = ::listen(s_, backlog);
    UpdateLastError();
    if (err == 0)
      state_ = CS_CONNECTING;
    return err;
  }

  Socket* Accept(SocketAddress *paddr) {
    struct sockaddr saddr;
    socklen_t cbAddr = sizeof(saddr);
    SOCKET s = ::accept(s_, &saddr, &cbAddr);
    UpdateLastError();
    if (s == INVALID_SOCKET)
      return NULL;
    if (paddr != NULL)
      SA2IP(&saddr, paddr);
    return ss_->WrapSocket(s);
  }

  int Close() {
    if (s_ == INVALID_SOCKET)
      return 0;
    int err = ::closesocket(s_);
    UpdateLastError();
    //LOG(INFO) << "SOCK[" << static_cast<int>(s_) << "] Close() Ret: " << err << " Error: " << error_;
    s_ = INVALID_SOCKET;
    state_ = CS_CLOSED;
    enabled_events_ = 0;
    return err;
  }

  int EstimateMTU(uint16* mtu) {
    SocketAddress addr = GetRemoteAddress();
    if (addr.IsAny()) {
      error_ = ENOTCONN;
      return -1;
    }

#ifdef WIN32

    WinPing ping;
    if (!ping.IsValid()) {
      error_ = EINVAL; // can't think of a better error ID
      return -1;
    }

    for (int level = 0; PACKET_MAXIMUMS[level + 1] > 0; ++level) {
      int32 size = PACKET_MAXIMUMS[level] - IP_HEADER_SIZE - ICMP_HEADER_SIZE;
      if (ping.Ping(addr.ip(), size, 0, 1, false) != WinPing::PING_TOO_LARGE) {
        *mtu = PACKET_MAXIMUMS[level];
        return 0;
      }
    }

    assert(false);
    return 0;

#endif // WIN32

#ifdef __linux 

    int value;
    socklen_t vlen = sizeof(value);
    int err = getsockopt(s_, IPPROTO_IP, IP_MTU, &value, &vlen);
    if (err < 0) {
      UpdateLastError();
      return err;
    }

    assert((0 <= value) && (value <= 65536));
    *mtu = uint16(value);
    return 0;

#endif   // __linux

    // TODO: OSX support
  }

  SocketServer* socketserver() { return ss_; }
 
protected:
  PhysicalSocketServer* ss_;
  SOCKET s_;
  uint32 enabled_events_;
  int error_;
  ConnState state_;

  void UpdateLastError() {
#ifdef WIN32
    error_ = WSAGetLastError();
#endif
#ifdef POSIX
    error_ = errno;
#endif
  }

  void IP2SA(const SocketAddress *paddr, struct sockaddr_in *psaddr) {
    memset(psaddr, 0, sizeof(*psaddr));
    psaddr->sin_family = AF_INET;
    psaddr->sin_port = HostToNetwork16(paddr->port());
    if (paddr->ip() == 0)
      psaddr->sin_addr.s_addr = INADDR_ANY;
    else
      psaddr->sin_addr.s_addr = HostToNetwork32(paddr->ip());
  }

  void SA2IP(const struct sockaddr *psaddr, SocketAddress *paddr) {
    const struct sockaddr_in *psaddr_in =
        reinterpret_cast<const struct sockaddr_in*>(psaddr);
    paddr->SetIP(NetworkToHost32(psaddr_in->sin_addr.s_addr));
    paddr->SetPort(NetworkToHost16(psaddr_in->sin_port));
  }
};

#ifdef POSIX
class Dispatcher {
public:
  virtual uint32 GetRequestedEvents() = 0;
  virtual void OnPreEvent(uint32 ff) = 0;    
  virtual void OnEvent(uint32 ff, int err) = 0;
  virtual int GetDescriptor() = 0;
};

class EventDispatcher : public Dispatcher {
public:
  EventDispatcher(PhysicalSocketServer* ss) : ss_(ss), fSignaled_(false) {
    if (pipe(afd_) < 0)
      LOG(LERROR) << "pipe failed";
    ss_->Add(this);
  }

  virtual ~EventDispatcher() {
    ss_->Remove(this);
    close(afd_[0]);
    close(afd_[1]);
  }
  
  virtual void Signal() {
    CritScope cs(&crit_);
    if (!fSignaled_) {
      uint8 b = 0;
      if (write(afd_[1], &b, sizeof(b)) < 0)
        LOG(LERROR) << "write failed";
      fSignaled_ = true;
    }
  }
  
  virtual uint32 GetRequestedEvents() {
    return kfRead;
  }

  virtual void OnPreEvent(uint32 ff) {
    // It is not possible to perfectly emulate an auto-resetting event with
    // pipes.  This simulates it by resetting before the event is handled.
  
    CritScope cs(&crit_);
    if (fSignaled_) {
      uint8 b;
      read(afd_[0], &b, sizeof(b));
      fSignaled_ = false;
    }
  }

  virtual void OnEvent(uint32 ff, int err) {
    assert(false);
  }

  virtual int GetDescriptor() {
    return afd_[0];
  }

private:
  PhysicalSocketServer *ss_;
  int afd_[2];
  bool fSignaled_;
  CriticalSection crit_;
};

class SocketDispatcher : public Dispatcher, public PhysicalSocket {
public:
  SocketDispatcher(PhysicalSocketServer *ss) : PhysicalSocket(ss) {
    ss_->Add(this);
  }
  SocketDispatcher(SOCKET s, PhysicalSocketServer *ss) : PhysicalSocket(ss, s) {
    ss_->Add(this);
  }

  virtual ~SocketDispatcher() {
    ss_->Remove(this);
  }

  bool Initialize() {
    fcntl(s_, F_SETFL, fcntl(s_, F_GETFL, 0) | O_NONBLOCK);
    return true;
  }

  virtual bool Create(int type) {
    // Change the socket to be non-blocking.
    if (!PhysicalSocket::Create(type))
      return false;

    return Initialize();
  }
  
  virtual int GetDescriptor() {
    return s_;
  }

  virtual uint32 GetRequestedEvents() {
    return enabled_events_;
  }

  virtual void OnPreEvent(uint32 ff) {
    if ((ff & kfConnect) != 0)
      state_ = CS_CONNECTED;
  }

  virtual void OnEvent(uint32 ff, int err) {
    if ((ff & kfRead) != 0) {
      enabled_events_ &= ~kfRead;
      SignalReadEvent(this);
    }
    if ((ff & kfWrite) != 0) {
      enabled_events_ &= ~kfWrite;
      SignalWriteEvent(this);
    }
    if ((ff & kfConnect) != 0) {
      enabled_events_ &= ~kfConnect;
      SignalConnectEvent(this);
    }
    if ((ff & kfClose) != 0)
      SignalCloseEvent(this, err);
  }
};

class FileDispatcher: public Dispatcher, public AsyncFile {
public:
  FileDispatcher(int fd, PhysicalSocketServer *ss) : ss_(ss), fd_(fd) {
    set_readable(true);

    ss_->Add(this);

    fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL, 0) | O_NONBLOCK);
  }

  virtual ~FileDispatcher() {
    ss_->Remove(this);
  }

  SocketServer* socketserver() { return ss_; }

  virtual int GetDescriptor() {
    return fd_;
  }

  virtual uint32 GetRequestedEvents() {
    return flags_;
  }

  virtual void OnPreEvent(uint32 ff) {
  }

  virtual void OnEvent(uint32 ff, int err) {
    if ((ff & kfRead) != 0)
      SignalReadEvent(this);
    if ((ff & kfWrite) != 0)
      SignalWriteEvent(this);
    if ((ff & kfClose) != 0)
      SignalCloseEvent(this, err);
  }

  virtual bool readable() {
    return (flags_ & kfRead) != 0;
  }

  virtual void set_readable(bool value) {
    flags_ = value ? (flags_ | kfRead) : (flags_ & ~kfRead);
  }

  virtual bool writable() {
    return (flags_ & kfWrite) != 0;
  }

  virtual void set_writable(bool value) {
    flags_ = value ? (flags_ | kfWrite) : (flags_ & ~kfWrite);
  }

private:
  PhysicalSocketServer* ss_;
  int fd_;
  int flags_;
};

AsyncFile* PhysicalSocketServer::CreateFile(int fd) {
  return new FileDispatcher(fd, this);
}

#endif // POSIX

#ifdef WIN32
class Dispatcher {
public:
  virtual uint32 GetRequestedEvents() = 0;
  virtual void OnPreEvent(uint32 ff) = 0;  
  virtual void OnEvent(uint32 ff, int err) = 0;
  virtual WSAEVENT GetWSAEvent() = 0;
  virtual SOCKET GetSocket() = 0;
  virtual bool CheckSignalClose() = 0;
};

uint32 FlagsToEvents(uint32 events) {
  uint32 ffFD = FD_CLOSE | FD_ACCEPT;
  if (events & kfRead)
    ffFD |= FD_READ;
  if (events & kfWrite)
    ffFD |= FD_WRITE;
  if (events & kfConnect)
    ffFD |= FD_CONNECT;
  return ffFD;
}

class EventDispatcher : public Dispatcher {
public:
  EventDispatcher(PhysicalSocketServer *ss) : ss_(ss) {
    if (hev_ = WSACreateEvent()) {
      ss_->Add(this);
    }
  }

  ~EventDispatcher() {
    if (hev_ != NULL) {
      ss_->Remove(this);
      WSACloseEvent(hev_);
      hev_ = NULL;
    }
  }
  
  virtual void Signal() {
    if (hev_ != NULL)
      WSASetEvent(hev_);
  }
  
  virtual uint32 GetRequestedEvents() {
    return 0;
  }

  virtual void OnPreEvent(uint32 ff) {
    WSAResetEvent(hev_);
  }

  virtual void OnEvent(uint32 ff, int err) {
  }

  virtual WSAEVENT GetWSAEvent() {
    return hev_;
  }

  virtual SOCKET GetSocket() {
    return INVALID_SOCKET;
  }

  virtual bool CheckSignalClose() { return false; }

private:
  PhysicalSocketServer* ss_;
  WSAEVENT hev_;
};

class SocketDispatcher : public Dispatcher, public PhysicalSocket {
public:
  static int next_id_;
  int id_;
  bool signal_close_;
  int signal_err_;

  SocketDispatcher(PhysicalSocketServer* ss) : PhysicalSocket(ss), id_(0), signal_close_(false) {
  }
  SocketDispatcher(SOCKET s, PhysicalSocketServer* ss) : PhysicalSocket(ss, s), id_(0), signal_close_(false) {
  }

  virtual ~SocketDispatcher() {
    Close();
  }

  bool Initialize() {
    assert(s_ != INVALID_SOCKET);
    // Must be a non-blocking
    u_long argp = 1;
    ioctlsocket(s_, FIONBIO, &argp);
    ss_->Add(this);
    return true;
  }
 
  virtual bool Create(int type) {
    // Create socket
    if (!PhysicalSocket::Create(type))
      return false;

    if (!Initialize())
      return false;

    do { id_ = ++next_id_; } while (id_ == 0);
    return true;
  }

  virtual int Close() {
    if (s_ == INVALID_SOCKET)
      return 0;

    id_ = 0;
    signal_close_ = false;
    ss_->Remove(this);
    return PhysicalSocket::Close();
  }

  virtual uint32 GetRequestedEvents() {
    return enabled_events_;
  }

  virtual void OnPreEvent(uint32 ff) {
    if ((ff & kfConnect) != 0)
      state_ = CS_CONNECTED;
  }

  virtual void OnEvent(uint32 ff, int err) {
    int cache_id = id_;
    if ((ff & kfRead) != 0) {
      enabled_events_ &= ~kfRead;
      SignalReadEvent(this);
    }
    if (((ff & kfWrite) != 0) && (id_ == cache_id)) {
      enabled_events_ &= ~kfWrite;
      SignalWriteEvent(this);
    }
    if (((ff & kfConnect) != 0) && (id_ == cache_id)) {
      if (ff != kfConnect)
        LOG(LS_VERBOSE) << "Signalled with kfConnect: " << ff;
      enabled_events_ &= ~kfConnect;
      SignalConnectEvent(this);
    }
    if (((ff & kfClose) != 0) && (id_ == cache_id)) {
      //LOG(INFO) << "SOCK[" << static_cast<int>(s_) << "] OnClose() Error: " << err;
      signal_close_ = true;
      signal_err_ = err;
    }
  }

  virtual WSAEVENT GetWSAEvent() {
    return WSA_INVALID_EVENT;
  }

  virtual SOCKET GetSocket() {
    return s_;
  }

  virtual bool CheckSignalClose() {
    if (!signal_close_)
      return false;

    char ch;
    if (recv(s_, &ch, 1, MSG_PEEK) > 0)
      return false;

    signal_close_ = false;
    SignalCloseEvent(this, signal_err_);
    return true;
  }
};

int SocketDispatcher::next_id_ = 0;

#endif // WIN32

// Sets the value of a boolean value to false when signaled.
class Signaler : public EventDispatcher {
public:
  Signaler(PhysicalSocketServer* ss, bool* pf)
      : EventDispatcher(ss), pf_(pf) {
  }
  virtual ~Signaler() { }

  void OnEvent(uint32 ff, int err) {
    if (pf_)
      *pf_ = false;
  }

private:
  bool *pf_;
};

PhysicalSocketServer::PhysicalSocketServer() : fWait_(false),
  last_tick_tracked_(0), last_tick_dispatch_count_(0) {
  signal_wakeup_ = new Signaler(this, &fWait_);
}

PhysicalSocketServer::~PhysicalSocketServer() {
  delete signal_wakeup_;
}

void PhysicalSocketServer::WakeUp() {
  signal_wakeup_->Signal();
}

Socket* PhysicalSocketServer::CreateSocket(int type) {
  PhysicalSocket* socket = new PhysicalSocket(this);
  if (socket->Create(type)) {
    return socket;
  } else {
    delete socket;
    return 0;
  }
}

AsyncSocket* PhysicalSocketServer::CreateAsyncSocket(int type) {
  SocketDispatcher* dispatcher = new SocketDispatcher(this);
  if (dispatcher->Create(type)) {
    return dispatcher;
  } else {
    delete dispatcher;
    return 0;
  }
}

AsyncSocket* PhysicalSocketServer::WrapSocket(SOCKET s) {
  SocketDispatcher* dispatcher = new SocketDispatcher(s, this);
  if (dispatcher->Initialize()) {
    return dispatcher;
  } else {
    delete dispatcher;
    return 0;
  }
}

void PhysicalSocketServer::Add(Dispatcher *pdispatcher) {
  CritScope cs(&crit_);
  dispatchers_.push_back(pdispatcher);
}

void PhysicalSocketServer::Remove(Dispatcher *pdispatcher) {
  CritScope cs(&crit_);
  dispatchers_.erase(std::remove(dispatchers_.begin(), dispatchers_.end(), pdispatcher), dispatchers_.end());
}

#ifdef POSIX
bool PhysicalSocketServer::Wait(int cmsWait, bool process_io) {
  // Calculate timing information

  struct timeval *ptvWait = NULL;
  struct timeval tvWait;
  struct timeval tvStop;
  if (cmsWait != -1) {
    // Calculate wait timeval
    tvWait.tv_sec = cmsWait / 1000;
    tvWait.tv_usec = (cmsWait % 1000) * 1000;
    ptvWait = &tvWait;

    // Calculate when to return in a timeval
    gettimeofday(&tvStop, NULL);
    tvStop.tv_sec += tvWait.tv_sec;
    tvStop.tv_usec += tvWait.tv_usec;
    if (tvStop.tv_usec >= 1000000) {
      tvStop.tv_usec -= 1000000;
      tvStop.tv_sec += 1;
    }
  }

  // Zero all fd_sets. Don't need to do this inside the loop since
  // select() zeros the descriptors not signaled
  
  fd_set fdsRead;
  FD_ZERO(&fdsRead);
  fd_set fdsWrite;
  FD_ZERO(&fdsWrite);
 
  fWait_ = true;

  while (fWait_) {
    int fdmax = -1;
    {
      CritScope cr(&crit_);
      for (unsigned i = 0; i < dispatchers_.size(); i++) {
        // Query dispatchers for read and write wait state
      
        Dispatcher *pdispatcher = dispatchers_[i];
        assert(pdispatcher);
        if (!process_io && (pdispatcher != signal_wakeup_))
          continue;
        int fd = pdispatcher->GetDescriptor();
        if (fd > fdmax)
          fdmax = fd;
        uint32 ff = pdispatcher->GetRequestedEvents();
        if (ff & kfRead)
          FD_SET(fd, &fdsRead);
        if (ff & (kfWrite | kfConnect))
          FD_SET(fd, &fdsWrite);
      }
    }
      
    // Wait then call handlers as appropriate
    // < 0 means error
    // 0 means timeout
    // > 0 means count of descriptors ready
    int n = select(fdmax + 1, &fdsRead, &fdsWrite, NULL, ptvWait);
    
    // If error, return error
    // todo: do something intelligent
    
    if (n < 0)
      return false;
    
    // If timeout, return success
    
    if (n == 0)
      return true;
    
    // We have signaled descriptors
   
    {
      CritScope cr(&crit_);
      for (unsigned i = 0; i < dispatchers_.size(); i++) {
        Dispatcher *pdispatcher = dispatchers_[i];
        int fd = pdispatcher->GetDescriptor();
        uint32 ff = 0;
        if (FD_ISSET(fd, &fdsRead)) {
          FD_CLR(fd, &fdsRead);
          ff |= kfRead;
        }
        if (FD_ISSET(fd, &fdsWrite)) {
          FD_CLR(fd, &fdsWrite);
          if (pdispatcher->GetRequestedEvents() & kfConnect) {
            ff |= kfConnect;
          } else {
            ff |= kfWrite;
          }
        }
        if (ff != 0) {
          pdispatcher->OnPreEvent(ff);
          pdispatcher->OnEvent(ff, 0);
        }
      }
    }

    // Recalc the time remaining to wait. Doing it here means it doesn't get
    // calced twice the first time through the loop

    if (cmsWait != -1) {
      ptvWait->tv_sec = 0;
      ptvWait->tv_usec = 0;
      struct timeval tvT;
      gettimeofday(&tvT, NULL);
      if (tvStop.tv_sec >= tvT.tv_sec) {
        ptvWait->tv_sec = tvStop.tv_sec - tvT.tv_sec;
        ptvWait->tv_usec = tvStop.tv_usec - tvT.tv_usec;
        if (ptvWait->tv_usec < 0) {
          ptvWait->tv_usec += 1000000;
          ptvWait->tv_sec -= 1;
        }
      }
    }
  }
        
  return true;
}
#endif // POSIX

#ifdef WIN32
bool PhysicalSocketServer::Wait(int cmsWait, bool process_io)
{
  int cmsTotal = cmsWait;
  int cmsElapsed = 0;
  uint32 msStart = GetMillisecondCount();

#if LOGGING
  if (last_tick_dispatch_count_ == 0) {
    last_tick_tracked_ = msStart;
  }
#endif

  WSAEVENT socket_ev = WSACreateEvent();
  
  fWait_ = true;
  while (fWait_) {
    std::vector<WSAEVENT> events;
    std::vector<Dispatcher *> event_owners;

    events.push_back(socket_ev);

    {
      CritScope cr(&crit_);
      for (size_t i = 0; i < dispatchers_.size(); ++i) {
        Dispatcher * disp = dispatchers_[i];
        if (!process_io && (disp != signal_wakeup_))
          continue;
        SOCKET s = disp->GetSocket();
        if (disp->CheckSignalClose()) {
          // We just signalled close, don't poll this socket
        } else if (s != INVALID_SOCKET) {
          WSAEventSelect(s, events[0], FlagsToEvents(disp->GetRequestedEvents()));
        } else {
          events.push_back(disp->GetWSAEvent());
          event_owners.push_back(disp);
        }
      }
    }

    // Which is shorter, the delay wait or the asked wait?

    int cmsNext;
    if (cmsWait == -1) {
      cmsNext = cmsWait;
    } else {
      cmsNext = cmsTotal - cmsElapsed;
      if (cmsNext < 0)
        cmsNext = 0;
    }

    // Wait for one of the events to signal
    DWORD dw = WSAWaitForMultipleEvents(static_cast<DWORD>(events.size()), &events[0], false, cmsNext, false);

#if 0  // LOGGING
    // we track this information purely for logging purposes.
    last_tick_dispatch_count_++;
    if (last_tick_dispatch_count_ >= 1000) {
      uint32 now = GetMillisecondCount();
      LOG(INFO) << "PhysicalSocketServer took " << TimeDiff(now, last_tick_tracked_) << "ms for 1000 events";

      // If we get more than 1000 events in a second, we are spinning badly
      // (normally it should take about 8-20 seconds).
      assert(TimeDiff(now, last_tick_tracked_) > 1000);
      
      last_tick_tracked_ = now;
      last_tick_dispatch_count_ = 0;
    }
#endif

    // Failed?
    // todo: need a better strategy than this!

    if (dw == WSA_WAIT_FAILED) {
      int error = WSAGetLastError();
      assert(false);
      WSACloseEvent(socket_ev);
      return false;
    }

    // Timeout?

    if (dw == WSA_WAIT_TIMEOUT) {
      WSACloseEvent(socket_ev);
      return true;
    }

    // Figure out which one it is and call it

    {
      CritScope cr(&crit_);
      int index = dw - WSA_WAIT_EVENT_0;
      if (index > 0) {
        --index; // The first event is the socket event
        event_owners[index]->OnPreEvent(0);
        event_owners[index]->OnEvent(0, 0);
      } else if (process_io) {
        for (size_t i = 0; i < dispatchers_.size(); ++i) {
          Dispatcher * disp = dispatchers_[i];
          SOCKET s = disp->GetSocket();
          if (s == INVALID_SOCKET)
            continue;

          WSANETWORKEVENTS wsaEvents;
          int err = WSAEnumNetworkEvents(s, events[0], &wsaEvents);
          if (err == 0) {
            
#if LOGGING
            {
              if ((wsaEvents.lNetworkEvents & FD_READ) && wsaEvents.iErrorCode[FD_READ_BIT] != 0) {
                LOG(WARNING) << "PhysicalSocketServer got FD_READ_BIT error " << wsaEvents.iErrorCode[FD_READ_BIT];
              }
              if ((wsaEvents.lNetworkEvents & FD_WRITE) && wsaEvents.iErrorCode[FD_WRITE_BIT] != 0) {
                LOG(WARNING) << "PhysicalSocketServer got FD_WRITE_BIT error " << wsaEvents.iErrorCode[FD_WRITE_BIT];
              }
              if ((wsaEvents.lNetworkEvents & FD_CONNECT) && wsaEvents.iErrorCode[FD_CONNECT_BIT] != 0) {
                LOG(WARNING) << "PhysicalSocketServer got FD_CONNECT_BIT error " << wsaEvents.iErrorCode[FD_CONNECT_BIT];
              }
              if ((wsaEvents.lNetworkEvents & FD_ACCEPT) && wsaEvents.iErrorCode[FD_ACCEPT_BIT] != 0) {
                LOG(WARNING) << "PhysicalSocketServer got FD_ACCEPT_BIT error " << wsaEvents.iErrorCode[FD_ACCEPT_BIT];
              }
              if ((wsaEvents.lNetworkEvents & FD_CLOSE) && wsaEvents.iErrorCode[FD_CLOSE_BIT] != 0) {
                LOG(WARNING) << "PhysicalSocketServer got FD_CLOSE_BIT error " << wsaEvents.iErrorCode[FD_CLOSE_BIT];
              }
            }
#endif
            uint32 ff = 0;
            int errcode = 0;
            if (wsaEvents.lNetworkEvents & FD_READ)
              ff |= kfRead;
            if (wsaEvents.lNetworkEvents & FD_WRITE)
              ff |= kfWrite;
            if (wsaEvents.lNetworkEvents & FD_CONNECT) {
              if (wsaEvents.iErrorCode[FD_CONNECT_BIT] == 0) {
                ff |= kfConnect;
              } else {
                // TODO: Decide whether we want to signal connect, but with an error code
                ff |= kfClose; 
                errcode = wsaEvents.iErrorCode[FD_CONNECT_BIT];
              }
            }
            if (wsaEvents.lNetworkEvents & FD_ACCEPT)
              ff |= kfRead;
            if (wsaEvents.lNetworkEvents & FD_CLOSE) {
              ff |= kfClose;
              errcode = wsaEvents.iErrorCode[FD_CLOSE_BIT];
            }
            if (ff != 0) {
              disp->OnPreEvent(ff);
              disp->OnEvent(ff, errcode);
            }
          }
        }
      }

      // Reset the network event until new activity occurs
      WSAResetEvent(socket_ev);
    }

    // Break?

    if (!fWait_)
      break;
    cmsElapsed = GetMillisecondCount() - msStart;
    if (cmsWait != -1) {
      if (cmsElapsed >= cmsWait)
        break;
    }
  }
  
  // Done
  
  WSACloseEvent(socket_ev);
  return true;
}
#endif // WIN32

} // namespace cricket

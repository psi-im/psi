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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include "talk/base/basicdefs.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"
#ifdef FEATURE_ENABLE_SSL
#include "talk/base/ssladapter.h"
#endif
#include "xmppsocket.h"

XmppSocket::XmppSocket(bool tls) : tls_(tls) {
  cricket::Thread* pth = cricket::Thread::Current();
  cricket::AsyncSocket* socket =
    pth->socketserver()->CreateAsyncSocket(SOCK_STREAM);
#ifdef FEATURE_ENABLE_SSL
  if (tls_) {
    socket = cricket::SSLAdapter::Create(socket);
  }
#endif
  cricket_socket_ = socket;
  cricket_socket_->SignalReadEvent.connect(this, &XmppSocket::OnReadEvent);
  cricket_socket_->SignalWriteEvent.connect(this, &XmppSocket::OnWriteEvent);
  cricket_socket_->SignalConnectEvent.connect(this,
                                              &XmppSocket::OnConnectEvent);
  state_ = buzz::AsyncSocket::STATE_CLOSED;
}

XmppSocket::~XmppSocket() {
  Close();
  delete cricket_socket_;
}

void XmppSocket::OnReadEvent(cricket::AsyncSocket * socket) {
  SignalRead();
}

void XmppSocket::OnWriteEvent(cricket::AsyncSocket * socket) {
  // Write bytes if there are any
  while (buffer_.Length() != 0) {
    int written = cricket_socket_->Send(buffer_.Data(), buffer_.Length());
    if (written > 0) {
      buffer_.Shift(written);
      continue;
    }
    if (!cricket_socket_->IsBlocking())
      LOG(LS_ERROR) << "Send error: " << cricket_socket_->GetError();
    return;
  }
}

void XmppSocket::OnConnectEvent(cricket::AsyncSocket * socket) {
#if defined(FEATURE_ENABLE_SSL)
  if (state_ == buzz::AsyncSocket::STATE_TLS_CONNECTING) {
    state_ = buzz::AsyncSocket::STATE_TLS_OPEN;
    SignalSSLConnected();
    OnWriteEvent(cricket_socket_);
    return;
  }
#endif  // !defined(FEATURE_ENABLE_SSL)
  state_ = buzz::AsyncSocket::STATE_OPEN;
  SignalConnected();
}

buzz::AsyncSocket::State XmppSocket::state() {
  return state_;
}

buzz::AsyncSocket::Error XmppSocket::error() {
  return buzz::AsyncSocket::ERROR_NONE;
}

bool XmppSocket::Connect(const cricket::SocketAddress& addr) {
  if (cricket_socket_->Connect(addr) < 0) {
    return cricket_socket_->IsBlocking();
  }
  return true;
}

bool XmppSocket::Read(char * data, size_t len, size_t* len_read) {
  int read = cricket_socket_->Recv(data, len);
  if (read > 0) {
    *len_read = (size_t)read;
    return true;
  }
  return false;
}

bool XmppSocket::Write(const char * data, size_t len) {
  buffer_.WriteBytes(data, len);
  OnWriteEvent(cricket_socket_);
  return true;
}

bool XmppSocket::Close() {
  if (state_ != buzz::AsyncSocket::STATE_OPEN)
    return false;
  if (cricket_socket_->Close() == 0) {
    state_ = buzz::AsyncSocket::STATE_CLOSED;
    SignalClosed();
    return true;
  }
  return false;
}

bool XmppSocket::StartTls(const std::string & domainname) {
#if defined(FEATURE_ENABLE_SSL)
  if (!tls_)
    return false;
  cricket::SSLAdapter* ssl_adapter =
    static_cast<cricket::SSLAdapter *>(cricket_socket_);
  ssl_adapter->set_ignore_bad_cert(true);
  if (ssl_adapter->StartSSL(domainname.c_str(), false) != 0)
    return false;
  state_ = buzz::AsyncSocket::STATE_TLS_CONNECTING;
  return true;
#else  // !defined(FEATURE_ENABLE_SSL)
  return false;
#endif  // !defined(FEATURE_ENABLE_SSL)
}

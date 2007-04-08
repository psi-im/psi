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

#ifndef __SOCKETADAPTERS_H__
#define __SOCKETADAPTERS_H__

#include <map>

#include "talk/base/asyncsocket.h"
#include "talk/base/logging.h"
#include "talk/xmpp/xmpppassword.h"  // TODO: move xmpppassword to base

namespace cricket {

///////////////////////////////////////////////////////////////////////////////

class BufferedReadAdapter : public AsyncSocketAdapter {
public:
  BufferedReadAdapter(AsyncSocket* socket, size_t buffer_size);
  virtual ~BufferedReadAdapter();

  virtual int Send(const void *pv, size_t cb);
  virtual int Recv(void *pv, size_t cb);

protected:
  int DirectSend(const void *pv, size_t cb) { return AsyncSocketAdapter::Send(pv, cb); }

  void BufferInput(bool on = true);
  virtual void ProcessInput(char * data, size_t& len) = 0;

  virtual void OnReadEvent(AsyncSocket * socket);

private:
  char * buffer_;
  size_t buffer_size_, data_len_;
  bool buffering_;
};

///////////////////////////////////////////////////////////////////////////////

class AsyncSSLSocket : public BufferedReadAdapter {
public:
  AsyncSSLSocket(AsyncSocket* socket);

  virtual int Connect(const SocketAddress& addr);

protected:
  virtual void OnConnectEvent(AsyncSocket * socket);
  virtual void ProcessInput(char * data, size_t& len);
};

///////////////////////////////////////////////////////////////////////////////

class AsyncHttpsProxySocket : public BufferedReadAdapter {
public:
  AsyncHttpsProxySocket(AsyncSocket* socket, const std::string& user_agent,
    const SocketAddress& proxy,
    const std::string& username, const buzz::XmppPassword& password);
  virtual ~AsyncHttpsProxySocket();

  virtual int Connect(const SocketAddress& addr);
  virtual SocketAddress GetRemoteAddress() const;
  virtual int Close();

  struct AuthContext {
    std::string auth_method;
    AuthContext(const std::string& auth) : auth_method(auth) { }
    virtual ~AuthContext() { }
  };

  // 'context' is used by this function to record information between calls.
  // Start by passing a null pointer, then pass the same pointer each additional
  // call.  When the authentication attempt is finished, delete the context.
  enum AuthResult { AR_RESPONSE, AR_IGNORE, AR_CREDENTIALS, AR_ERROR };
  static AuthResult Authenticate(const char * challenge, size_t len,
    const SocketAddress& server,
    const std::string& method, const std::string& uri,
    const std::string& username, const buzz::XmppPassword& password,
    AuthContext *& context, std::string& response, std::string& auth_method);

protected:
  virtual void OnConnectEvent(AsyncSocket * socket);
  virtual void OnCloseEvent(AsyncSocket * socket, int err);
  virtual void ProcessInput(char * data, size_t& len);

  void SendRequest();
  void ProcessLine(char * data, size_t len);
  void EndResponse();
  void Error(int error);

  static void ParseAuth(const char * data, size_t len, std::string& method, std::map<std::string,std::string>& args);

private:
  SocketAddress proxy_, dest_;
  std::string agent_, user_, headers_;
  buzz::XmppPassword pass_;
  size_t content_length_;
  int defer_error_;
  bool expect_close_;
  enum ProxyState { PS_LEADER, PS_AUTHENTICATE, PS_SKIP_HEADERS, PS_ERROR_HEADERS, PS_TUNNEL_HEADERS, PS_SKIP_BODY, PS_TUNNEL, PS_WAIT_CLOSE, PS_ERROR } state_;
  AuthContext * context_;
  std::string unknown_mechanisms_;
};

///////////////////////////////////////////////////////////////////////////////

class AsyncSocksProxySocket : public BufferedReadAdapter {
public:
  AsyncSocksProxySocket(AsyncSocket* socket, const SocketAddress& proxy,
    const std::string& username, const buzz::XmppPassword& password);

  virtual int Connect(const SocketAddress& addr);
  virtual SocketAddress GetRemoteAddress() const;

protected:
  virtual void OnConnectEvent(AsyncSocket * socket);
  virtual void ProcessInput(char * data, size_t& len);

  void SendHello();
  void SendConnect();
  void SendAuth();
  void Error(int error);

private:
  SocketAddress proxy_, dest_;
  std::string user_;
  buzz::XmppPassword pass_;
  enum SocksState { SS_HELLO, SS_AUTH, SS_CONNECT, SS_TUNNEL, SS_ERROR } state_;
};

///////////////////////////////////////////////////////////////////////////////

class LoggingAdapter : public AsyncSocketAdapter {
public:
  LoggingAdapter(AsyncSocket* socket, LoggingSeverity level,
                 const char * label, bool binary_mode = false);

  virtual int Send(const void *pv, size_t cb);
  virtual int SendTo(const void *pv, size_t cb, const SocketAddress& addr);
  virtual int Recv(void *pv, size_t cb);
  virtual int RecvFrom(void *pv, size_t cb, SocketAddress *paddr);

protected:
  virtual void OnConnectEvent(AsyncSocket * socket);
  virtual void OnCloseEvent(AsyncSocket * socket, int err);

private:
  void LogMultiline(bool input, const char * data, size_t len);

  LoggingSeverity level_;
  std::string label_;
  bool binary_mode_;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace cricket

#endif // __SOCKETADAPTERS_H__

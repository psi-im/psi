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

#ifndef _XMPPCLIENTSETTINGS_H_
#define _XMPPCLIENTSETTINGS_H_

#include "talk/p2p/base/port.h"
#include "talk/xmpp/xmpppassword.h"

namespace buzz {

class XmppClientSettings {
public:
  XmppClientSettings() :
    use_tls_(false), use_cookie_auth_(false), protocol_(cricket::PROTO_TCP),
    proxy_(cricket::PROXY_NONE), proxy_port_(80), use_proxy_auth_(false) {}

  void set_user(const std::string & user) { user_ = user; }
  void set_host(const std::string & host) { host_ = host; }
  void set_pass(const XmppPassword & pass) { pass_ = pass; }
  void set_auth_cookie(const std::string & cookie) { auth_cookie_ = cookie; }
  void set_resource(const std::string & resource) { resource_ = resource; }
  void set_use_tls(bool use_tls) { use_tls_ = use_tls; }
  void set_server(const cricket::SocketAddress & server) { 
      server_ = server; 
  }
  void set_protocol(cricket::ProtocolType protocol) { protocol_ = protocol; }
  void set_proxy(cricket::ProxyType f) { proxy_ = f; }
  void set_proxy_host(const std::string & host) { proxy_host_ = host; }
  void set_proxy_port(int port) { proxy_port_ = port; };
  void set_use_proxy_auth(bool f) { use_proxy_auth_ = f; }
  void set_proxy_user(const std::string & user) { proxy_user_ = user; }
  void set_proxy_pass(const XmppPassword & pass) { proxy_pass_ = pass; }

  const std::string & user() const { return user_; }
  const std::string & host() const { return host_; }
  const XmppPassword & pass() const { return pass_; }
  const std::string & auth_cookie() const { return auth_cookie_; }
  const std::string & resource() const { return resource_; }
  bool use_tls() const { return use_tls_; }
  const cricket::SocketAddress & server() const { return server_; }
  cricket::ProtocolType protocol() const { return protocol_; }
  cricket::ProxyType proxy() const { return proxy_; }
  const std::string & proxy_host() const { return proxy_host_; }
  int proxy_port() const { return proxy_port_; }
  bool use_proxy_auth() const { return use_proxy_auth_; }
  const std::string & proxy_user() const { return proxy_user_; }
  const XmppPassword & proxy_pass() const { return proxy_pass_; }

private:
  std::string user_;
  std::string host_;
  XmppPassword pass_;
  std::string auth_cookie_;
  std::string resource_;
  bool use_tls_;
  bool use_cookie_auth_;
  cricket::SocketAddress server_;
  cricket::ProtocolType protocol_;
  cricket::ProxyType proxy_;
  std::string proxy_host_;
  int proxy_port_;
  bool use_proxy_auth_;
  std::string proxy_user_;
  XmppPassword proxy_pass_;
};

}

#endif

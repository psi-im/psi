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

#include <time.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define _WINSOCKAPI_
#include <windows.h>
#include <wininet.h>  // HTTP_STATUS_PROXY_AUTH_REQ
#define SECURITY_WIN32
#include <security.h>
#endif

#include <cassert>

#include "talk/base/base64.h"
#include "talk/base/basicdefs.h"
#include "talk/base/bytebuffer.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/md5.h"
#include "talk/base/socketadapters.h"
#include "talk/base/stringutils.h"

#include <errno.h>


#ifdef WIN32
#include "talk/base/sec_buffer.h"
#endif // WIN32

namespace cricket {

#ifdef WIN32
extern const ConstantLabel SECURITY_ERRORS[];
#endif

static const char HEX[] = "0123456789abcdef";

BufferedReadAdapter::BufferedReadAdapter(AsyncSocket* socket, size_t buffer_size)
  : AsyncSocketAdapter(socket), buffer_size_(buffer_size), data_len_(0), buffering_(false) {
  buffer_ = new char[buffer_size_];
}

BufferedReadAdapter::~BufferedReadAdapter() {
  delete [] buffer_;
}

int BufferedReadAdapter::Send(const void *pv, size_t cb) {
  if (buffering_) {
    // TODO: Spoof error better; Signal Writeable
    socket_->SetError(EWOULDBLOCK);
    return -1;
  }
  return AsyncSocketAdapter::Send(pv, cb);
}

int BufferedReadAdapter::Recv(void *pv, size_t cb) {
  if (buffering_) {
    socket_->SetError(EWOULDBLOCK);
    return -1;
  }

  size_t read = 0;

  if (data_len_) {
    read = _min(cb, data_len_);
    memcpy(pv, buffer_, read);
    data_len_ -= read;
    if (data_len_ > 0) {
      memmove(buffer_, buffer_ + read, data_len_);
    }
    pv = static_cast<char *>(pv) + read;
    cb -= read;
  }

  // FIX: If cb == 0, we won't generate another read event

  int res = AsyncSocketAdapter::Recv(pv, cb);
  if (res < 0)
    return res;

  return res + static_cast<int>(read);
}

void BufferedReadAdapter::BufferInput(bool on) {
  buffering_ = on;
}

void BufferedReadAdapter::OnReadEvent(AsyncSocket * socket) {
  assert(socket == socket_);

  if (!buffering_) {
    AsyncSocketAdapter::OnReadEvent(socket);
    return;
  }

  if (data_len_ >= buffer_size_) {
    LOG(INFO) << "Input buffer overflow";
    assert(false);
    data_len_ = 0;
  }

  int len = socket_->Recv(buffer_ + data_len_, buffer_size_ - data_len_);
  if (len < 0) {
    // TODO: Do something better like forwarding the error to the user.
    LOG(INFO) << "Recv: " << errno << " " <<  std::strerror(errno);
    return;
  }

  data_len_ += len;

  ProcessInput(buffer_, data_len_);
}

///////////////////////////////////////////////////////////////////////////////

const uint8 SSL_SERVER_HELLO[] = {
  22,3,1,0,74,2,0,0,70,3,1,66,133,69,167,39,169,93,160,
  179,197,231,83,218,72,43,63,198,90,202,137,193,88,82,
  161,120,60,91,23,70,0,133,63,32,14,211,6,114,91,91,
  27,95,21,172,19,249,136,83,157,155,232,61,123,12,48,
  50,110,56,77,162,117,87,65,108,52,92,0,4,0
};

const char SSL_CLIENT_HELLO[] = {
  -128,70,1,3,1,0,45,0,0,0,16,1,0,-128,3,0,-128,7,0,-64,6,0,64,2,0,
  -128,4,0,-128,0,0,4,0,-2,-1,0,0,10,0,-2,-2,0,0,9,0,0,100,0,0,98,0,
  0,3,0,0,6,31,23,12,-90,47,0,120,-4,70,85,46,-79,-125,57,-15,-22
};

AsyncSSLSocket::AsyncSSLSocket(AsyncSocket* socket) : BufferedReadAdapter(socket, 1024) {
}

int AsyncSSLSocket::Connect(const SocketAddress& addr) {
  // Begin buffering before we connect, so that there isn't a race condition between
  // potential senders and receiving the OnConnectEvent signal
  BufferInput(true);
  return BufferedReadAdapter::Connect(addr);
}

void AsyncSSLSocket::OnConnectEvent(AsyncSocket * socket) {
  assert(socket == socket_);

  // TODO: we could buffer output too...
  int res = DirectSend(SSL_CLIENT_HELLO, sizeof(SSL_CLIENT_HELLO));
  assert(res == sizeof(SSL_CLIENT_HELLO));
}

void AsyncSSLSocket::ProcessInput(char * data, size_t& len) {
  if (len < sizeof(SSL_SERVER_HELLO))
    return;

  if (memcmp(SSL_SERVER_HELLO, data, sizeof(SSL_SERVER_HELLO)) != 0) {
    Close();
    SignalCloseEvent(this, 0); // TODO: error code?
    return;
  }

  len -= sizeof(SSL_SERVER_HELLO);
  if (len > 0) {
    memmove(data, data + sizeof(SSL_SERVER_HELLO), len);
  }

  bool remainder = (len > 0);
  BufferInput(false);
  SignalConnectEvent(this);

  // FIX: if SignalConnect causes the socket to be destroyed, we are in trouble
  if (remainder)
    SignalReadEvent(this);
}

///////////////////////////////////////////////////////////////////////////////

#define TEST_DIGEST 0
#if TEST_DIGEST
/*
const char * const DIGEST_CHALLENGE =
  "Digest realm=\"testrealm@host.com\","
  " qop=\"auth,auth-int\","
  " nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\","
  " opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"";
const char * const DIGEST_METHOD = "GET";
const char * const DIGEST_URI =
  "/dir/index.html";;
const char * const DIGEST_CNONCE =
  "0a4f113b";
const char * const DIGEST_RESPONSE =
  "6629fae49393a05397450978507c4ef1";
//user_ = "Mufasa";
//pass_ = "Circle Of Life";
*/
const char * const DIGEST_CHALLENGE =
  "Digest realm=\"Squid proxy-caching web server\","
  " nonce=\"Nny4QuC5PwiSDixJ\","
  " qop=\"auth\","
  " stale=false";
const char * const DIGEST_URI =
  "/";
const char * const DIGEST_CNONCE =
  "6501d58e9a21cee1e7b5fec894ded024";
const char * const DIGEST_RESPONSE =
  "edffcb0829e755838b073a4a42de06bc";
#endif

static std::string MD5(const std::string& data) {
  MD5_CTX ctx;
  MD5Init(&ctx);
  MD5Update(&ctx, const_cast<unsigned char *>(reinterpret_cast<const unsigned char *>(data.data())), static_cast<unsigned int>(data.size()));
  unsigned char digest[16];
  MD5Final(digest, &ctx);
  std::string hex_digest;
  for (int i=0; i<16; ++i) {
    hex_digest += HEX[digest[i] >> 4];
    hex_digest += HEX[digest[i] & 0xf];
  }
  return hex_digest;
}

static std::string Quote(const std::string& str) {
  std::string result;
  result.push_back('"');
  for (size_t i=0; i<str.size(); ++i) {
    if ((str[i] == '"') || (str[i] == '\\'))
      result.push_back('\\');
    result.push_back(str[i]);
  }
  result.push_back('"');
  return result;
}

#ifdef WIN32
struct NegotiateAuthContext : public AsyncHttpsProxySocket::AuthContext {
  CredHandle cred;
  CtxtHandle ctx;
  size_t steps;
  bool specified_credentials;

  NegotiateAuthContext(const std::string& auth, CredHandle c1, CtxtHandle c2)
    : AuthContext(auth), cred(c1), ctx(c2), steps(0), specified_credentials(false) { }

  virtual ~NegotiateAuthContext() {
    DeleteSecurityContext(&ctx);
    FreeCredentialsHandle(&cred);
  }
};
#endif // WIN32

AsyncHttpsProxySocket::AuthResult
AsyncHttpsProxySocket::Authenticate(const char * challenge, size_t len,
                                    const SocketAddress& server,
                                    const std::string& method, const std::string& uri,
                                    const std::string& username, const buzz::XmppPassword& password,
                                    AuthContext *& context, std::string& response, std::string& auth_method) {
#if TEST_DIGEST
  challenge = DIGEST_CHALLENGE;
  len = strlen(challenge);
#endif

  std::map<std::string, std::string> args;
  ParseAuth(challenge, len, auth_method, args);

  if (context && (context->auth_method != auth_method))
    return AR_IGNORE;

  // BASIC
  if (_stricmp(auth_method.c_str(), "basic") == 0) {
    if (context)
      return AR_CREDENTIALS; // Bad credentials
    if (username.empty())
      return AR_CREDENTIALS; // Missing credentials

    context = new AuthContext(auth_method);

    // TODO: convert sensitive to a secure buffer that gets securely deleted
    //std::string decoded = username + ":" + password;
    size_t len = username.size() + password.GetLength() + 2;
    char * sensitive = new char[len];
    size_t pos = strcpyn(sensitive, len, username.data(), username.size());
    pos += strcpyn(sensitive + pos, len - pos, ":");
    password.CopyTo(sensitive + pos, true);

    response = auth_method;
    response.append(" ");
    // TODO: create a sensitive-source version of Base64::encode
    response.append(Base64::encode(sensitive));
    memset(sensitive, 0, len);
    delete [] sensitive;
    return AR_RESPONSE;
  }

  // DIGEST
  if (_stricmp(auth_method.c_str(), "digest") == 0) {
    if (context)
      return AR_CREDENTIALS; // Bad credentials
    if (username.empty())
      return AR_CREDENTIALS; // Missing credentials

    context = new AuthContext(auth_method);

    std::string cnonce, ncount;
#if TEST_DIGEST
    method = DIGEST_METHOD;
    uri    = DIGEST_URI;
    cnonce = DIGEST_CNONCE;
#else
    char buffer[256];
    sprintf(buffer, "%d", time(0));
    cnonce = MD5(buffer);
#endif
    ncount = "00000001";

    // TODO: convert sensitive to be secure buffer
    //std::string A1 = username + ":" + args["realm"] + ":" + password;
    size_t len = username.size() + args["realm"].size() + password.GetLength() + 3;
    char * sensitive = new char[len];  // A1
    size_t pos = strcpyn(sensitive, len, username.data(), username.size());
    pos += strcpyn(sensitive + pos, len - pos, ":");
    pos += strcpyn(sensitive + pos, len - pos, args["realm"].c_str());
    pos += strcpyn(sensitive + pos, len - pos, ":");
    password.CopyTo(sensitive + pos, true);

    std::string A2 = method + ":" + uri;
    std::string middle;
    if (args.find("qop") != args.end()) {
      args["qop"] = "auth";
      middle = args["nonce"] + ":" + ncount + ":" + cnonce + ":" + args["qop"];
    } else {
      middle = args["nonce"];
    }
    std::string HA1 = MD5(sensitive);
    memset(sensitive, 0, len);
    delete [] sensitive;
    std::string HA2 = MD5(A2);
    std::string dig_response = MD5(HA1 + ":" + middle + ":" + HA2);

#if TEST_DIGEST
    assert(strcmp(dig_response.c_str(), DIGEST_RESPONSE) == 0);
#endif

    std::stringstream ss;
    ss << auth_method;
    ss << " username=" << Quote(username);
    ss << ", realm=" << Quote(args["realm"]);
    ss << ", nonce=" << Quote(args["nonce"]);
    ss << ", uri=" << Quote(uri);
    if (args.find("qop") != args.end()) {
      ss << ", qop=" << args["qop"];
      ss << ", nc="  << ncount;
      ss << ", cnonce=" << Quote(cnonce);
    }
    ss << ", response=\"" << dig_response << "\"";
    if (args.find("opaque") != args.end()) {
      ss << ", opaque=" << Quote(args["opaque"]);
    }
    response = ss.str();
    return AR_RESPONSE;
  }

#ifdef WIN32
#if 1
  bool want_negotiate = (_stricmp(auth_method.c_str(), "negotiate") == 0);
  bool want_ntlm = (_stricmp(auth_method.c_str(), "ntlm") == 0);
  // SPNEGO & NTLM
  if (want_negotiate || want_ntlm) {
    const size_t MAX_MESSAGE = 12000, MAX_SPN = 256;
    char out_buf[MAX_MESSAGE], spn[MAX_SPN];

#if 0 // Requires funky windows versions
    DWORD len = MAX_SPN;
    if (DsMakeSpn("HTTP", server.IPAsString().c_str(), NULL, server.port(), 0, &len, spn) != ERROR_SUCCESS) {
      LOG(WARNING) << "AsyncHttpsProxySocket::Authenticate(Negotiate) - DsMakeSpn failed";
      return AR_IGNORE;
    }
#else
    sprintfn(spn, MAX_SPN, "HTTP/%s", server.ToString().c_str());
#endif

    SecBuffer out_sec;
    out_sec.pvBuffer   = out_buf;
    out_sec.cbBuffer   = sizeof(out_buf);
    out_sec.BufferType = SECBUFFER_TOKEN;

    SecBufferDesc out_buf_desc;
    out_buf_desc.ulVersion = 0;
    out_buf_desc.cBuffers  = 1;
    out_buf_desc.pBuffers  = &out_sec;

    const ULONG NEG_FLAGS_DEFAULT =
      //ISC_REQ_ALLOCATE_MEMORY
      ISC_REQ_CONFIDENTIALITY
      //| ISC_REQ_EXTENDED_ERROR
      //| ISC_REQ_INTEGRITY
      | ISC_REQ_REPLAY_DETECT
      | ISC_REQ_SEQUENCE_DETECT
      //| ISC_REQ_STREAM
      //| ISC_REQ_USE_SUPPLIED_CREDS
      ;

    TimeStamp lifetime;
    SECURITY_STATUS ret = S_OK;
    ULONG ret_flags = 0, flags = NEG_FLAGS_DEFAULT;

    bool specify_credentials = !username.empty();
    size_t steps = 0;

    //uint32 now = cricket::Time();

    NegotiateAuthContext * neg = static_cast<NegotiateAuthContext *>(context);
    if (neg) {
      const size_t max_steps = 10;
      if (++neg->steps >= max_steps) {
        LOG(WARNING) << "AsyncHttpsProxySocket::Authenticate(Negotiate) too many retries";
        return AR_ERROR;
      }
      steps = neg->steps;

      std::string decoded_challenge = Base64::decode(args[""]);
      if (!decoded_challenge.empty()) {
        SecBuffer in_sec;
        in_sec.pvBuffer   = const_cast<char *>(decoded_challenge.data());
        in_sec.cbBuffer   = static_cast<unsigned long>(decoded_challenge.size());
        in_sec.BufferType = SECBUFFER_TOKEN;

        SecBufferDesc in_buf_desc;
        in_buf_desc.ulVersion = 0;
        in_buf_desc.cBuffers  = 1;
        in_buf_desc.pBuffers  = &in_sec;

        ret = InitializeSecurityContextA(&neg->cred, &neg->ctx, spn, flags, 0, SECURITY_NATIVE_DREP, &in_buf_desc, 0, &neg->ctx, &out_buf_desc, &ret_flags, &lifetime);
        //LOG(INFO) << "$$$ InitializeSecurityContext @ " << cricket::TimeDiff(cricket::Time(), now);
        if (FAILED(ret)) {
          LOG(LS_ERROR) << "InitializeSecurityContext returned: "
                      << ErrorName(ret, SECURITY_ERRORS);
          return AR_ERROR;
        }
      } else if (neg->specified_credentials) {
        // Try again with default credentials
        specify_credentials = false;
        delete context;
        context = neg = 0;
      } else {
        return AR_CREDENTIALS;
      }
    }

    if (!neg) {
      unsigned char userbuf[256], passbuf[256], domainbuf[16];
      SEC_WINNT_AUTH_IDENTITY_A auth_id, * pauth_id = 0;
      if (specify_credentials) {
        memset(&auth_id, 0, sizeof(auth_id));
        size_t len = password.GetLength()+1;
        char * sensitive = new char[len];
        password.CopyTo(sensitive, true);
        std::string::size_type pos = username.find('\\');
        if (pos == std::string::npos) {
          auth_id.UserLength = static_cast<unsigned long>(
            _min(sizeof(userbuf) - 1, username.size()));
          memcpy(userbuf, username.c_str(), auth_id.UserLength);
          userbuf[auth_id.UserLength] = 0;
          auth_id.DomainLength = 0;
          domainbuf[auth_id.DomainLength] = 0;
          auth_id.PasswordLength = static_cast<unsigned long>(
            _min(sizeof(passbuf) - 1, password.GetLength()));
          memcpy(passbuf, sensitive, auth_id.PasswordLength);
          passbuf[auth_id.PasswordLength] = 0;
        } else {
          auth_id.UserLength = static_cast<unsigned long>(
            _min(sizeof(userbuf) - 1, username.size() - pos - 1));
          memcpy(userbuf, username.c_str() + pos + 1, auth_id.UserLength);
          userbuf[auth_id.UserLength] = 0;
          auth_id.DomainLength = static_cast<unsigned long>(
            _min(sizeof(domainbuf) - 1, pos));
          memcpy(domainbuf, username.c_str(), auth_id.DomainLength);
          domainbuf[auth_id.DomainLength] = 0;
          auth_id.PasswordLength = static_cast<unsigned long>(
            _min(sizeof(passbuf) - 1, password.GetLength()));
          memcpy(passbuf, sensitive, auth_id.PasswordLength);
          passbuf[auth_id.PasswordLength] = 0;
        }
        memset(sensitive, 0, len);
        delete [] sensitive;
        auth_id.User = userbuf;
        auth_id.Domain = domainbuf;
        auth_id.Password = passbuf;
        auth_id.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
        pauth_id = &auth_id;
        LOG(LS_VERBOSE) << "Negotiate protocol: Using specified credentials";
      } else {
        LOG(LS_VERBOSE) << "Negotiate protocol: Using default credentials";
      }

      CredHandle cred;
      ret = AcquireCredentialsHandleA(0, want_negotiate ? NEGOSSP_NAME_A : NTLMSP_NAME_A, SECPKG_CRED_OUTBOUND, 0, pauth_id, 0, 0, &cred, &lifetime);
      //LOG(INFO) << "$$$ AcquireCredentialsHandle @ " << cricket::TimeDiff(cricket::Time(), now);
      if (ret != SEC_E_OK) {
        LOG(LS_ERROR) << "AcquireCredentialsHandle error: "
                    << ErrorName(ret, SECURITY_ERRORS);
        return AR_IGNORE;
      }

      //CSecBufferBundle<5, CSecBufferBase::FreeSSPI> sb_out;

      CtxtHandle ctx;
      ret = InitializeSecurityContextA(&cred, 0, spn, flags, 0, SECURITY_NATIVE_DREP, 0, 0, &ctx, &out_buf_desc, &ret_flags, &lifetime);
      //LOG(INFO) << "$$$ InitializeSecurityContext @ " << cricket::TimeDiff(cricket::Time(), now);
      if (FAILED(ret)) {
        LOG(LS_ERROR) << "InitializeSecurityContext returned: "
                    << ErrorName(ret, SECURITY_ERRORS);
        FreeCredentialsHandle(&cred);
        return AR_IGNORE;
      }

      assert(!context);
      context = neg = new NegotiateAuthContext(auth_method, cred, ctx);
      neg->specified_credentials = specify_credentials;
      neg->steps = steps;
    }

    if ((ret == SEC_I_COMPLETE_NEEDED) || (ret == SEC_I_COMPLETE_AND_CONTINUE)) {
      ret = CompleteAuthToken(&neg->ctx, &out_buf_desc);
      //LOG(INFO) << "$$$ CompleteAuthToken @ " << cricket::TimeDiff(cricket::Time(), now);
      LOG(LS_VERBOSE) << "CompleteAuthToken returned: "
                      << ErrorName(ret, SECURITY_ERRORS);
      if (FAILED(ret)) {
        return AR_ERROR;
      }
    }

    //LOG(INFO) << "$$$ NEGOTIATE took " << cricket::TimeDiff(cricket::Time(), now) << "ms";

    std::string decoded(out_buf, out_buf + out_sec.cbBuffer);
    response = auth_method;
    response.append(" ");
    response.append(Base64::encode(decoded));
    return AR_RESPONSE;
  }
#endif
#endif // WIN32

  return AR_IGNORE;
}

inline bool end_of_name(size_t pos, size_t len, const char * data) {
  if (pos >= len)
    return true;
  if (isspace(data[pos]))
    return true;
  // The reason for this complexity is that some non-compliant auth schemes (like Negotiate)
  // use base64 tokens in the challenge instead of name=value.  This could confuse us when the
  // base64 ends in equal signs.
  if ((pos+1 < len) && (data[pos] == '=') && !isspace(data[pos+1]) && (data[pos+1] != '='))
    return true;
  return false;
}

void AsyncHttpsProxySocket::ParseAuth(const char * data, size_t len, std::string& method, std::map<std::string,std::string>& args) {
  size_t pos = 0;
  while ((pos < len) && isspace(data[pos])) ++pos;
  size_t start = pos;
  while ((pos < len) && !isspace(data[pos])) ++pos;
  method.assign(data + start, data + pos);
  while (pos < len) {
    while ((pos < len) && isspace(data[pos])) ++pos;
    if (pos >= len)
      return;

    start = pos;
    while (!end_of_name(pos, len, data)) ++pos;
    //while ((pos < len) && !isspace(data[pos]) && (data[pos] != '=')) ++pos;
    std::string name(data + start, data + pos), value;

    if ((pos < len) && (data[pos] == '=')) {
      ++pos; // Skip '='
      // Check if quoted value
      if ((pos < len) && (data[pos] == '"')) {
        while (++pos < len) {
          if (data[pos] == '"') {
            ++pos;
            break;
          }
          if ((data[pos] == '\\') && (pos + 1 < len))
            ++pos;
          value.append(1, data[pos]);
        }
      } else {
        while ((pos < len) && !isspace(data[pos]) && (data[pos] != ','))
          value.append(1, data[pos++]);
      }
    } else {
      value = name;
      name.clear();
    }

    args.insert(std::make_pair(name, value));
    if ((pos < len) && (data[pos] == ',')) ++pos; // Skip ','
  }
}

AsyncHttpsProxySocket::AsyncHttpsProxySocket(AsyncSocket* socket,
                                             const std::string& user_agent,
                                             const SocketAddress& proxy,
                                             const std::string& username,
                                             const buzz::XmppPassword& password)
  : BufferedReadAdapter(socket, 1024), proxy_(proxy), agent_(user_agent), 
    user_(username), pass_(password), state_(PS_ERROR), context_(0) {
}

AsyncHttpsProxySocket::~AsyncHttpsProxySocket() {
  delete context_;
}

int AsyncHttpsProxySocket::Connect(const SocketAddress& addr) {
  LOG(LS_VERBOSE) << "AsyncHttpsProxySocket::Connect("
                  << proxy_.ToString() << ")";
  dest_ = addr;
  if (dest_.port() != 80) {
    BufferInput(true);
  }
  return BufferedReadAdapter::Connect(proxy_);
}

SocketAddress AsyncHttpsProxySocket::GetRemoteAddress() const {
  return dest_;
}

int AsyncHttpsProxySocket::Close() {
  headers_.clear();
  state_ = PS_ERROR;
  delete context_;
  context_ = 0;
  return BufferedReadAdapter::Close();
}

void AsyncHttpsProxySocket::OnConnectEvent(AsyncSocket * socket) {
  LOG(LS_VERBOSE) << "AsyncHttpsProxySocket::OnConnectEvent";
  // TODO: Decide whether tunneling or not should be explicitly set,
  // or indicated by destination port (as below)
  if (dest_.port() == 80) {
    state_ = PS_TUNNEL;
    BufferedReadAdapter::OnConnectEvent(socket);
    return;
  }
  SendRequest();
}

void AsyncHttpsProxySocket::OnCloseEvent(AsyncSocket * socket, int err) {
  LOG(LS_VERBOSE) << "AsyncHttpsProxySocket::OnCloseEvent(" << err << ")";
  if ((state_ == PS_WAIT_CLOSE) && (err == 0)) {
    state_ = PS_ERROR;
    Connect(dest_);
  } else {
    BufferedReadAdapter::OnCloseEvent(socket, err);
  }
}

void AsyncHttpsProxySocket::ProcessInput(char * data, size_t& len) {
  size_t start = 0;
  for (size_t pos = start; (state_ < PS_TUNNEL) && (pos < len); ) {
    if (state_ == PS_SKIP_BODY) {
      size_t consume = _min(len - pos, content_length_);
      pos += consume;
      start = pos;
      content_length_ -= consume;
      if (content_length_ == 0) {
        EndResponse();
      }
      continue;
    }

    if (data[pos++] != '\n')
      continue;

    size_t len = pos - start - 1;
    if ((len > 0) && (data[start + len - 1] == '\r'))
      --len;

    data[start + len] = 0;
    ProcessLine(data + start, len);
    start = pos;
  }

  len -= start;
  if (len > 0) {
    memmove(data, data + start, len);
  }

  if (state_ != PS_TUNNEL)
    return;

  bool remainder = (len > 0);
  BufferInput(false);
  SignalConnectEvent(this);

  // FIX: if SignalConnect causes the socket to be destroyed, we are in trouble
  if (remainder)
    SignalReadEvent(this); // TODO: signal this??
}

void AsyncHttpsProxySocket::SendRequest() {
  std::stringstream ss;
  ss << "CONNECT " << dest_.ToString() << " HTTP/1.0\r\n";
  ss << "User-Agent: " << agent_ << "\r\n";
  ss << "Host: " << dest_.IPAsString() << "\r\n";
  ss << "Content-Length: 0\r\n";
  ss << "Proxy-Connection: Keep-Alive\r\n";
  ss << headers_;
  ss << "\r\n";
  std::string str = ss.str();
  DirectSend(str.c_str(), str.size());
  state_ = PS_LEADER;
  expect_close_ = true;
  content_length_ = 0;
  headers_.clear();

  LOG(LS_VERBOSE) << "AsyncHttpsProxySocket >> " << str;
}

void AsyncHttpsProxySocket::ProcessLine(char * data, size_t len) {
  LOG(LS_VERBOSE) << "AsyncHttpsProxySocket << " << data;

  if (len == 0) {
    if (state_ == PS_TUNNEL_HEADERS) {
      state_ = PS_TUNNEL;
    } else if (state_ == PS_ERROR_HEADERS) {
      Error(defer_error_);
      return;
    } else if (state_ == PS_SKIP_HEADERS) {
      if (content_length_) {
        state_ = PS_SKIP_BODY;
      } else {
        EndResponse();
        return;
      }
    } else {
      static bool report = false;
      if (!unknown_mechanisms_.empty() && !report) {
        report = true;
        std::string msg(
          "Unable to connect to the Google Talk service due to an incompatibility "
          "with your proxy.\r\nPlease help us resolve this issue by submitting the "
          "following information to us using our technical issue submission form "
          "at:\r\n\r\n"
          "http://www.google.com/support/talk/bin/request.py\r\n\r\n"
          "We apologize for the inconvenience.\r\n\r\n"
          "Information to submit to Google: "
          );
        //std::string msg("Please report the following information to foo@bar.com:\r\nUnknown methods: ");
        msg.append(unknown_mechanisms_);
#ifdef WIN32
        MessageBoxA(0, msg.c_str(), "Oops!", MB_OK);
#endif
#ifdef POSIX
        //TODO: Raise a signal or something so the UI can be separated.
        LOG(LS_ERROR) << "Oops!\n\n" << msg;
#endif
      }
      // Unexpected end of headers
      Error(0);
      return;
    }
  } else if (state_ == PS_LEADER) {
    uint32 code;
    if (sscanf(data, "HTTP/%*lu.%*lu %lu", &code) != 1) {
      Error(0);
      return;
    }
    switch (code) {
    case 200:
      // connection good!
      state_ = PS_TUNNEL_HEADERS;
      return;
#if defined(HTTP_STATUS_PROXY_AUTH_REQ) && (HTTP_STATUS_PROXY_AUTH_REQ != 407)
#error Wrong code for HTTP_STATUS_PROXY_AUTH_REQ
#endif
    case 407: // HTTP_STATUS_PROXY_AUTH_REQ
      state_ = PS_AUTHENTICATE;
      return;
    default:
      defer_error_ = 0;
      state_ = PS_ERROR_HEADERS;
      return;
    }
  } else if ((state_ == PS_AUTHENTICATE) && (_strnicmp(data, "Proxy-Authenticate:", 19) == 0)) {
    std::string response, auth_method;
    switch (Authenticate(data + 19, len - 19, proxy_, "CONNECT", "/", user_, pass_, context_, response, auth_method)) {
    case AR_IGNORE:
      LOG(LS_VERBOSE) << "Ignoring Proxy-Authenticate: " << auth_method;
      if (!unknown_mechanisms_.empty())
        unknown_mechanisms_.append(", ");
      unknown_mechanisms_.append(auth_method);
      break;
    case AR_RESPONSE:
      headers_ = "Proxy-Authorization: ";
      headers_.append(response);
      headers_.append("\r\n");
      state_ = PS_SKIP_HEADERS;
      unknown_mechanisms_.clear();
      break;
    case AR_CREDENTIALS:
      defer_error_ = SOCKET_EACCES;
      state_ = PS_ERROR_HEADERS;
      unknown_mechanisms_.clear();
      break;
    case AR_ERROR:
      defer_error_ = 0;
      state_ = PS_ERROR_HEADERS;
      unknown_mechanisms_.clear();
      break;
    }
  } else if (_strnicmp(data, "Content-Length:", 15) == 0) {
    content_length_ = strtoul(data + 15, 0, 0);
  } else if (_strnicmp(data, "Proxy-Connection: Keep-Alive", 28) == 0) {
    expect_close_ = false;
    /*
  } else if (_strnicmp(data, "Connection: close", 17) == 0) {
    expect_close_ = true;
    */
  }
}

void AsyncHttpsProxySocket::EndResponse() {
  if (!expect_close_) {
    SendRequest();
    return;
  }

  // No point in waiting for the server to close... let's close now
  // TODO: Refactor out PS_WAIT_CLOSE
  state_ = PS_WAIT_CLOSE;
  BufferedReadAdapter::Close();
  OnCloseEvent(this, 0);
}

void AsyncHttpsProxySocket::Error(int error) {
  BufferInput(false);
  Close();
  SetError(error);
  SignalCloseEvent(this, error);
}

///////////////////////////////////////////////////////////////////////////////

AsyncSocksProxySocket::AsyncSocksProxySocket(AsyncSocket* socket, const SocketAddress& proxy,
                                   const std::string& username, const buzz::XmppPassword& password)
  : BufferedReadAdapter(socket, 1024), proxy_(proxy), user_(username), pass_(password),
    state_(SS_ERROR) {
}

int AsyncSocksProxySocket::Connect(const SocketAddress& addr) {
  dest_ = addr;
  BufferInput(true);
  return BufferedReadAdapter::Connect(proxy_);
}

SocketAddress AsyncSocksProxySocket::GetRemoteAddress() const {
  return dest_;
}

void AsyncSocksProxySocket::OnConnectEvent(AsyncSocket * socket) {
  SendHello();
}

void AsyncSocksProxySocket::ProcessInput(char * data, size_t& len) {
  assert(state_ < SS_TUNNEL);

  ByteBuffer response(data, len);

  if (state_ == SS_HELLO) {
    uint8 ver, method;
    if (!response.ReadUInt8(ver) ||
        !response.ReadUInt8(method))
      return;

    if (ver != 5) {
      Error(0);
      return;
    }

    if (method == 0) {
      SendConnect();
    } else if (method == 2) {
      SendAuth();
    } else {
      Error(0);
      return;
    }
  } else if (state_ == SS_AUTH) {
    uint8 ver, status;
    if (!response.ReadUInt8(ver) ||
        !response.ReadUInt8(status))
      return;

    if ((ver != 1) || (status != 0)) {
      Error(SOCKET_EACCES);
      return;
    }

    SendConnect();
  } else if (state_ == SS_CONNECT) {
    uint8 ver, rep, rsv, atyp;
    if (!response.ReadUInt8(ver) ||
        !response.ReadUInt8(rep) ||
        !response.ReadUInt8(rsv) ||
        !response.ReadUInt8(atyp))
      return;

    if ((ver != 5) || (rep != 0)) {
      Error(0);
      return;
    }

    uint16 port;
    if (atyp == 1) {
      uint32 addr;
      if (!response.ReadUInt32(addr) ||
          !response.ReadUInt16(port))
        return;
      LOG(LS_VERBOSE) << "Bound on " << addr << ":" << port;
    } else if (atyp == 3) {
      uint8 len;
      std::string addr;
      if (!response.ReadUInt8(len) ||
          !response.ReadString(addr, len) ||
          !response.ReadUInt16(port))
        return;
      LOG(LS_VERBOSE) << "Bound on " << addr << ":" << port;
    } else if (atyp == 4) {
      std::string addr;
      if (!response.ReadString(addr, 16) ||
          !response.ReadUInt16(port))
        return;
      LOG(LS_VERBOSE) << "Bound on <IPV6>:" << port;
    } else {
      Error(0);
      return;
    }

    state_ = SS_TUNNEL;
  }

  // Consume parsed data
  len = response.Length();
  memcpy(data, response.Data(), len);

  if (state_ != SS_TUNNEL)
    return;

  bool remainder = (len > 0);
  BufferInput(false);
  SignalConnectEvent(this);

  // FIX: if SignalConnect causes the socket to be destroyed, we are in trouble
  if (remainder)
    SignalReadEvent(this); // TODO: signal this??
}

void AsyncSocksProxySocket::SendHello() {
  ByteBuffer request;
  request.WriteUInt8(5);   // Socks Version
  if (user_.empty()) {
    request.WriteUInt8(1); // Authentication Mechanisms
    request.WriteUInt8(0); // No authentication
  } else {
    request.WriteUInt8(2); // Authentication Mechanisms
    request.WriteUInt8(0); // No authentication
    request.WriteUInt8(2); // Username/Password
  }
  DirectSend(request.Data(), request.Length());
  state_ = SS_HELLO;
}

void AsyncSocksProxySocket::SendAuth() {
  ByteBuffer request;
  request.WriteUInt8(1);      // Negotiation Version
  request.WriteUInt8(static_cast<uint8>(user_.size()));
  request.WriteString(user_); // Username
  request.WriteUInt8(static_cast<uint8>(pass_.GetLength()));
  size_t len = pass_.GetLength() + 1;
  char * sensitive = new char[len];
  pass_.CopyTo(sensitive, true);
  request.WriteString(sensitive); // Password
  memset(sensitive, 0, len);
  delete [] sensitive;
  DirectSend(request.Data(), request.Length());
  state_ = SS_AUTH;
}

void AsyncSocksProxySocket::SendConnect() {
  ByteBuffer request;
  request.WriteUInt8(5);             // Socks Version
  request.WriteUInt8(1);             // CONNECT
  request.WriteUInt8(0);             // Reserved
  if (dest_.IsUnresolved()) {
    std::string hostname = dest_.IPAsString();
    request.WriteUInt8(3);           // DOMAINNAME
    request.WriteUInt8(static_cast<uint8>(hostname.size()));
    request.WriteString(hostname);   // Destination Hostname
  } else {
    request.WriteUInt8(1);           // IPV4
    request.WriteUInt32(dest_.ip()); // Destination IP
  }
  request.WriteUInt16(dest_.port()); // Destination Port
  DirectSend(request.Data(), request.Length());
  state_ = SS_CONNECT;
}

void AsyncSocksProxySocket::Error(int error) {
  state_ = SS_ERROR;
  BufferInput(false);
  Close();
  SetError(SOCKET_EACCES);
  SignalCloseEvent(this, error);
}

///////////////////////////////////////////////////////////////////////////////

LoggingAdapter::LoggingAdapter(AsyncSocket* socket, LoggingSeverity level,
                               const char * label, bool binary_mode)
  : AsyncSocketAdapter(socket), level_(level), binary_mode_(binary_mode)
{
  label_.append("[");
  label_.append(label);
  label_.append("]");
}

int
LoggingAdapter::Send(const void *pv, size_t cb) {
  int res = AsyncSocketAdapter::Send(pv, cb);
  if (res > 0)
    LogMultiline(false, static_cast<const char *>(pv), res);
  return res;
}

int
LoggingAdapter::SendTo(const void *pv, size_t cb, const SocketAddress& addr) {
  int res = AsyncSocketAdapter::SendTo(pv, cb, addr);
  if (res > 0)
    LogMultiline(false, static_cast<const char *>(pv), res);
  return res;
}

int
LoggingAdapter::Recv(void *pv, size_t cb) {
  int res = AsyncSocketAdapter::Recv(pv, cb);
  if (res > 0)
    LogMultiline(true, static_cast<const char *>(pv), res);
  return res;
}

int
LoggingAdapter::RecvFrom(void *pv, size_t cb, SocketAddress *paddr) {
  int res = AsyncSocketAdapter::RecvFrom(pv, cb, paddr);
  if (res > 0)
    LogMultiline(true, static_cast<const char *>(pv), res);
  return res;
}

void
LoggingAdapter::OnConnectEvent(AsyncSocket * socket) {
  LOG(level_) << label_ << " Connected";
  AsyncSocketAdapter::OnConnectEvent(socket);
}

void
LoggingAdapter::OnCloseEvent(AsyncSocket * socket, int err) {
  LOG(level_) << label_ << " Closed with error: " << err;
  AsyncSocketAdapter::OnCloseEvent(socket, err);
}

void
LoggingAdapter::LogMultiline(bool input, const char * data, size_t len) {
  const char * direction = (input ? " << " : " >> ");
  if (binary_mode_) {
    const size_t LINE_SIZE = 24;
    char hex_line[LINE_SIZE * 9 / 4 + 2], asc_line[LINE_SIZE + 1];
    while (len > 0) {
      memset(asc_line, ' ', sizeof(asc_line));
      memset(hex_line, ' ', sizeof(hex_line));
      size_t line_len = _min(len, LINE_SIZE);
      for (size_t i=0; i<line_len; ++i) {
        unsigned char ch = static_cast<unsigned char>(data[i]);
        asc_line[i] = isprint(ch) ? data[i] : '.';
        hex_line[i*2 + i/4] = HEX[ch >> 4];
        hex_line[i*2 + i/4 + 1] = HEX[ch & 0xf];
      }
      asc_line[sizeof(asc_line)-1] = 0;
      hex_line[sizeof(hex_line)-1] = 0;
      LOG(level_) << label_ << direction << asc_line << " " << hex_line << " ";
      data += line_len;
      len -= line_len;
    }
    return;
  }
  std::string str(data, len);
  while (!str.empty()) {
    std::string::size_type pos = str.find('\n');
    std::string substr = str;
    if (pos == std::string::npos) {
      substr = str;
      str.clear();
    } else if ((pos > 0) && (str[pos-1] == '\r')) {
      substr = str.substr(0, pos - 1);
      str = str.substr(pos + 1);
    } else {
      substr  = str.substr(0, pos);
      str = str.substr(pos + 1);
    }

    // Filter out any private data
    std::string::size_type pos_private = substr.find("Email");
    if (pos_private == std::string::npos) {
      pos_private = substr.find("Passwd");
    }
    if (pos_private == std::string::npos) {
        LOG(level_) << label_ << direction << substr;
    } else {
        LOG(level_) << label_ << direction << "## TEXT REMOVED ##";
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace cricket

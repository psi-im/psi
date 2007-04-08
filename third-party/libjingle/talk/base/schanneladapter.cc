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

#include "talk/base/win32.h"
#define SECURITY_WIN32
#include <security.h>
#include <schannel.h>

#include <iomanip>
#include <vector>

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/schanneladapter.h"
#include "talk/base/sec_buffer.h"
#include "talk/base/thread.h"

namespace cricket {

/////////////////////////////////////////////////////////////////////////////
// SChannelAdapter
/////////////////////////////////////////////////////////////////////////////

extern const ConstantLabel SECURITY_ERRORS[];

const ConstantLabel SECURITY_ERRORS[] = {
  KLABEL(SEC_I_COMPLETE_AND_CONTINUE),
  KLABEL(SEC_I_COMPLETE_NEEDED),
  KLABEL(SEC_I_CONTINUE_NEEDED),
  KLABEL(SEC_I_INCOMPLETE_CREDENTIALS),
  KLABEL(SEC_E_CERT_EXPIRED),
  KLABEL(SEC_E_INCOMPLETE_MESSAGE),
  KLABEL(SEC_E_INSUFFICIENT_MEMORY),
  KLABEL(SEC_E_INTERNAL_ERROR),
  KLABEL(SEC_E_INVALID_HANDLE),
  KLABEL(SEC_E_INVALID_TOKEN),
  KLABEL(SEC_E_LOGON_DENIED),
  KLABEL(SEC_E_NO_AUTHENTICATING_AUTHORITY),
  KLABEL(SEC_E_NO_CREDENTIALS),
  KLABEL(SEC_E_NOT_OWNER),
  KLABEL(SEC_E_OK),
  KLABEL(SEC_E_SECPKG_NOT_FOUND),
  KLABEL(SEC_E_TARGET_UNKNOWN),
  KLABEL(SEC_E_UNKNOWN_CREDENTIALS),
  KLABEL(SEC_E_UNSUPPORTED_FUNCTION),
  KLABEL(SEC_E_UNTRUSTED_ROOT),
  KLABEL(SEC_E_WRONG_PRINCIPAL),
  LASTLABEL
};

const ConstantLabel SCHANNEL_BUFFER_TYPES[] = {
  KLABEL(SECBUFFER_EMPTY),              //  0
  KLABEL(SECBUFFER_DATA),               //  1
  KLABEL(SECBUFFER_TOKEN),              //  2
  KLABEL(SECBUFFER_PKG_PARAMS),         //  3
  KLABEL(SECBUFFER_MISSING),            //  4
  KLABEL(SECBUFFER_EXTRA),              //  5
  KLABEL(SECBUFFER_STREAM_TRAILER),     //  6
  KLABEL(SECBUFFER_STREAM_HEADER),      //  7
  KLABEL(SECBUFFER_MECHLIST),           // 11
  KLABEL(SECBUFFER_MECHLIST_SIGNATURE), // 12
  KLABEL(SECBUFFER_TARGET),             // 13
  KLABEL(SECBUFFER_CHANNEL_BINDINGS),   // 14
  LASTLABEL
};

void DescribeBuffer(LoggingSeverity severity, const char* prefix,
                    const SecBuffer& sb) {
  LOG(severity) << prefix
                << "(" << sb.cbBuffer
                << ", " << FindLabel(sb.BufferType & ~SECBUFFER_ATTRMASK,
                                     SCHANNEL_BUFFER_TYPES)
                << ", " << sb.pvBuffer << ")";
}

void DescribeBuffers(LoggingSeverity severity, const char* prefix,
                     const SecBufferDesc* sbd) {
  if (LogMessage::GetMinLogSeverity() > severity)
    return;
  LOG(severity) << prefix << "(";
  for (size_t i=0; i<sbd->cBuffers; ++i) {
    DescribeBuffer(severity, "  ", sbd->pBuffers[i]);
  }
  LOG(severity) << ")";
}

const ULONG SSL_FLAGS_DEFAULT = ISC_REQ_ALLOCATE_MEMORY
                              | ISC_REQ_CONFIDENTIALITY
                              | ISC_REQ_EXTENDED_ERROR
                              | ISC_REQ_INTEGRITY
                              | ISC_REQ_REPLAY_DETECT
                              | ISC_REQ_SEQUENCE_DETECT
                              | ISC_REQ_STREAM;
                              //| ISC_REQ_USE_SUPPLIED_CREDS;

typedef std::vector<char> SChannelBuffer;

struct SChannelAdapter::SSLImpl {
  CredHandle cred;
  CtxtHandle ctx;
  bool cred_init, ctx_init;
  SChannelBuffer inbuf, outbuf, readable;
  SecPkgContext_StreamSizes sizes;

  SSLImpl() : cred_init(false), ctx_init(false) { }
};

SChannelAdapter::SChannelAdapter(AsyncSocket* socket)
  : SSLAdapter(socket), state_(SSL_NONE),
    restartable_(false), signal_close_(false), message_pending_(false),
    impl_(new SSLImpl) {
}

SChannelAdapter::~SChannelAdapter() {
  Cleanup();
}

int
SChannelAdapter::StartSSL(const char* hostname, bool restartable) {
  if (state_ != SSL_NONE)
    return ERROR_ALREADY_INITIALIZED;

  ssl_host_name_ = hostname;
  restartable_ = restartable;

  if (socket_->GetState() != Socket::CS_CONNECTED) {
    state_ = SSL_WAIT;
    return 0;
  }

  state_ = SSL_CONNECTING;
  if (int err = BeginSSL()) {
    Error("BeginSSL", err, false);
    return err;
  }

  return 0;
}

int
SChannelAdapter::BeginSSL() {
  LOG(LS_INFO) << "BeginSSL: " << ssl_host_name_;
  ASSERT(state_ == SSL_CONNECTING);

  SECURITY_STATUS ret;

  SCHANNEL_CRED sc_cred = { 0 };
  sc_cred.dwVersion = SCHANNEL_CRED_VERSION;
  //sc_cred.dwMinimumCipherStrength = 128; // Note: use system default
  sc_cred.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_AUTO_CRED_VALIDATION;

  ret = AcquireCredentialsHandle(NULL, UNISP_NAME, SECPKG_CRED_OUTBOUND, NULL,
                                 &sc_cred, NULL, NULL, &impl_->cred, NULL);
  if (ret != SEC_E_OK) {
    LOG(LS_ERROR) << "AcquireCredentialsHandle error: "
                  << ErrorName(ret, SECURITY_ERRORS);
    return ret;
  }
  impl_->cred_init = true;

  ULONG flags = SSL_FLAGS_DEFAULT, ret_flags = 0;
  if (ignore_bad_cert())
    flags |= ISC_REQ_MANUAL_CRED_VALIDATION;

  CSecBufferBundle<2, CSecBufferBase::FreeSSPI> sb_out;
  ret = InitializeSecurityContextA(&impl_->cred, NULL,
                                   const_cast<char*>(ssl_host_name_.c_str()),
                                   flags, 0, 0, NULL, 0,
                                   &impl_->ctx, sb_out.desc(),
                                   &ret_flags, NULL);
  if (SUCCEEDED(ret))
    impl_->ctx_init = true;
  return ProcessContext(ret, NULL, sb_out.desc());
}

int
SChannelAdapter::ContinueSSL() {
  LOG(LS_INFO) << "ContinueSSL";
  ASSERT(state_ == SSL_CONNECTING);

  SECURITY_STATUS ret;

  CSecBufferBundle<2> sb_in;
  sb_in[0].BufferType = SECBUFFER_TOKEN;
  sb_in[0].cbBuffer = static_cast<unsigned long>(impl_->inbuf.size());
  sb_in[0].pvBuffer = &impl_->inbuf[0];
  //DescribeBuffers(LS_VERBOSE, "Input Buffer ", sb_in.desc());

  ULONG flags = SSL_FLAGS_DEFAULT, ret_flags = 0;
  if (ignore_bad_cert())
    flags |= ISC_REQ_MANUAL_CRED_VALIDATION;

  CSecBufferBundle<2, CSecBufferBase::FreeSSPI> sb_out;
  ret = InitializeSecurityContextA(&impl_->cred, &impl_->ctx,
                                   const_cast<char*>(ssl_host_name_.c_str()),
                                   flags, 0, 0, sb_in.desc(), 0,
                                   NULL, sb_out.desc(),
                                   &ret_flags, NULL);
  return ProcessContext(ret, sb_in.desc(), sb_out.desc());
}

int
SChannelAdapter::ProcessContext(long int status, _SecBufferDesc* sbd_in,
                                _SecBufferDesc* sbd_out) {
  LoggingSeverity level = LS_ERROR;
  if ((status == SEC_E_OK)
      || (status != SEC_I_CONTINUE_NEEDED)
      || (status != SEC_E_INCOMPLETE_MESSAGE)) {
    level = LS_VERBOSE;  // Expected messages
  }
  LOG(level) << "InitializeSecurityContext error: "
             << ErrorName(status, SECURITY_ERRORS);
  //if (sbd_in)
  //  DescribeBuffers(LS_VERBOSE, "Input Buffer ", sbd_in);
  //if (sbd_out)
  //  DescribeBuffers(LS_VERBOSE, "Output Buffer ", sbd_out);

  if (status == SEC_E_INCOMPLETE_MESSAGE) {
    // Wait for more input from server.
    return Flush();
  }
  
  if (FAILED(status)) {
    // We can't continue.  Common errors:
    // SEC_E_CERT_EXPIRED - Typically, this means the computer clock is wrong.
    return status;
  }

  // Note: we check both input and output buffers for SECBUFFER_EXTRA.
  // Experience shows it appearing in the input, but the documentation claims
  // it should appear in the output.
  size_t extra = 0;
  if (sbd_in) {
    for (size_t i=0; i<sbd_in->cBuffers; ++i) {
      SecBuffer& buffer = sbd_in->pBuffers[i];
      if (buffer.BufferType == SECBUFFER_EXTRA) {
        extra += buffer.cbBuffer;
      }
    }
  }
  if (sbd_out) {
    for (size_t i=0; i<sbd_out->cBuffers; ++i) {
      SecBuffer& buffer = sbd_out->pBuffers[i];
      if (buffer.BufferType == SECBUFFER_EXTRA) {
        extra += buffer.cbBuffer;
      } else if (buffer.BufferType == SECBUFFER_TOKEN) {
        impl_->outbuf.insert(impl_->outbuf.end(),
          reinterpret_cast<char*>(buffer.pvBuffer),
          reinterpret_cast<char*>(buffer.pvBuffer) + buffer.cbBuffer);
      }
    }
  }

  if (extra) {
    ASSERT(extra <= impl_->inbuf.size());
    size_t consumed = impl_->inbuf.size() - extra;
    memmove(&impl_->inbuf[0], &impl_->inbuf[consumed], extra);
    impl_->inbuf.resize(extra);
  } else {
    impl_->inbuf.clear();
  }

  switch (status) {
  case SEC_E_OK: {
    LOG(LS_INFO) << "QueryContextAttributes";
    status = QueryContextAttributes(&impl_->ctx, SECPKG_ATTR_STREAM_SIZES,
                                    &impl_->sizes);
    if (FAILED(status)) {
      LOG(LS_ERROR) << "QueryContextAttributes error: "
                    << ErrorName(status, SECURITY_ERRORS);
      return status;
    }

    state_ = SSL_CONNECTED;
    AsyncSocketAdapter::OnConnectEvent(this);
#if 0  // TODO: worry about this
    //PRefPtr<SChannelAdapter> lock(this);
    //if (pending_write_)
      AsyncSocketAdapter::OnWriteEvent(this);
    //if (pending_read_)
      AsyncSocketAdapter::OnReadEvent(this);
#endif
    break; }

  case SEC_I_CONTINUE_NEEDED:
    // Send data to server and wait for response.
    break;

  case SEC_I_INCOMPLETE_CREDENTIALS:
    // We don't support client authentication in schannel.
    return status;

  case SEC_I_COMPLETE_AND_CONTINUE:
  case SEC_I_COMPLETE_NEEDED:
    // We don't expect these with schannel, fall through.
  default:
    ASSERT(false);
    return status;
  }

  // Note: ContinueSSL will result in a Flush, unless an error occurs.
  if (!impl_->inbuf.empty())
    return ContinueSSL();

  return Flush();
}

int
SChannelAdapter::DecryptData() {
  SChannelBuffer& inbuf = impl_->inbuf;
  SChannelBuffer& readable = impl_->readable;

  while (!inbuf.empty()) {
    CSecBufferBundle<4> in_buf;
    in_buf[0].BufferType = SECBUFFER_DATA;
    in_buf[0].cbBuffer = static_cast<unsigned long>(inbuf.size());
    in_buf[0].pvBuffer = &inbuf[0];

    //DescribeBuffers(LS_VERBOSE, "Decrypt In ", in_buf.desc());
    SECURITY_STATUS status = DecryptMessage(&impl_->ctx, in_buf.desc(), 0, 0);
    //DescribeBuffers(LS_VERBOSE, "Decrypt Out ", in_buf.desc());

    if (status == SEC_E_OK) {
      size_t extra = 0;
      for (size_t i=0; i<in_buf.desc()->cBuffers; ++i) {
        if (in_buf[i].BufferType == SECBUFFER_DATA) {
          readable.insert(readable.end(),
            reinterpret_cast<char*>(in_buf[i].pvBuffer),
            reinterpret_cast<char*>(in_buf[i].pvBuffer) + in_buf[i].cbBuffer);
        } else if (in_buf[i].BufferType == SECBUFFER_EXTRA) {
          extra += in_buf[i].cbBuffer;
        }
      }
      if (extra) {
        size_t consumed = inbuf.size() - extra;
        memmove(&inbuf[0], &inbuf[consumed], extra);
        inbuf.resize(extra);
      } else {
        inbuf.clear();
      }
    } else if (status == SEC_E_INCOMPLETE_MESSAGE) {
      break;
    } else {
      return status;
    }
  }

  if (!readable.empty()) {
    AsyncSocketAdapter::OnReadEvent(this);
  }
  return 0;
}

void
SChannelAdapter::Cleanup() {
  //LOG(LS_INFO) << "SChannelAdapter::Cleanup";
  if (impl_->ctx_init)
    DeleteSecurityContext(&impl_->ctx);
  if (impl_->cred_init)
    FreeCredentialsHandle(&impl_->cred);
  delete impl_;
}

void
SChannelAdapter::Error(const char* context, int err, bool signal) {
  LOG(LS_WARNING) << "SChannelAdapter::Error("
                  << context << ", "
                  << ErrorName(err, SECURITY_ERRORS) << ")";
  state_ = SSL_ERROR;
  SetError(err);
  if (signal)
    AsyncSocketAdapter::OnCloseEvent(this, err);
}

int
SChannelAdapter::Read() {
  char buffer[4096];
  SChannelBuffer& inbuf = impl_->inbuf;
  while (true) {
    int ret = AsyncSocketAdapter::Recv(buffer, sizeof(buffer));
    if (ret > 0) {
      //LOG(LS_INFO) << "read " << ret << " bytes";
      inbuf.insert(inbuf.end(), buffer, buffer + ret);
    } else if (GetError() == EWOULDBLOCK) {
      return 0;  // Blocking
    } else {
      return GetError();
    }
  }
}

int
SChannelAdapter::Flush() {
  int result = 0;
  size_t pos = 0;
  SChannelBuffer& outbuf = impl_->outbuf;
  while (pos < outbuf.size()) {
    int sent = AsyncSocketAdapter::Send(&outbuf[pos], outbuf.size() - pos);
    if (sent > 0) {
      //LOG(LS_INFO) << "wrote " << sent << " bytes";
      pos += sent;
    } else if (GetError() == EWOULDBLOCK) {
      break;  // Blocking
    } else {
      result = GetError();
      break;
    }
  }
  if (int remainder = outbuf.size() - pos) {
    memmove(&outbuf[0], &outbuf[pos], remainder);
    outbuf.resize(remainder);
  } else {
    outbuf.clear();
  }
  return result;
}

//
// AsyncSocket Implementation
//

int
SChannelAdapter::Send(const void* pv, size_t cb) {
  //LOG(LS_INFO) << "SChannelAdapter::Send(" << cb << ")";

  switch (state_) {
  case SSL_NONE:
    return AsyncSocketAdapter::Send(pv, cb);

  case SSL_WAIT:
  case SSL_CONNECTING:
    //pending_write_ = true;
    SetError(EWOULDBLOCK);
    return SOCKET_ERROR;

  case SSL_CONNECTED:
    break;

  case SSL_ERROR:
  default:
    return SOCKET_ERROR;
  }

  size_t written = 0;
  SChannelBuffer& outbuf = impl_->outbuf;
  while (written < cb) {
    const size_t encrypt_len = std::min<size_t>(cb - written,
                                                impl_->sizes.cbMaximumMessage);

    CSecBufferBundle<4> out_buf;
    out_buf[0].BufferType = SECBUFFER_STREAM_HEADER;
    out_buf[0].cbBuffer = impl_->sizes.cbHeader;
    out_buf[1].BufferType = SECBUFFER_DATA;
    out_buf[1].cbBuffer = static_cast<unsigned long>(encrypt_len);
    out_buf[2].BufferType = SECBUFFER_STREAM_TRAILER;
    out_buf[2].cbBuffer = impl_->sizes.cbTrailer;

    size_t packet_len = out_buf[0].cbBuffer
                      + out_buf[1].cbBuffer
                      + out_buf[2].cbBuffer;

    SChannelBuffer message;
    message.resize(packet_len);
    out_buf[0].pvBuffer = &message[0];
    out_buf[1].pvBuffer = &message[out_buf[0].cbBuffer];
    out_buf[2].pvBuffer = &message[out_buf[0].cbBuffer + out_buf[1].cbBuffer];

    memcpy(out_buf[1].pvBuffer,
           static_cast<const char*>(pv) + written,
           encrypt_len);

    //DescribeBuffers(LS_VERBOSE, "Encrypt In ", out_buf.desc());
    SECURITY_STATUS res = EncryptMessage(&impl_->ctx, 0, out_buf.desc(), 0);
    //DescribeBuffers(LS_VERBOSE, "Encrypt Out ", out_buf.desc());

    if (FAILED(res)) {
      Error("EncryptMessage", res, false);
      return SOCKET_ERROR;
    }

    // We assume that the header and data segments do not change length,
    // or else encrypting the concatenated packet in-place is wrong.
    ASSERT(out_buf[0].cbBuffer == impl_->sizes.cbHeader);
    ASSERT(out_buf[1].cbBuffer == static_cast<unsigned long>(encrypt_len));

    // However, the length of the trailer may change due to padding.
    ASSERT(out_buf[2].cbBuffer <= impl_->sizes.cbTrailer);

    packet_len = out_buf[0].cbBuffer
               + out_buf[1].cbBuffer
               + out_buf[2].cbBuffer;

    written += encrypt_len;
    outbuf.insert(outbuf.end(), &message[0], &message[packet_len-1]+1);
  }

  if (int err = Flush()) {
    state_ = SSL_ERROR;
    SetError(err);
    return SOCKET_ERROR;
  }

  return static_cast<int>(written);
}

int
SChannelAdapter::Recv(void* pv, size_t cb) {
  //LOG(LS_INFO) << "Recv(" << cb << ")";

  switch (state_) {
  case SSL_NONE:
    return AsyncSocketAdapter::Recv(pv, cb);

  case SSL_WAIT:
  case SSL_CONNECTING:
    //pending_read_ = true;
    SetError(EWOULDBLOCK);
    return SOCKET_ERROR;

  case SSL_CONNECTED:
    break;

  case SSL_ERROR:
  default:
    return SOCKET_ERROR;
  }

  SChannelBuffer& readable = impl_->readable;
  if (readable.empty()) {
    SetError(EWOULDBLOCK);
    return SOCKET_ERROR;
  }
  size_t read = min(cb, readable.size());
  memcpy(pv, &readable[0], read);
  if (size_t remaining = readable.size() - read) {
    memmove(&readable[0], &readable[read], remaining);
    readable.resize(remaining);
  } else {
    readable.clear();
  }

  if (!message_pending_ && (!readable.empty() || signal_close_)) {
    if (cricket::Thread* thread = cricket::Thread::Current()) {
      message_pending_ = true;
      thread->Post(this);
    } else {
      LOG(LS_ERROR) << "No thread context available for SChannelAdapter";
      ASSERT(false);
    }
  }

  return static_cast<int>(read);
}

int
SChannelAdapter::Close() {
  if (!impl_->readable.empty()) {
    LOG(WARNING) << "SChannelAdapter::Close with readable data";
    // Note: this isn't strictly an error, but we're using it temporarily to
    // track bugs.
    //ASSERT(false);
  }
  if (state_ == SSL_CONNECTED) {
    DWORD token = SCHANNEL_SHUTDOWN;
    CSecBufferBundle<1> sb_in;
    sb_in[0].BufferType = SECBUFFER_TOKEN;
    sb_in[0].cbBuffer = sizeof(token);
    sb_in[0].pvBuffer = &token;
    ApplyControlToken(&impl_->ctx, sb_in.desc());
    // TODO: In theory, to do a nice shutdown, we need to begin shutdown
    // negotiation with more calls to InitializeSecurityContext.  Since the
    // socket api doesn't support nice shutdown at this point, we don't bother.
  }
  Cleanup();
  impl_ = new SSLImpl;
  state_ = restartable_ ? SSL_WAIT : SSL_NONE;
  signal_close_ = false;
  message_pending_ = false;
  return AsyncSocketAdapter::Close();
}

Socket::ConnState
SChannelAdapter::GetState() const {
  if (signal_close_)
    return CS_CONNECTED;
  ConnState state = socket_->GetState();
  if ((state == CS_CONNECTED)
      && ((state_ == SSL_WAIT) || (state_ == SSL_CONNECTING)))
    state = CS_CONNECTING;
  return state;
}

void
SChannelAdapter::OnConnectEvent(AsyncSocket* socket) {
  LOG(LS_INFO) << "SChannelAdapter::OnConnectEvent";
  if (state_ != SSL_WAIT) {
    ASSERT(state_ == SSL_NONE);
    AsyncSocketAdapter::OnConnectEvent(socket);
    return;
  }

  state_ = SSL_CONNECTING;
  if (int err = BeginSSL()) {
    Error("BeginSSL", err);
  }
}

void
SChannelAdapter::OnReadEvent(AsyncSocket* socket) {
  //LOG(LS_INFO) << "SChannelAdapter::OnReadEvent";

  if (state_ == SSL_NONE) {
    AsyncSocketAdapter::OnReadEvent(socket);
    return;
  }

  if (int err = Read()) {
    Error("Read", err);
    return;
  }

  if (impl_->inbuf.empty())
    return;

  if (state_ == SSL_CONNECTED) {
    if (int err = DecryptData()) {
      Error("DecryptData", err);
    }
  } else if (state_ == SSL_CONNECTING) {
    if (int err = ContinueSSL()) {
      Error("ContinueSSL", err);
    }
  }
}

void
SChannelAdapter::OnWriteEvent(AsyncSocket* socket) {
  //LOG(LS_INFO) << "SChannelAdapter::OnWriteEvent";

  if (state_ == SSL_NONE) {
    AsyncSocketAdapter::OnWriteEvent(socket);
    return;
  }

  if (int err = Flush()) {
    Error("Flush", err);
    return;
  }

  // See if we have more data to write
  if (!impl_->outbuf.empty())
    return;

  // Buffer is empty, submit notification
  if (state_ == SSL_CONNECTED) {
    AsyncSocketAdapter::OnWriteEvent(socket);
  }
}

void
SChannelAdapter::OnCloseEvent(AsyncSocket* socket, int err) {
  if ((state_ == SSL_NONE) || impl_->readable.empty()) {
    AsyncSocketAdapter::OnCloseEvent(socket, err);
    return;
  }

  // If readable is non-empty, then we have a pending Message
  // that will allow us to signal close (eventually).
  signal_close_ = true;
}

void
SChannelAdapter::OnMessage(Message* pmsg) {
  if (!message_pending_)
    return;  // This occurs when socket is closed

  message_pending_ = false;
  if (!impl_->readable.empty()) {
    AsyncSocketAdapter::OnReadEvent(this);
  } else if (signal_close_) {
    signal_close_ = false;
    AsyncSocketAdapter::OnCloseEvent(this, 0); // TODO: cache this error?
  }
}

} // namespace cricket

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/openssladapter.h"
#include "talk/base/stringutils.h"
#include "talk/base/Equifax_Secure_Global_eBusiness_CA-1.h"

//////////////////////////////////////////////////////////////////////
// StreamBIO
//////////////////////////////////////////////////////////////////////

#if 0
static int stream_write(BIO* h, const char* buf, int num);
static int stream_read(BIO* h, char* buf, int size);
static int stream_puts(BIO* h, const char* str);
static long stream_ctrl(BIO* h, int cmd, long arg1, void* arg2);
static int stream_new(BIO* h);
static int stream_free(BIO* data);

static BIO_METHOD methods_stream = {
	BIO_TYPE_BIO,
	"stream",
	stream_write,
	stream_read,
	stream_puts,
	0,
	stream_ctrl,
	stream_new,
	stream_free,
	NULL,
};

BIO_METHOD* BIO_s_stream() { return(&methods_stream); }

BIO* BIO_new_stream(StreamInterface* stream) {
	BIO* ret = BIO_new(BIO_s_stream());
	if (ret == NULL)
    return NULL;
	ret->ptr = stream;
	return ret;
}

static int stream_new(BIO* b) {
	b->shutdown = 0;
	b->init = 1;
	b->num = 0; // 1 means end-of-stream
	b->ptr = 0;
	return 1;
}

static int stream_free(BIO* b) {
	if (b == NULL)
		return 0;
	return 1;
}

static int stream_read(BIO* b, char* out, int outl) {
	if (!out)
		return -1;
	StreamInterface* stream = static_cast<StreamInterface*>(b->ptr);
	BIO_clear_retry_flags(b);
	size_t read;
  int error;
  StreamResult result = stream->Read(out, outl, &read, &error);
  if (result == SR_SUCCESS) {
    return read;
  } else if (result == SR_EOS) {
		b->num = 1;
	} else if (result == SR_BLOCK) {
		BIO_set_retry_read(b);
	}
	return -1;
}

static int stream_write(BIO* b, const char* in, int inl) {
	if (!in)
		return -1;
	StreamInterface* stream = static_cast<StreamInterface*>(b->ptr);
	BIO_clear_retry_flags(b);
	size_t written;
  int error;
  StreamResult result = stream->Write(in, inl, &written, &error);
  if (result == SR_SUCCESS) {
    return written;
  } else if (result == SR_BLOCK) {
		BIO_set_retry_write(b);
	}
	return -1;
}

static int stream_puts(BIO* b, const char* str) {
	return stream_write(b, str, strlen(str));
}

static long stream_ctrl(BIO* b, int cmd, long num, void* ptr) {
  UNUSED(num);
  UNUSED(ptr);

	switch (cmd) {
	case BIO_CTRL_RESET:
		return 0;
	case BIO_CTRL_EOF:
		return b->num;
	case BIO_CTRL_WPENDING:
	case BIO_CTRL_PENDING:
		return 0;
	case BIO_CTRL_FLUSH:
		return 1;
	default:
		return 0;
	}
}
#endif

//////////////////////////////////////////////////////////////////////
// SocketBIO
//////////////////////////////////////////////////////////////////////

static int socket_write(BIO* h, const char* buf, int num);
static int socket_read(BIO* h, char* buf, int size);
static int socket_puts(BIO* h, const char* str);
static long socket_ctrl(BIO* h, int cmd, long arg1, void* arg2);
static int socket_new(BIO* h);
static int socket_free(BIO* data);

static BIO_METHOD methods_socket = {
	BIO_TYPE_BIO,
	"socket",
	socket_write,
	socket_read,
	socket_puts,
	0,
	socket_ctrl,
	socket_new,
	socket_free,
	NULL,
};

BIO_METHOD* BIO_s_socket() { return(&methods_socket); }

BIO* BIO_new_socket(cricket::AsyncSocket* socket) {
	BIO* ret = BIO_new(BIO_s_socket());
	if (ret == NULL) {
          return NULL;
	}
	ret->ptr = socket;
	return ret;
}

static int socket_new(BIO* b) {
	b->shutdown = 0;
	b->init = 1;
	b->num = 0; // 1 means socket closed
	b->ptr = 0;
	return 1;
}

static int socket_free(BIO* b) {
	if (b == NULL)
		return 0;
	return 1;
}

static int socket_read(BIO* b, char* out, int outl) {
	if (!out)
		return -1;
	cricket::AsyncSocket* socket = static_cast<cricket::AsyncSocket*>(b->ptr);
	BIO_clear_retry_flags(b);
  int result = socket->Recv(out, outl);
  if (result > 0) {
    return result;
  } else if (result == 0) {
		b->num = 1;
  } else if (socket->IsBlocking()) {
		BIO_set_retry_read(b);
	}
	return -1;
}

static int socket_write(BIO* b, const char* in, int inl) {
	if (!in)
		return -1;
	cricket::AsyncSocket* socket = static_cast<cricket::AsyncSocket*>(b->ptr);
	BIO_clear_retry_flags(b);
  int result = socket->Send(in, inl);
  if (result > 0) {
    return result;
  } else if (socket->IsBlocking()) {
		BIO_set_retry_write(b);
	}
	return -1;
}

static int socket_puts(BIO* b, const char* str) {
	return socket_write(b, str, strlen(str));
}

static long socket_ctrl(BIO* b, int cmd, long num, void* ptr) {
  UNUSED(num);
  UNUSED(ptr);

	switch (cmd) {
	case BIO_CTRL_RESET:
		return 0;
	case BIO_CTRL_EOF:
		return b->num;
	case BIO_CTRL_WPENDING:
	case BIO_CTRL_PENDING:
		return 0;
	case BIO_CTRL_FLUSH:
		return 1;
	default:
		return 0;
	}
}

/////////////////////////////////////////////////////////////////////////////
// OpenSSLAdapter
/////////////////////////////////////////////////////////////////////////////

namespace cricket {

OpenSSLAdapter::OpenSSLAdapter(AsyncSocket* socket)
  : SSLAdapter(socket),
    state_(SSL_NONE),
    ssl_read_needs_write_(false),
    ssl_write_needs_read_(false),
    restartable_(false),
    ssl_(NULL), ssl_ctx_(NULL) {
}

OpenSSLAdapter::~OpenSSLAdapter() {
  Cleanup();
}

int
OpenSSLAdapter::StartSSL(const char* hostname, bool restartable) {
  if (state_ != SSL_NONE)
    return -1; 

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
OpenSSLAdapter::BeginSSL() {
  LOG(LS_INFO) << "BeginSSL: " << ssl_host_name_;
  ASSERT(state_ == SSL_CONNECTING);

  int err = 0;
  BIO* bio = NULL;

  // First set up the context
  if (!ssl_ctx_)
    ssl_ctx_ = SetupSSLContext();

  if (!ssl_ctx_) {
    err = -1;
    goto ssl_error;
  }

  bio = BIO_new_socket(static_cast<cricket::AsyncSocketAdapter*>(socket_));
  if (!bio) {
    err = -1;
    goto ssl_error;
  }
 
  ssl_ = SSL_new(ssl_ctx_);
  if (!ssl_) {
    err = -1;
    goto ssl_error;
  }

  SSL_set_app_data(ssl_, this);

  SSL_set_bio(ssl_, bio, bio);
  SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE |
                     SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

  // the SSL object owns the bio now
  bio = NULL;

  // Do the connect
  err = ContinueSSL();
  if (err != 0)
    goto ssl_error;

  return err;

ssl_error:
  Cleanup();
  if (bio)
    BIO_free(bio);

  return err;
}

int
OpenSSLAdapter::ContinueSSL() {
  LOG(LS_INFO) << "ContinueSSL";
  ASSERT(state_ == SSL_CONNECTING);

  int code = SSL_connect(ssl_);
  switch (SSL_get_error(ssl_, code)) {
  case SSL_ERROR_NONE:
    LOG(LS_INFO) << " -- success";

    if (!SSLPostConnectionCheck(ssl_, ssl_host_name_.c_str())) {
      LOG(LS_ERROR) << "TLS post connection check failed";
      // make sure we close the socket
      Cleanup();
      // The connect failed so return -1 to shut down the socket
      return -1;
    }

    state_ = SSL_CONNECTED;
    AsyncSocketAdapter::OnConnectEvent(this);
#if 0  // TODO: worry about this
    // Don't let ourselves go away during the callbacks
    PRefPtr<OpenSSLAdapter> lock(this);
    LOG(LS_INFO) << " -- onStreamReadable";
    AsyncSocketAdapter::OnReadEvent(this);
    LOG(LS_INFO) << " -- onStreamWriteable";
    AsyncSocketAdapter::OnWriteEvent(this);
#endif
    break;

  case SSL_ERROR_WANT_READ:
    LOG(LS_INFO) << " -- error want read";
    break;

  case SSL_ERROR_WANT_WRITE:
    LOG(LS_INFO) << " -- error want write";
    break;

  case SSL_ERROR_ZERO_RETURN:
  default:
    LOG(LS_INFO) << " -- error " << code;
    return (code != 0) ? code : -1;
  }

  return 0;
}

void
OpenSSLAdapter::Error(const char* context, int err, bool signal) {
  LOG(LS_WARNING) << "SChannelAdapter::Error("
                  << context << ", " << err << ")";
  state_ = SSL_ERROR;
  SetError(err);
  if (signal)
    AsyncSocketAdapter::OnCloseEvent(this, err);
}

void
OpenSSLAdapter::Cleanup() {
  LOG(LS_INFO) << "Cleanup";

  state_ = SSL_NONE;
  ssl_read_needs_write_ = false;
  ssl_write_needs_read_ = false;

  if (ssl_) {
    SSL_free(ssl_);
    ssl_ = NULL;
  }

  if (ssl_ctx_) {
    SSL_CTX_free(ssl_ctx_);
    ssl_ctx_ = NULL;
  }
}

//
// AsyncSocket Implementation
//

int
OpenSSLAdapter::Send(const void* pv, size_t cb) {
  //LOG(LS_INFO) << "OpenSSLAdapter::Send(" << cb << ")";

  switch (state_) {
  case SSL_NONE:
    return AsyncSocketAdapter::Send(pv, cb);

  case SSL_WAIT:
  case SSL_CONNECTING:
    SetError(EWOULDBLOCK);
    return SOCKET_ERROR;

  case SSL_CONNECTED:
    break;

  case SSL_ERROR:
  default:
    return SOCKET_ERROR;
  }

  // OpenSSL will return an error if we try to write zero bytes
  if (cb == 0)
    return 0;

  ssl_write_needs_read_ = false;

  int code = SSL_write(ssl_, pv, cb);
  switch (SSL_get_error(ssl_, code)) {
  case SSL_ERROR_NONE:
    //LOG(LS_INFO) << " -- success";
    return code;
  case SSL_ERROR_WANT_READ:
    //LOG(LS_INFO) << " -- error want read";
    ssl_write_needs_read_ = true;
    SetError(EWOULDBLOCK);
    break;
  case SSL_ERROR_WANT_WRITE:
    //LOG(LS_INFO) << " -- error want write";
    SetError(EWOULDBLOCK);
    break;
  case SSL_ERROR_ZERO_RETURN:
    //LOG(LS_INFO) << " -- remote side closed";
    SetError(EWOULDBLOCK);
    // do we need to signal closure?
    break;
  default:
    //LOG(LS_INFO) << " -- error " << code;
    Error("SSL_write", (code ? code : -1), false);
    break;
  }

  return SOCKET_ERROR;
}

int
OpenSSLAdapter::Recv(void* pv, size_t cb) {
  //LOG(LS_INFO) << "OpenSSLAdapter::Recv(" << cb << ")";
  switch (state_) {

  case SSL_NONE:
    return AsyncSocketAdapter::Recv(pv, cb);

  case SSL_WAIT:
  case SSL_CONNECTING:
    SetError(EWOULDBLOCK);
    return SOCKET_ERROR;

  case SSL_CONNECTED:
    break;

  case SSL_ERROR:
  default:
    return SOCKET_ERROR;
  }

  // Don't trust OpenSSL with zero byte reads
  if (cb == 0)
    return 0;

  ssl_read_needs_write_ = false;

  int code = SSL_read(ssl_, pv, cb);
  switch (SSL_get_error(ssl_, code)) {
  case SSL_ERROR_NONE:
    //LOG(LS_INFO) << " -- success";
    return code;
  case SSL_ERROR_WANT_READ:
    //LOG(LS_INFO) << " -- error want read";
    SetError(EWOULDBLOCK);
    break;
  case SSL_ERROR_WANT_WRITE:
    //LOG(LS_INFO) << " -- error want write";
    ssl_read_needs_write_ = true;
    SetError(EWOULDBLOCK);
    break;
  case SSL_ERROR_ZERO_RETURN:
    //LOG(LS_INFO) << " -- remote side closed";
    SetError(EWOULDBLOCK);
    // do we need to signal closure?
    break;
  default:
    //LOG(LS_INFO) << " -- error " << code;
    Error("SSL_read", (code ? code : -1), false);
    break;
  }

  return SOCKET_ERROR;
}

int
OpenSSLAdapter::Close() {
  Cleanup();
  state_ = restartable_ ? SSL_WAIT : SSL_NONE;
  return AsyncSocketAdapter::Close();
}

Socket::ConnState
OpenSSLAdapter::GetState() const {
  //if (signal_close_)
  //  return CS_CONNECTED;
  ConnState state = socket_->GetState();
  if ((state == CS_CONNECTED)
      && ((state_ == SSL_WAIT) || (state_ == SSL_CONNECTING)))
    state = CS_CONNECTING;
  return state;
}

void
OpenSSLAdapter::OnConnectEvent(AsyncSocket* socket) {
  LOG(LS_INFO) << "OpenSSLAdapter::OnConnectEvent";
  if (state_ != SSL_WAIT) {
    ASSERT(state_ == SSL_NONE);
    AsyncSocketAdapter::OnConnectEvent(socket);
    return;
  }

  state_ = SSL_CONNECTING;
  if (int err = BeginSSL()) {
    AsyncSocketAdapter::OnCloseEvent(socket, err);
  }
}

void
OpenSSLAdapter::OnReadEvent(AsyncSocket* socket) {
  //LOG(LS_INFO) << "OpenSSLAdapter::OnReadEvent";

  if (state_ == SSL_NONE) {
    AsyncSocketAdapter::OnReadEvent(socket);
    return;
  }

  if (state_ == SSL_CONNECTING) {
    if (int err = ContinueSSL()) {
      Error("ContinueSSL", err);
    }
    return;
  }

  if (state_ != SSL_CONNECTED)
    return;

  // Don't let ourselves go away during the callbacks
  //PRefPtr<OpenSSLAdapter> lock(this); // TODO: fix this
  if (ssl_write_needs_read_)  {
    //LOG(LS_INFO) << " -- onStreamWriteable";
    AsyncSocketAdapter::OnWriteEvent(socket);
  }

  //LOG(LS_INFO) << " -- onStreamReadable";
  AsyncSocketAdapter::OnReadEvent(socket);
}

void
OpenSSLAdapter::OnWriteEvent(AsyncSocket* socket) {
  //LOG(LS_INFO) << "OpenSSLAdapter::OnWriteEvent";

  if (state_ == SSL_NONE) {
    AsyncSocketAdapter::OnWriteEvent(socket);
    return;
  }

  if (state_ == SSL_CONNECTING) {
    if (int err = ContinueSSL()) {
      Error("ContinueSSL", err);
    }
    return;
  }

  if (state_ != SSL_CONNECTED)
    return;

  // Don't let ourselves go away during the callbacks
  //PRefPtr<OpenSSLAdapter> lock(this); // TODO: fix this

  if (ssl_read_needs_write_)  {
    //LOG(LS_INFO) << " -- onStreamReadable";
    AsyncSocketAdapter::OnReadEvent(socket);
  }

  //LOG(LS_INFO) << " -- onStreamWriteable";
  AsyncSocketAdapter::OnWriteEvent(socket);
}

void
OpenSSLAdapter::OnCloseEvent(AsyncSocket* socket, int err) {
  LOG(LS_INFO) << "OpenSSLAdapter::OnCloseEvent(" << err << ")";
  AsyncSocketAdapter::OnCloseEvent(socket, err);
}

// This code is taken from the "Network Security with OpenSSL"
// sample in chapter 5
bool
OpenSSLAdapter::SSLPostConnectionCheck(SSL* ssl, const char* host) {
  if (!host)
    return false;

  // Checking the return from SSL_get_peer_certificate here is not strictly
  // necessary.  With our setup, it is not possible for it to return
  // NULL.  However, it is good form to check the return.
  X509* certificate = SSL_get_peer_certificate(ssl);
  if (!certificate)
    return false;

#ifdef _DEBUG
  {
    LOG(LS_INFO) << "Certificate from server:";
    BIO* mem = BIO_new(BIO_s_mem());
    X509_print_ex(mem, certificate, XN_FLAG_SEP_CPLUS_SPC, X509_FLAG_NO_HEADER);
    BIO_write(mem, "\0", 1);
    char* buffer;
    BIO_get_mem_data(mem, &buffer);
    LOG(LS_INFO) << buffer;
    BIO_free(mem);

    char* cipher_description =
      SSL_CIPHER_description(SSL_get_current_cipher(ssl), NULL, 128);
    LOG(LS_INFO) << "Cipher: " << cipher_description;
    OPENSSL_free(cipher_description);
  }
#endif

  bool ok = false;
  int extension_count = X509_get_ext_count(certificate);
  for (int i = 0; i < extension_count; ++i) {
    X509_EXTENSION* extension = X509_get_ext(certificate, i);
    int extension_nid = OBJ_obj2nid(X509_EXTENSION_get_object(extension));

    if (extension_nid == NID_subject_alt_name) {
      X509V3_EXT_METHOD* meth = X509V3_EXT_get(extension);
      if (!meth)
        break;

      void* ext_str = NULL;
      if (meth->it) {
        ext_str = ASN1_item_d2i(NULL, &(extension->value->data), extension->value->length,
                                ASN1_ITEM_ptr(meth->it));
      } else {
        ext_str = meth->d2i(NULL, &(extension->value->data), extension->value->length);
      }

      STACK_OF(CONF_VALUE)* value = meth->i2v(meth, ext_str, NULL);
      for (int j = 0; j < sk_CONF_VALUE_num(value); ++j) {
        CONF_VALUE* nval = sk_CONF_VALUE_value(value, j);
        if (!strcmp(nval->name, "DNS") && !strcmp(nval->value, host)) {
          ok = true;
          break;
        }
      }
    }
    if (ok)
      break;
  }

  char data[256];
  X509_name_st* subject;
  if (!ok
      && (subject = X509_get_subject_name(certificate))
      && (X509_NAME_get_text_by_NID(subject, NID_commonName,
                                    data, sizeof(data)) > 0)) {
    data[sizeof(data)-1] = 0;
    if (_stricmp(data, host) == 0)
      ok = true;
  }

  X509_free(certificate);

  if (!ok && ignore_bad_cert()) {
    LOG(LS_WARNING) << "TLS certificate check FAILED.  "
      << "Allowing connection anyway.";
    ok = true;
  }

  if (ok) 
    ok = (SSL_get_verify_result(ssl) == X509_V_OK);

  if (!ok && ignore_bad_cert()) {
    LOG(LS_INFO) << "Other TLS post connection checks failed.";
    ok = true;
  }

  return ok;
}

#if _DEBUG

// We only use this for tracing and so it is only needed in debug mode

void
OpenSSLAdapter::SSLInfoCallback(const SSL* s, int where, int ret) {
  const char* str = "undefined";
  int w = where & ~SSL_ST_MASK;
  if (w & SSL_ST_CONNECT) {
    str = "SSL_connect";
  } else if (w & SSL_ST_ACCEPT) {
    str = "SSL_accept";
  }
  if (where & SSL_CB_LOOP) {
    LOG(LS_INFO) <<  str << ":" << SSL_state_string_long(s);
  } else if (where & SSL_CB_ALERT) {
    str = (where & SSL_CB_READ) ? "read" : "write";
    LOG(LS_INFO) <<  "SSL3 alert " << str
      << ":" << SSL_alert_type_string_long(ret)
      << ":" << SSL_alert_desc_string_long(ret);
  } else if (where & SSL_CB_EXIT) {
    if (ret == 0) {
      LOG(LS_INFO) << str << ":failed in " << SSL_state_string_long(s);
    } else if (ret < 0) {
      LOG(LS_INFO) << str << ":error in " << SSL_state_string_long(s);
    }
  }
}

#endif  // _DEBUG

int
OpenSSLAdapter::SSLVerifyCallback(int ok, X509_STORE_CTX* store) {
#if _DEBUG
  if (!ok) {
    char data[256];
    X509* cert = X509_STORE_CTX_get_current_cert(store);
    int depth = X509_STORE_CTX_get_error_depth(store);
    int err = X509_STORE_CTX_get_error(store);

    LOG(LS_INFO) << "Error with certificate at depth: " << depth;
    X509_NAME_oneline(X509_get_issuer_name(cert), data, sizeof(data));
    LOG(LS_INFO) << "  issuer  = " << data;
    X509_NAME_oneline(X509_get_subject_name(cert), data, sizeof(data));
    LOG(LS_INFO) << "  subject = " << data;
    LOG(LS_INFO) << "  err     = " << err
      << ":" << X509_verify_cert_error_string(err);
  }
#endif

  // Get our stream pointer from the store
  SSL* ssl = reinterpret_cast<SSL*>(
                X509_STORE_CTX_get_ex_data(store,
                  SSL_get_ex_data_X509_STORE_CTX_idx()));

  OpenSSLAdapter* stream =
    reinterpret_cast<OpenSSLAdapter*>(SSL_get_app_data(ssl));

  if (!ok && stream->ignore_bad_cert()) {
    LOG(LS_WARNING) << "Ignoring cert error while verifying cert chain";
    ok = 1;
  }

  return ok;
}

SSL_CTX*
OpenSSLAdapter::SetupSSLContext() {
  SSL_CTX* ctx = SSL_CTX_new(TLSv1_client_method());
  if (ctx == NULL) 
	  return NULL;
 
  // Add the root cert to the SSL context
  unsigned char* cert_buffer
    = EquifaxSecureGlobalEBusinessCA1_certificate;
  size_t cert_buffer_len = sizeof(EquifaxSecureGlobalEBusinessCA1_certificate);
  X509* cert = d2i_X509(NULL, &cert_buffer, cert_buffer_len);
  if (cert == NULL) {
    SSL_CTX_free(ctx);
    return NULL;
  }
  if (!X509_STORE_add_cert(SSL_CTX_get_cert_store(ctx), cert)) {
    X509_free(cert);
    SSL_CTX_free(ctx);
    return NULL;
  }

#ifdef _DEBUG
  SSL_CTX_set_info_callback(ctx, SSLInfoCallback);
#endif

  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, SSLVerifyCallback);
  SSL_CTX_set_verify_depth(ctx, 4);
  SSL_CTX_set_cipher_list(ctx, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");

  return ctx;
}

} // namespace cricket

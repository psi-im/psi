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

#include "talk/base/ssladapter.h"

#if !defined(SSL_USE_SCHANNEL) && !defined(SSL_USE_OPENSSL)
#ifdef WIN32
#define SSL_USE_SCHANNEL 1
#else  // !WIN32
#define SSL_USE_OPENSSL 1
#endif  // !WIN32
#endif

#if SSL_USE_SCHANNEL
#include "schanneladapter.h"
namespace cricket {
  typedef SChannelAdapter DefaultSSLAdapter;
}
#endif  // SSL_USE_SCHANNEL

#if SSL_USE_OPENSSL
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include "openssladapter.h"
namespace cricket {
  typedef OpenSSLAdapter DefaultSSLAdapter;
}
#endif  // SSL_USE_OPENSSL

///////////////////////////////////////////////////////////////////////////////

namespace cricket {

SSLAdapter*
SSLAdapter::Create(AsyncSocket* socket) {
  return new DefaultSSLAdapter(socket);
}

}  // namespace cricket

///////////////////////////////////////////////////////////////////////////////

#if SSL_USE_OPENSSL

#if defined(WIN32)
  #define MUTEX_TYPE HANDLE
  #define MUTEX_SETUP(x) (x) = CreateMutex(NULL, FALSE, NULL)
  #define MUTEX_CLEANUP(x) CloseHandle(x)
  #define MUTEX_LOCK(x) WaitForSingleObject((x), INFINITE)
  #define MUTEX_UNLOCK(x) ReleaseMutex(x)
  #define THREAD_ID GetCurrentThreadId()
#elif defined(_POSIX_THREADS)
  // _POSIX_THREADS is normally defined in unistd.h if pthreads are available
  // on your platform.
  #define MUTEX_TYPE pthread_mutex_t
  #define MUTEX_SETUP(x) pthread_mutex_init(&(x), NULL)
  #define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
  #define MUTEX_LOCK(x) pthread_mutex_lock(&(x))
  #define MUTEX_UNLOCK(x) pthread_mutex_unlock(&(x))
  #define THREAD_ID pthread_self()
#else
  #error You must define mutex operations appropriate for your platform!
#endif

// This array will store all of the mutexes available to OpenSSL.
static MUTEX_TYPE* mutex_buf = NULL;

static void locking_function(int mode, int n, const char * file, int line) {
  if (mode & CRYPTO_LOCK) {
    MUTEX_LOCK(mutex_buf[n]);
  } else {
    MUTEX_UNLOCK(mutex_buf[n]);
  }
}

static unsigned long id_function() {
  return static_cast<unsigned long>(THREAD_ID);
}

struct CRYPTO_dynlock_value {
  MUTEX_TYPE mutex;
};

static CRYPTO_dynlock_value* dyn_create_function(const char* file, int line) {
  CRYPTO_dynlock_value* value = new CRYPTO_dynlock_value;
  if (!value)
    return NULL;
  MUTEX_SETUP(value->mutex);
  return value;
}

static void dyn_lock_function(int mode, CRYPTO_dynlock_value* l,
                              const char* file, int line) {
  if (mode & CRYPTO_LOCK) {
    MUTEX_LOCK(l->mutex);
  } else {
    MUTEX_UNLOCK(l->mutex);
  }
}

static void dyn_destroy_function(CRYPTO_dynlock_value* l,
                                 const char* file, int line) {
  MUTEX_CLEANUP(l->mutex);
  delete l;
}

bool InitializeSSL() {
  if (!InitializeSSLThread() || !SSL_library_init())
  	  return false;
  SSL_load_error_strings();
  ERR_load_BIO_strings();
  OpenSSL_add_all_algorithms();
  RAND_poll();
  return true;
}

bool InitializeSSLThread() {
  mutex_buf = new MUTEX_TYPE[CRYPTO_num_locks()];
  if (!mutex_buf)
    return false;
  for (int i = 0; i < CRYPTO_num_locks(); ++i)
    MUTEX_SETUP(mutex_buf[i]);
  CRYPTO_set_id_callback(id_function);
  CRYPTO_set_locking_callback(locking_function);
  CRYPTO_set_dynlock_create_callback(dyn_create_function);
  CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
  CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);
  return true;
}

bool CleanupSSL() {
  if (!mutex_buf)
    return false;
  CRYPTO_set_id_callback(NULL);
  CRYPTO_set_locking_callback(NULL);
  CRYPTO_set_dynlock_create_callback(NULL);
  CRYPTO_set_dynlock_lock_callback(NULL);
  CRYPTO_set_dynlock_destroy_callback(NULL);
  for (int i = 0; i < CRYPTO_num_locks(); ++i)
    MUTEX_CLEANUP(mutex_buf[i]);
  delete [] mutex_buf;
  mutex_buf = NULL;
  return true;
}

#else  // !SSL_USE_OPENSSL

bool InitializeSSL() {
  return true;
}

bool InitializeSSLThread() {
  return true;
}

bool CleanupSSL() {
  return true;
}

#endif  // !SSL_USE_OPENSSL

///////////////////////////////////////////////////////////////////////////////

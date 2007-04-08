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

#ifndef __THREAD_H__
#define __THREAD_H__

#include "talk/base/messagequeue.h"

#include <algorithm>
#include <list>
#include <vector>

#ifdef POSIX
#include <pthread.h>
#endif

#ifdef WIN32
#include "talk/base/win32.h"
#endif

namespace cricket {

class Thread;

class ThreadManager {
public:
  ThreadManager();
  ~ThreadManager();

  static Thread *CurrentThread();
  static void SetCurrent(Thread *thread);
  void Add(Thread *thread);
  void Remove(Thread *thread);

private:
  Thread *main_thread_;
  std::vector<Thread *> threads_;
  CriticalSection crit_;

#ifdef POSIX
  static pthread_key_t key_; 
#endif

#ifdef WIN32
  static DWORD key_;
#endif
};

class Thread;

struct _SendMessage {
  _SendMessage() {}
  Thread *thread;
  Message msg;
  bool *ready;
};

class Thread : public MessageQueue {
public:
  Thread(SocketServer* ss = 0);
  virtual ~Thread();

  static inline Thread* Current() {
    return ThreadManager::CurrentThread();
  }
  inline bool IsCurrent() const {
    return (ThreadManager::CurrentThread() == this);
  }

  virtual void Start();
  virtual void Stop();
  virtual void Loop(int cms = -1);
  virtual void Send(MessageHandler *phandler, uint32 id = 0,
      MessageData *pdata = NULL);

  // From MessageQueue
  virtual void Clear(MessageHandler *phandler, uint32 id = (uint32)-1);
  virtual void ReceiveSends();

#ifdef WIN32
  HANDLE GetHandle() {
    return thread_;
  }
#endif

private:
  static void *PreLoop(void *pv);
  void Join();

  std::list<_SendMessage> sendlist_;
  bool started_;
  bool has_sends_;

#ifdef POSIX
  pthread_t thread_;
#endif

#ifdef WIN32
  HANDLE thread_;
#endif

  friend class ThreadManager;
};

// AutoThread automatically installs itself at construction
// uninstalls at destruction, if a Thread object is
// _not already_ associated with the current OS thread.

class AutoThread : public Thread {
public:
  AutoThread(SocketServer* ss = 0);
  virtual ~AutoThread();
};

} // namespace cricket

#endif // __THREAD_H__

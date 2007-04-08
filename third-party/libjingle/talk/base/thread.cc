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

#ifdef POSIX
extern "C" {
#include <sys/time.h>
}
#endif

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"
#include "talk/base/time.h"

namespace cricket {

ThreadManager g_thmgr;

#ifdef POSIX
pthread_key_t ThreadManager::key_;

ThreadManager::ThreadManager() {
  pthread_key_create(&key_, NULL);
  main_thread_ = new Thread();
  SetCurrent(main_thread_);
}

ThreadManager::~ThreadManager() {
  pthread_key_delete(key_);
  delete main_thread_;
}

Thread *ThreadManager::CurrentThread() {
  return (Thread *)pthread_getspecific(key_);
}

void ThreadManager::SetCurrent(Thread *thread) {
  pthread_setspecific(key_, thread);
}
#endif

#ifdef WIN32
DWORD ThreadManager::key_;

ThreadManager::ThreadManager() {
  key_ = TlsAlloc();
  main_thread_ = new Thread();
  SetCurrent(main_thread_);
}

ThreadManager::~ThreadManager() {
  TlsFree(key_);
  delete main_thread_;
}

Thread *ThreadManager::CurrentThread() {
  return (Thread *)TlsGetValue(key_);
}

void ThreadManager::SetCurrent(Thread *thread) {
  TlsSetValue(key_, thread);
}
#endif

void ThreadManager::Add(Thread *thread) {
  CritScope cs(&crit_);
  threads_.push_back(thread);
}

void ThreadManager::Remove(Thread *thread) {
  CritScope cs(&crit_);
  threads_.erase(std::remove(threads_.begin(), threads_.end(), thread), threads_.end());
}

Thread::Thread(SocketServer* ss) : MessageQueue(ss) {
  g_thmgr.Add(this);
  started_ = false;
  has_sends_ = false;
}

Thread::~Thread() {
  Stop();
  Clear(NULL);
  g_thmgr.Remove(this);
}

#ifdef POSIX
void Thread::Start() {
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_create(&thread_, &attr, PreLoop, this);
  started_ = true;
}

void Thread::Join() {
  if (started_) {
    void *pv;
    pthread_join(thread_, &pv);
  }
}
#endif

#ifdef WIN32
void Thread::Start() {
  thread_ = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PreLoop, this, 0, NULL);
  started_ = true;
}

void Thread::Join() {
  if (started_) {
    WaitForSingleObject(thread_, INFINITE);
    CloseHandle(thread_);
    started_ = false;
  }
}
#endif

void *Thread::PreLoop(void *pv) {
  Thread *thread = (Thread *)pv;
  ThreadManager::SetCurrent(thread);
  thread->Loop();
  return NULL;
}

void Thread::Loop(int cmsLoop) {
  uint32 msEnd;
  if (cmsLoop != -1)
    msEnd = GetMillisecondCount() + cmsLoop;
  int cmsNext = cmsLoop;

  while (true) {
    Message msg;
    if (!Get(&msg, cmsNext))
      return;
    Dispatch(&msg);
    
    if (cmsLoop != -1) {
      uint32 msCur = GetMillisecondCount();
      if (msCur >= msEnd)
        return;
      cmsNext = msEnd - msCur;
    }
  }
}

void Thread::Stop() {
  MessageQueue::Stop();
  Join();
}

void Thread::Send(MessageHandler *phandler, uint32 id, MessageData *pdata) {
  // Sent messages are sent to the MessageHandler directly, in the context
  // of "thread", like Win32 SendMessage. If in the right context,
  // call the handler directly.

  Message msg;
  msg.phandler = phandler;
  msg.message_id = id;
  msg.pdata = pdata;
  if (IsCurrent()) {
    phandler->OnMessage(&msg);
    return;
  }

  AutoThread thread; 
  Thread *current_thread = Thread::Current();
  ASSERT(current_thread != NULL);  // AutoThread ensures this

  crit_.Enter();
  bool ready = false;
  _SendMessage smsg;
  smsg.thread = current_thread;
  smsg.msg = msg;
  smsg.ready = &ready;
  sendlist_.push_back(smsg);
  has_sends_ = true;
  crit_.Leave();

  // Wait for a reply

  ss_->WakeUp();
  while (!ready) {
    current_thread->ReceiveSends();
    current_thread->socketserver()->Wait(-1, false);
  }
}

void Thread::ReceiveSends() {
  // Before entering critical section, check boolean.

  if (!has_sends_)
    return;

  // Receive a sent message. Cleanup scenarios:
  // - thread sending exits: We don't allow this, since thread can exit
  //   only via Join, so Send must complete.
  // - thread receiving exits: Wakeup/set ready in Thread::Clear()
  // - object target cleared: Wakeup/set ready in Thread::Clear()
  crit_.Enter();
  while (!sendlist_.empty()) {
    _SendMessage smsg = sendlist_.front();
    sendlist_.pop_front();
    crit_.Leave();
    smsg.msg.phandler->OnMessage(&smsg.msg);
    crit_.Enter();
    *smsg.ready = true;
    smsg.thread->socketserver()->WakeUp();
  }
  has_sends_ = false;
  crit_.Leave();
}

void Thread::Clear(MessageHandler *phandler, uint32 id) {
  CritScope cs(&crit_);

  // Remove messages on sendlist_ with phandler
  // Object target cleared: remove from send list, wakeup/set ready
  // if sender not NULL.

  std::list<_SendMessage>::iterator iter = sendlist_.begin();
  while (iter != sendlist_.end()) {
    _SendMessage smsg = *iter;
    if (phandler == NULL || smsg.msg.phandler == phandler) {
      if (id == (uint32)-1 || smsg.msg.message_id == id) {
        iter = sendlist_.erase(iter);
        *smsg.ready = true;
        smsg.thread->socketserver()->WakeUp();
        continue;
      }
    }
    ++iter;
  }

  MessageQueue::Clear(phandler, id);
}

AutoThread::AutoThread(SocketServer* ss) : Thread(ss) {
  if (!ThreadManager::CurrentThread()) {
    ThreadManager::SetCurrent(this);
  }
}

AutoThread::~AutoThread() {
  if (ThreadManager::CurrentThread() == this) {
    ThreadManager::SetCurrent(NULL);
  }
}

} // namespace cricket

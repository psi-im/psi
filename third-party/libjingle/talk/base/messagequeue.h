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

#ifndef __MESSAGEQUEUE_H__
#define __MESSAGEQUEUE_H__

#include "talk/base/basictypes.h"
#include "talk/base/criticalsection.h"
#include "talk/base/socketserver.h"
#include "talk/base/time.h"
#include <vector>
#include <queue>
#include <algorithm>

namespace cricket {

struct Message;
class MessageQueue;
class MessageHandler;

// MessageQueueManager does cleanup of of message queues

class MessageQueueManager {
public:
  static MessageQueueManager* Instance();

  void Add(MessageQueue *message_queue);
  void Remove(MessageQueue *message_queue);
  void Clear(MessageHandler *handler);

private:
  MessageQueueManager();
  ~MessageQueueManager();

  static MessageQueueManager* instance_;
  std::vector<MessageQueue *> message_queues_;
  CriticalSection crit_;
};

// Messages get dispatched to a MessageHandler

class MessageHandler {
public:
  virtual ~MessageHandler() {
    MessageQueueManager::Instance()->Clear(this);
  }

  virtual void OnMessage(Message *pmsg) = 0;
};

// Derive from this for specialized data
// App manages lifetime, except when messages are purged

class MessageData {
public:
  MessageData() {}
  virtual ~MessageData() {}
};

template <class arg1_type>
class TypedMessageData : public MessageData {
public:
  TypedMessageData(arg1_type data) {
    data_ = data;
  }
  arg1_type data() {
    return data_;
  }
private:
  arg1_type data_;
};

// No destructor

struct Message {
  Message() {
    memset(this, 0, sizeof(*this));
  }
  MessageHandler *phandler;
  uint32 message_id;
  MessageData *pdata;
};

// DelayedMessage goes into a priority queue, sorted by trigger time

class DelayedMessage {
public:
  DelayedMessage(int cmsDelay, Message *pmsg) {
    cmsDelay_ = cmsDelay;
    msTrigger_ = GetMillisecondCount() + cmsDelay;
    msg_ = *pmsg;
  }

  bool operator< (const DelayedMessage& dmsg) const {
    return dmsg.msTrigger_ < msTrigger_;
  }

  int cmsDelay_; // for debugging
  uint32 msTrigger_;
  Message msg_;
};

class MessageQueue {
public:
  MessageQueue(SocketServer* ss = 0);
  virtual ~MessageQueue();

  SocketServer* socketserver() { return ss_; }
  void set_socketserver(SocketServer* ss);

  // Once the queue is stopped, all calls to Get/Peek will return false.
  virtual void Stop();
  virtual bool IsStopping();
  virtual void Restart();

  virtual bool Get(Message *pmsg, int cmsWait = -1);
  virtual bool Peek(Message *pmsg, int cmsWait = 0);
  virtual void Post(MessageHandler *phandler, uint32 id = 0,
      MessageData *pdata = NULL);
  virtual void PostDelayed(int cmsDelay, MessageHandler *phandler,
      uint32 id = 0, MessageData *pdata = NULL);
  virtual void Clear(MessageHandler *phandler, uint32 id = (uint32)-1);
  virtual void Dispatch(Message *pmsg);
  virtual void ReceiveSends();
  virtual int GetDelay();

protected:
  SocketServer* ss_;
  bool new_ss;
  bool fStop_;
  bool fPeekKeep_;
  Message msgPeek_;
  std::queue<Message> msgq_;
  std::priority_queue<DelayedMessage> dmsgq_;
  CriticalSection crit_;
};

} // namespace cricket 

#endif // __MESSAGEQUEUE_H__

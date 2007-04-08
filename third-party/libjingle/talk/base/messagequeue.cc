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
#include "talk/base/messagequeue.h"
#include "talk/base/physicalsocketserver.h"

#ifdef POSIX
extern "C" {
#include <sys/time.h>
}
#endif

namespace cricket {

//------------------------------------------------------------------
// MessageQueueManager

MessageQueueManager* MessageQueueManager::instance_;

MessageQueueManager* MessageQueueManager::Instance() {
  // Note: This is not thread safe, but it is first called before threads are
  // spawned.
  if (!instance_)
    instance_ = new MessageQueueManager;
  return instance_;
}

MessageQueueManager::MessageQueueManager() {
}

MessageQueueManager::~MessageQueueManager() {
}

void MessageQueueManager::Add(MessageQueue *message_queue) {
  CritScope cs(&crit_);
  message_queues_.push_back(message_queue);
}

void MessageQueueManager::Remove(MessageQueue *message_queue) {
  CritScope cs(&crit_);
  std::vector<MessageQueue *>::iterator iter;
  iter = std::find(message_queues_.begin(), message_queues_.end(), message_queue);
  if (iter != message_queues_.end())
    message_queues_.erase(iter);
}

void MessageQueueManager::Clear(MessageHandler *handler) {
  CritScope cs(&crit_);
  std::vector<MessageQueue *>::iterator iter;
  for (iter = message_queues_.begin(); iter != message_queues_.end(); iter++)
    (*iter)->Clear(handler);
}

//------------------------------------------------------------------
// MessageQueue

MessageQueue::MessageQueue(SocketServer* ss)
    : ss_(ss), new_ss(false), fStop_(false), fPeekKeep_(false) {
  if (!ss_) {
    new_ss = true;
    ss_ = new PhysicalSocketServer();
  }
  MessageQueueManager::Instance()->Add(this);
}

MessageQueue::~MessageQueue() {
  Clear(NULL);
  if (new_ss)
    delete ss_;
  MessageQueueManager::Instance()->Remove(this);
}

void MessageQueue::set_socketserver(SocketServer* ss) {
  if (new_ss)
    delete ss_;
  new_ss = false;
  ss_ = ss;
}

void MessageQueue::Stop() {
  fStop_ = true;
  ss_->WakeUp();
}

bool MessageQueue::IsStopping() {
  return fStop_;
}

void MessageQueue::Restart() {
  fStop_ = false;
}

bool MessageQueue::Peek(Message *pmsg, int cmsWait) {
  if (fStop_)
    return false;
  if (fPeekKeep_) {
    *pmsg = msgPeek_;
    return true;
  }
  if (!Get(pmsg, cmsWait))
    return false;
  msgPeek_ = *pmsg;
  fPeekKeep_ = true;
  return true;
}

bool MessageQueue::Get(Message *pmsg, int cmsWait) {
  // Force stopping

  if (fStop_)
    return false;

  // Return and clear peek if present
  // Always return the peek if it exists so there is Peek/Get symmetry

  if (fPeekKeep_) {
    *pmsg = msgPeek_;
    fPeekKeep_ = false;
    return true;
  }

  // Get w/wait + timer scan / dispatch + socket / event multiplexer dispatch

  int cmsTotal = cmsWait;
  int cmsElapsed = 0;
  uint32 msStart = GetMillisecondCount();
  uint32 msCurrent = msStart;
  while (!fStop_) {
    // Check for sent messages

    ReceiveSends();

    // Check queues

    int cmsDelayNext = -1;
    {
      CritScope cs(&crit_);

      // Check for delayed messages that have been triggered
      // Calc the next trigger too

      while (!dmsgq_.empty()) {
        if (msCurrent < dmsgq_.top().msTrigger_) {
          cmsDelayNext = dmsgq_.top().msTrigger_ - msCurrent;
          break;
        }
        msgq_.push(dmsgq_.top().msg_);
        dmsgq_.pop();
      }

      // Check for posted events

      if (!msgq_.empty()) {
        *pmsg = msgq_.front();
        msgq_.pop();
        return true;
      }
    }

    // Which is shorter, the delay wait or the asked wait?

    int cmsNext;
    if (cmsWait == -1) {
      cmsNext = cmsDelayNext;
    } else {
      cmsNext = cmsTotal - cmsElapsed;
      if (cmsNext < 0)
        cmsNext = 0;
      if (cmsDelayNext != -1 && cmsDelayNext < cmsNext)
        cmsNext = cmsDelayNext;
    }

    // Wait and multiplex in the meantime
    ss_->Wait(cmsNext, true);

    // If the specified timeout expired, return

    msCurrent = GetMillisecondCount();
    cmsElapsed = msCurrent - msStart;
    if (cmsWait != -1) {
      if (cmsElapsed >= cmsWait)
        return false;
    }
  }
  return false;
}

void MessageQueue::ReceiveSends() {
}

void MessageQueue::Post(MessageHandler *phandler, uint32 id,
    MessageData *pdata) {
  // Keep thread safe
  // Add the message to the end of the queue
  // Signal for the multiplexer to return

  CritScope cs(&crit_);
  Message msg;
  msg.phandler = phandler;
  msg.message_id = id;
  msg.pdata = pdata;
  msgq_.push(msg);
  ss_->WakeUp();
}

void MessageQueue::PostDelayed(int cmsDelay, MessageHandler *phandler,
    uint32 id, MessageData *pdata) {
  // Keep thread safe
  // Add to the priority queue. Gets sorted soonest first.
  // Signal for the multiplexer to return.

  CritScope cs(&crit_);
  Message msg;
  msg.phandler = phandler;
  msg.message_id = id;
  msg.pdata = pdata;
  dmsgq_.push(DelayedMessage(cmsDelay, &msg));
  ss_->WakeUp();
}

int MessageQueue::GetDelay() {
  CritScope cs(&crit_);

  if (!msgq_.empty())
    return 0;

  if (!dmsgq_.empty()) {
    int delay = dmsgq_.top().msTrigger_ - GetMillisecondCount();
    if (delay < 0)
      delay = 0;
    return delay;
  }

  return -1;
}

void MessageQueue::Clear(MessageHandler *phandler, uint32 id) {
  CritScope cs(&crit_);

  // Remove messages with phandler

  if (fPeekKeep_) {
    if (phandler == NULL || msgPeek_.phandler == phandler) {
      if (id == (uint32)-1 || msgPeek_.message_id == id) {
        delete msgPeek_.pdata;
        fPeekKeep_ = false;
      }
    }
  }

  // Remove from ordered message queue

  size_t c = msgq_.size();
  while (c-- != 0) {
    Message msg = msgq_.front();
    msgq_.pop();
    if (phandler != NULL && msg.phandler != phandler) {
      msgq_.push(msg);
    } else {
      if (id == (uint32)-1 || msg.message_id == id) {
        delete msg.pdata;
      } else {
        msgq_.push(msg);
      }
    }
  }

  // Remove from priority queue. Not directly iterable, so use this approach

  std::queue<DelayedMessage> dmsgs;
  while (!dmsgq_.empty()) {
    DelayedMessage dmsg = dmsgq_.top();
    dmsgq_.pop();
    if (phandler != NULL && dmsg.msg_.phandler != phandler) {
      dmsgs.push(dmsg);
    } else {
      if (id == (uint32)-1 || dmsg.msg_.message_id == id) {
        delete dmsg.msg_.pdata;
      } else {
        dmsgs.push(dmsg);
      }
    }
  }
  while (!dmsgs.empty()) {
    dmsgq_.push(dmsgs.front());
    dmsgs.pop();
  }
}

void MessageQueue::Dispatch(Message *pmsg) {
  pmsg->phandler->OnMessage(pmsg);
}

} // namespace cricket

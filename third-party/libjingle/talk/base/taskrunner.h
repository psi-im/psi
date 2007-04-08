/*
 * libjingle
 * Copyright 2004--2006, Google Inc.
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

#ifndef TALK_BASE_TASKRUNNER_H__
#define TALK_BASE_TASKRUNNER_H__

#include <vector>

#include "talk/base/sigslot.h"
#include "talk/base/task.h"

namespace buzz {

class Task;

class TaskRunner : public Task, public sigslot::has_slots<> {
 public:
  TaskRunner();
  virtual ~TaskRunner();

  virtual void WakeTasks() = 0;
  virtual int64 CurrentTime() = 0 ;

  void StartTask(Task *task);
  void RunTasks();
  void PollTasks();

  void UpdateTaskTimeout(Task *task);

  // dummy state machine - never run.
  virtual int ProcessStart() { return STATE_DONE; }

 private:
  std::vector<Task *> tasks_;
  Task *next_timeout_task_;
  bool tasks_running_;

  void RecalcNextTimeout(Task *exclude_task);
};
}

#endif  // TASK_BASE_TASKRUNNER_H__

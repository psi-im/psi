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

#include <algorithm>

#include "talk/base/taskrunner.h"

#include "talk/base/common.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/task.h"
#include "talk/base/logging.h"

namespace buzz {

TaskRunner::TaskRunner()
    : Task(NULL),
      tasks_running_(false),
      next_timeout_task_(NULL) {
}

TaskRunner::~TaskRunner() {
  // this kills and deletes children silently!
  AbortAllChildren();
  RunTasks();
}

void TaskRunner::StartTask(Task * task) {
  tasks_.push_back(task);

  // the task we just started could be about to timeout --
  // make sure our "next timeout task" is correct
  UpdateTaskTimeout(task);

  WakeTasks();
}

void TaskRunner::RunTasks() {
  // Running continues until all tasks are Blocked (ok for a small # of tasks)
  if (tasks_running_) {
    return;  // don't reenter
  }

  tasks_running_ = true;

  int did_run = true;
  while (did_run) {
    did_run = false;
    // use indexing instead of iterators because tasks_ may grow
    for (size_t i = 0; i < tasks_.size(); ++i) {
      while (!tasks_[i]->Blocked()) {
        tasks_[i]->Step();
        did_run = true;
      }
    }
  }
  // Tasks are deleted when running has paused
  bool need_timeout_recalc = false;
  for (size_t i = 0; i < tasks_.size(); ++i) {
    if (tasks_[i]->IsDone()) {
      Task* task = tasks_[i];
      if (next_timeout_task_ &&
          task->get_unique_id() == next_timeout_task_->get_unique_id()) {
        next_timeout_task_ = NULL;
        need_timeout_recalc = true;
      }

      delete task;
      tasks_[i] = NULL;
    }
  }
  // Finally, remove nulls
  std::vector<Task *>::iterator it;
  it = std::remove(tasks_.begin(),
                   tasks_.end(),
                   reinterpret_cast<Task *>(NULL));

  tasks_.erase(it, tasks_.end());

  if (need_timeout_recalc)
    RecalcNextTimeout(NULL);

  tasks_running_ = false;
}

void TaskRunner::PollTasks() {
  // see if our "next potentially timed-out task" has indeed timed out.
  // If it has, wake it up, then queue up the next task in line
  if (next_timeout_task_ &&
      next_timeout_task_->TimedOut()) {
    next_timeout_task_->Wake();
    WakeTasks();
  }
}

// this function gets called frequently -- when each task changes
// state to something other than DONE, ERROR or BLOCKED, it calls
// ResetTimeout(), which will call this function to make sure that
// the next timeout-able task hasn't changed.  The logic in this function
// prevents RecalcNextTimeout() from getting called in most cases,
// effectively making the task scheduler O-1 instead of O-N

void TaskRunner::UpdateTaskTimeout(Task *task) {
  ASSERT(task != NULL);

  // if the relevant task has a timeout, then
  // check to see if it's closer than the current
  // "about to timeout" task
  if (task->get_timeout_time()) {
    if (next_timeout_task_ == NULL ||
        (task->get_timeout_time() <=
         next_timeout_task_->get_timeout_time())) {
      next_timeout_task_ = task;
    }
  } else if (next_timeout_task_ != NULL &&
             task->get_unique_id() == next_timeout_task_->get_unique_id()) {
    // otherwise, if the task doesn't have a timeout,
    // and it used to be our "about to timeout" task,
    // walk through all the tasks looking for the real
    // "about to timeout" task
    RecalcNextTimeout(task);
  }
}

void TaskRunner::RecalcNextTimeout(Task *exclude_task) {
  // walk through all the tasks looking for the one
  // which satisfies the following:
  //   it's not finished already
  //   we're not excluding it
  //   it has the closest timeout time

  int64 next_timeout_time = 0;
  next_timeout_task_ = NULL;

  for (size_t i = 0; i < tasks_.size(); ++i) {
    Task *task = tasks_[i];
    // if the task isn't complete, and it actually has a timeout time
    if (!task->IsDone() &&
        (task->get_timeout_time() > 0))
      // if it doesn't match our "exclude" task
      if (exclude_task == NULL ||
          exclude_task->get_unique_id() != task->get_unique_id())
        // if its timeout time is sooner than our current timeout time
        if (next_timeout_time == 0 ||
            task->get_timeout_time() <= next_timeout_time) {
          // set this task as our next-to-timeout
          next_timeout_time = task->get_timeout_time();
          next_timeout_task_ = task;
        }
  }
}
}

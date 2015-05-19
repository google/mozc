// Copyright 2010-2013, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/task_manager.h"

#include <map>

#include "base/base.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "base/task_token.h"
#include "base/thread.h"
#include "base/unnamed_event.h"
#include "base/util.h"

namespace mozc {

// Container of the task information such as the request and the response.
struct TaskManager::TaskInfo {
  enum TaskStatus {
    TASK_NOT_STARTED,
    TASK_RUNNING,
    TASK_COMPLETED,
    TASK_CANCELED,
  };
  TaskInfo(const TaskRequestInterface *request_interface,
           TaskManagerCallbackFunc callback_func,
           void *callback_data)
      : status(TASK_NOT_STARTED), request(request_interface), response(NULL),
        callback_func_(callback_func), callback_data_(callback_data),
        task_runner(NULL) {
  }
  // The status of the task.
  TaskStatus status;
  // The request object of the task filled in at the constructor.
  scoped_ptr<const TaskRequestInterface> request;
  // The response object of the task filled in at the task completion.
  scoped_ptr<const TaskResponseInterface> response;
  // The callback function which called at the task completion.
  // Set NULL if not needed.
  TaskManagerCallbackFunc callback_func_;
  // The first argument of the callback function.
  void *callback_data_;
  // The TaskRunner object which runs the task.
  // TaskInfo does not take the ownership.
  TaskRunner *task_runner;
};

TaskManager::TaskManager(TaskRunnerFactoryInterface *runner_factory)
    : runner_factory_(runner_factory), is_shutting_down_(false),
      new_task_event_(new UnnamedEvent), task_done_event_(new UnnamedEvent) {
}

TaskManager::~TaskManager() {
  int runnning_task_count = 0;
  {
    scoped_lock l(&mutex_);
    for (map<TaskToken, TaskInfo*>::iterator it = task_info_map_.begin();
         it != task_info_map_.end(); ++it) {
      DCHECK(it->second);
      if (it->second->status == TaskInfo::TASK_RUNNING) {
        DCHECK(it->second->task_runner);
        it->second->task_runner->CancelTask();
        ++runnning_task_count;
      }
    }
  }

  // Waits until the all |started_tasks| will be done.
  while (runnning_task_count != 0) {
    task_done_event_->Wait(-1);
    GetTaskStatusInfo(NULL, NULL, &runnning_task_count, NULL, NULL);
  }

  // Deletes the all data.
  {
    scoped_lock l(&mutex_);
    for (map<TaskToken, TaskInfo*>::iterator it = task_info_map_.begin();
         it != task_info_map_.end(); ++it) {
      delete it->second;
    }
  }
}

void TaskManager::OnTaskDone(const TaskRunner *task_runner,
                             const TaskToken &token,
                             const TaskRequestInterface *request,
                             const TaskResponseInterface *response) {
  scoped_lock l(&mutex_);
  TaskInfo *task_info = GetTaskInfo(token);
  if (task_info == NULL) {
    delete response;
    return;
  }
  if (task_info->status == TaskInfo::TASK_CANCELED) {
    delete response;
    delete task_info;
    task_info_map_.erase(token);
    task_done_event_->Notify();
    return;
  }
  task_info->status = TaskInfo::TASK_COMPLETED;
  task_info->response.reset(response);
  task_info->task_runner = NULL;  // task_runner will be deleted by itself.
  if (task_info->callback_func_ != NULL) {
    if ((task_info->callback_func_)(this, token, request, response,
                                    task_info->callback_data_)) {
      delete task_info;
      task_info_map_.erase(token);
    }
  }
  task_done_event_->Notify();
}

TaskToken TaskManager::AddTask(const TaskRequestInterface *request,
                               TaskManagerCallbackFunc callback_func,
                               void *callback_data) {
  scoped_lock l(&mutex_);
  const TaskToken token = token_manager_.NewToken();
  task_info_map_.insert(
      make_pair(token, new TaskInfo(request, callback_func, callback_data)));
  new_task_event_->Notify();
  return token;
}

bool TaskManager::UnregisterTaskCallback(const TaskToken &token) {
  scoped_lock l(&mutex_);
  TaskInfo *task_info = GetTaskInfo(token);
  if (task_info == NULL) {
    return false;
  }
  task_info->callback_func_ = NULL;
  task_info->callback_data_ = NULL;
  return true;
}

bool TaskManager::StartTask(const TaskToken &token) {
  TaskRunner *task_runner = NULL;
  {
    scoped_lock l(&mutex_);
    TaskInfo *task_info = GetTaskInfo(token);
    if (task_info == NULL) {
      return false;
    }
    if (task_info->status != TaskInfo::TASK_NOT_STARTED) {
      return false;
    }
    task_runner = runner_factory_->NewRunner(token, task_info->request.get(),
                                             this);
    task_info->task_runner = task_runner;
    task_info->status = TaskInfo::TASK_RUNNING;
  }
  task_runner->StartTask();
  return true;
}

bool TaskManager::StartOldestTask(TaskToken *token) {
  TaskRunner *task_runner = NULL;
  {
    scoped_lock l(&mutex_);
    if (is_shutting_down_) {
      new_task_event_->Notify();  // Chains the notification to other threads.
      return false;
    }

    map<TaskToken, TaskInfo*>::iterator it = task_info_map_.begin();
    while (it != task_info_map_.end()) {
      DCHECK(it->second);
      if (it->second->status == TaskInfo::TASK_NOT_STARTED) {
        break;
      }
      ++it;
    }

    if (it != task_info_map_.end()) {
      *token = it->first;
      task_runner = runner_factory_->NewRunner(it->first,
                                               it->second->request.get(), this);
      it->second->task_runner = task_runner;
      it->second->status = TaskInfo::TASK_RUNNING;
    }
  }
  if (task_runner == NULL) {
    return false;
  }
  task_runner->StartTask();
  return true;
}

bool TaskManager::CancelTask(const TaskToken &token) {
  scoped_lock l(&mutex_);
  TaskInfo *task_info = GetTaskInfo(token);
  if (task_info == NULL) {
    return false;
  }
  if (task_info->status != TaskInfo::TASK_RUNNING) {
    delete task_info;
    task_info_map_.erase(token);
    return true;
  }
  task_info->callback_func_ = NULL;
  task_info->status = TaskInfo::TASK_CANCELED;
  task_info->task_runner->CancelTask();
  return true;
}

bool TaskManager::WaitForNewTaskEvent(int msec) {
  return new_task_event_->Wait(msec);
}

bool TaskManager::TakeCompletedTask(
    const TaskToken &token, scoped_ptr<const TaskRequestInterface> *request,
    scoped_ptr<const TaskResponseInterface> *response) {
  scoped_lock l(&mutex_);
  TaskInfo *task_info = GetTaskInfo(token);
  if (task_info == NULL) {
    return false;
  }
  if (task_info->status != TaskInfo::TASK_COMPLETED) {
    return false;
  }
  task_info->response.swap(*response);
  task_info->request.swap(*request);
  delete task_info;
  task_info_map_.erase(token);
  return true;
}

void TaskManager::GetTaskStatusInfo(int *total_task_count,
                                    int *not_started_task_count,
                                    int *running_task_count,
                                    int *completed_task_count,
                                    int *canceled_task_count) {
  scoped_lock l(&mutex_);
  int total = 0;
  int not_started = 0;
  int running = 0;
  int completed = 0;
  int canceled = 0;
  for (map<TaskToken, TaskInfo*>::iterator it = task_info_map_.begin();
       it != task_info_map_.end(); ++it) {
    ++total;
    DCHECK(it->second);
    switch (it->second->status) {
      case TaskInfo::TASK_NOT_STARTED:
        ++not_started;
        break;
      case TaskInfo::TASK_RUNNING:
        ++running;
        break;
      case TaskInfo::TASK_COMPLETED:
        ++completed;
        break;
      case TaskInfo::TASK_CANCELED:
        ++canceled;
        break;
      default:
        LOG(WARNING) << "Unknown task status: " << it->second->status;
        break;
    }
  }
  if (total_task_count != NULL) {
    *total_task_count = total;
  }
  if (not_started_task_count != NULL) {
    *not_started_task_count = not_started;
  }
  if (running_task_count != NULL) {
    *running_task_count = running;
  }
  if (completed_task_count != NULL) {
    *completed_task_count = completed;
  }
  if (canceled_task_count != NULL) {
    *canceled_task_count = canceled;
  }
}

bool TaskManager::IsShuttingDown() {
  scoped_lock l(&mutex_);
  return is_shutting_down_;
}

void TaskManager::ShutDown() {
  scoped_lock l(&mutex_);
  is_shutting_down_ = true;
  new_task_event_->Notify();
}

TaskManager::TaskInfo* TaskManager::GetTaskInfo(const TaskToken &token) {
  map<TaskToken, TaskInfo*>::iterator it = task_info_map_.find(token);
  if (it == task_info_map_.end()) {
    return NULL;
  }
  return it->second;
}

}  // namespace mozc

// Copyright 2010-2014, Google Inc.
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

#ifndef MOZC_BASE_TASK_RUNNER_H_
#define MOZC_BASE_TASK_RUNNER_H_

#include <map>

#include "base/mutex.h"
#include "base/port.h"
#include "base/task_token.h"

namespace mozc {

class TaskManager;
class TaskRunner;

class TaskRequestInterface {
 public:
  virtual ~TaskRequestInterface() {}
};

class TaskResponseInterface {
 public:
  virtual ~TaskResponseInterface() {}
};

class TaskRunnerCallbackInterface {
 public:
  virtual ~TaskRunnerCallbackInterface() {}
  // The callback object cannot own the request object.
  // The callback object owns the response object and have to release it.
  virtual void OnTaskDone(const TaskRunner *task_runner,
                          const TaskToken &token,
                          const TaskRequestInterface *request,
                          const TaskResponseInterface *response) = 0;
};

class TaskRunner {
 public:
  virtual void StartTask() = 0;

  const TaskToken token() const;
  virtual const TaskRequestInterface *request() const;

  void CancelTask();
  bool canceled();

  void DestroyTaskRunner();

 protected:
  // TaskRunner is designed to be deleted using "delete this" in CompleteTask
  // or DestroyTaskRunner. In order to prevent auto variable of TaskRunner being
  // declared, the constructor and the destructor of TaskRunner is in protected.
  TaskRunner(const TaskToken &token,
             const TaskRequestInterface *request,
             TaskRunnerCallbackInterface *callback);
  TaskRunner(const TaskToken &token,
             const TaskRequestInterface *request,
             TaskManager *task_manager);
  virtual ~TaskRunner();

  // TaskRunner has to call CompleteTask() when the task has finished or
  // has been canceled.
  void CompleteTask(TaskResponseInterface *response);

 private:
  Mutex mutex_;
  const TaskToken token_;
  const TaskRequestInterface* const request_;  // Does not take the ownership.
  TaskRunnerCallbackInterface* const callback_;  // Does not take the ownership.
  TaskManager* const task_manager_;  // Does not take the ownership.

  bool canceled_ GUARDED_BY(mutex_);
  DISALLOW_IMPLICIT_CONSTRUCTORS(TaskRunner);
};

}  // namespace mozc

#endif  // MOZC_BASE_TASK_RUNNER_H_

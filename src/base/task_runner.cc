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

#include "base/task_runner.h"

#include "base/mutex.h"
#include "base/port.h"
#include "base/task_manager.h"
#include "base/task_token.h"

namespace mozc {

const TaskToken TaskRunner::token() const {
  return token_;
}

const TaskRequestInterface *TaskRunner::request() const {
  return request_;
}

void TaskRunner::CancelTask() {
  scoped_lock l(&mutex_);
  canceled_ = true;
}

bool TaskRunner::canceled() {
  scoped_lock l(&mutex_);
  return canceled_;
}

void TaskRunner::DestroyTaskRunner() {
  delete this;
}

TaskRunner::TaskRunner(
    const TaskToken &token, const TaskRequestInterface *request,
    TaskRunnerCallbackInterface *callback)
    : token_(token), request_(request), callback_(callback),
      task_manager_(NULL), canceled_(false) {
}

TaskRunner::TaskRunner(
    const TaskToken &token, const TaskRequestInterface *request,
    TaskManager *task_manager)
    : token_(token), request_(request), callback_(NULL),
      task_manager_(task_manager), canceled_(false) {
}

TaskRunner::~TaskRunner() {}

void TaskRunner::CompleteTask(TaskResponseInterface *response) {
  if (callback_ != NULL) {
    callback_->OnTaskDone(this, token_, request_, response);
  } else if (task_manager_ != NULL) {
    task_manager_->OnTaskDone(this, token_, request_, response);
  }
  delete this;
}

}  // namespace mozc

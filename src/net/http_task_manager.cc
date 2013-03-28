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

#include "net/http_task_manager.h"

#include "base/base.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/task_runner.h"
#include "base/task_token.h"
#include "base/task_manager.h"
#include "base/thread.h"
#include "net/http_client.h"

namespace mozc {
namespace net {
namespace {

// HTTPClientRunner is created by HTTPTaskManager using HTTPClientRunnerFactory
// in TaskManager::StartTask(). And it will be deleted by itsef in
// TaskRunner::CompleteTask() when the task has been completed.
class HTTPClientRunner : public TaskRunner {
 public:
  static HTTPClientRunner *Create(const TaskToken &token,
                                  const HttpTaskRequest *request,
                                  TaskManager *task_manager) {
    return new HTTPClientRunner(token, request, task_manager);
  }

  virtual void StartTask();

  virtual const HttpTaskRequest *request() const {
    return static_cast<const HttpTaskRequest *>(TaskRunner::request());
  }

 private:
  class DetachedDownloadThread : public DetachedThread {
   public:
    DetachedDownloadThread(const HttpTaskRequest *request,
                           HTTPClientRunner *runner)
        : request_(request), runner_(runner) {
    }
    virtual void Run();

   private:
    const HttpTaskRequest *request_;
    HTTPClientRunner *runner_;
    DISALLOW_COPY_AND_ASSIGN(DetachedDownloadThread);
  };

  //  In order to prevent auto variable of TaskRunner being declared,
  //  the constructor and the destructor of TaskRunner must be in private.
  HTTPClientRunner(
      const TaskToken &token, const HttpTaskRequest *request,
      TaskManager *task_manager)
      : TaskRunner(token, request, task_manager) {
  }
  virtual ~HTTPClientRunner() {}

  DISALLOW_COPY_AND_ASSIGN(HTTPClientRunner);
};

void HTTPClientRunner::StartTask() {
  DetachedDownloadThread *thread =
      new DetachedDownloadThread(request(), this);
  thread->Start();
}

void HTTPClientRunner::DetachedDownloadThread::Run() {
  string output;
  bool result;
  switch (request_->type()) {
    case HTTP_GET:
      result = HTTPClient::Get(request_->url(), &output);
      break;
    case HTTP_HEAD:
      result = HTTPClient::Head(request_->url(), &output);
      break;
    case HTTP_POST:
      result = HTTPClient::Post(request_->url(), request_->data(), &output);
      break;
    default:
      LOG(ERROR) << "Unknown method: " << request_->type();
      result = false;
      break;
  }
  runner_->CompleteTask(new HttpTaskResponse(result, output));
}

class HTTPClientRunnerFactory : public TaskRunnerFactoryInterface {
 public:
  HTTPClientRunnerFactory() {}
  virtual HTTPClientRunner *NewRunner(
      const TaskToken &token, const TaskRequestInterface *request,
      TaskManager *task_manager) {
    return HTTPClientRunner::Create(
        token,
        static_cast<const HttpTaskRequest *>(request), task_manager);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HTTPClientRunnerFactory);
};

}  // namespace

HTTPTaskManager::HTTPTaskManager()
    : TaskManager(new HTTPClientRunnerFactory()) {
}

HTTPTaskManager *HTTPTaskManager::GetInstance() {
  return Singleton<HTTPTaskManager>::get();
}

}  // namespace net
}  // namespace mozc


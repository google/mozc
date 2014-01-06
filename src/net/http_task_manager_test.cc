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

#include "net/http_task_manager.h"

#include "base/base.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/task_runner.h"
#include "base/task_token.h"
#include "base/thread.h"
#include "base/util.h"
#include "net/http_client_mock.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

using testing::_;
using testing::Invoke;

namespace mozc {
namespace net {
namespace {

class DownloadTaskCallback {
 public:
  DownloadTaskCallback() {
    callcount_ = 0;
  }
  static bool OnTaskDone(const TaskManager *task_manager,
                         const TaskToken &token,
                         const TaskRequestInterface *request,
                         const TaskResponseInterface *response,
                         void *callback_data) {
    DownloadTaskCallback *self =
        static_cast<DownloadTaskCallback*>(callback_data);
    {
      scoped_lock l(&self->mutex_);
      ++self->callcount_;
    }
    return self->OnDownloadDone(
        static_cast<const HttpTaskRequest *>(request),
        static_cast<const HttpTaskResponse *>(response));
  }

  MOCK_METHOD2(OnDownloadDone,
               bool(const HttpTaskRequest*, const HttpTaskResponse*));

  bool OnDownloadDone1(const HttpTaskRequest *request,
                       const HttpTaskResponse *response) {
    EXPECT_TRUE(response->result());
    EXPECT_EQ("rest result", response->output());
    return true;
  }

  bool OnDownloadDone2(const HttpTaskRequest *request,
                       const HttpTaskResponse *response) {
    EXPECT_FALSE(response->result());
    return true;
  }

  bool OnDownloadDone3(const HttpTaskRequest *request,
                       const HttpTaskResponse *response) {
    EXPECT_TRUE(response->result());
    EXPECT_EQ("rest result", response->output());
    return true;
  }

  int32 callcount() {
    scoped_lock l(&mutex_);
    return callcount_;
  }

 private:
  int32 callcount_;
  Mutex mutex_;
  DISALLOW_COPY_AND_ASSIGN(DownloadTaskCallback);
};

}  // namespace

TEST(HTTPTaskManager, SimpleTest) {
  HTTPClientMock client;
  HTTPClientMock::Result result;
  result.expected_url = "http://www.google.com/";
  result.expected_request = "";
  result.expected_result = "rest result";
  client.set_result(result);
  client.set_execution_time(500);
  HTTPClient::SetHTTPClientHandler(&client);

  DownloadTaskCallback callback;
  EXPECT_CALL(callback, OnDownloadDone(_, _))
      .WillOnce(Invoke(&callback, &DownloadTaskCallback::OnDownloadDone3))
      .WillOnce(Invoke(&callback, &DownloadTaskCallback::OnDownloadDone2))
      .WillOnce(Invoke(&callback, &DownloadTaskCallback::OnDownloadDone1));

  HTTPTaskManager *http_task_manager = HTTPTaskManager::GetInstance();
  ASSERT_TRUE(http_task_manager);
  const TaskToken token1 = http_task_manager->AddTask(
      new HttpTaskRequest(mozc::HTTP_GET, "http://www.google.com/", "",
                          HTTPClient::Option()),
      DownloadTaskCallback::OnTaskDone, &callback);
  EXPECT_EQ(0, callback.callcount());
  http_task_manager->StartTask(token1);
  EXPECT_EQ(0, callback.callcount());
  Util::Sleep(300);
  EXPECT_EQ(0, callback.callcount());
  Util::Sleep(700);
  EXPECT_EQ(1, callback.callcount());

  const TaskToken token2 = http_task_manager->AddTask(
      new HttpTaskRequest(mozc::HTTP_GET, "http://www.google.com/dummy", "",
                          HTTPClient::Option()),
      DownloadTaskCallback::OnTaskDone, &callback);

  const TaskToken token3 = http_task_manager->AddTask(
      new HttpTaskRequest(mozc::HTTP_GET, "http://www.google.com/", "",
                          HTTPClient::Option()),
      DownloadTaskCallback::OnTaskDone, &callback);
  http_task_manager->StartTask(token2);
  Util::Sleep(300);
  http_task_manager->StartTask(token3);
  Util::Sleep(1000);
  EXPECT_EQ(3, callback.callcount());
}

}  // namespace net
}  // namespace mozc

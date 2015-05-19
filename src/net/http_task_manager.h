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

// HTTPTaskManager provides a way to call HTTP request asynchronously.
//
// TODO(horo): Currently we use DetachedThread to call HTTP request
// asynchronously. But this implementation will be replaced with the
// asynchronous network API which is provided in the each platform.
//
// How to use HTTPTaskManager:
//  1. Declare download callback function.
//  bool OnDownloadDone(const TaskManager *task_manager,
//                      const TaskToken &token,
//                      const TaskRequestInterface *request,
//                      const TaskResponseInterface *response,
//                      void *callback_data) {
//    // Just shows the output.
//    cout << static_cast<const HttpTaskResponse*>(response)->output();
//  }
//
//  2. Get the instance of HTTPTaskManager.
//  HTTPTaskManager *http_task_manager = HTTPTaskManager::GetInstance();
//
//  3. Add a HTTP request task.
//  TaskToken http_task_token =
//      http_task_manager->AddTask(
//          new HttpTaskRequest(mozc::HTTP_GET, "http://www.google.com/",  "",
//                              HTTPClient::Option()),
//          OnDownloadDone, this);
//
//  4. Start the HTTP request task.
//  http_client_task_manager->StartTask(http_task_token);
//
//  5. The registered callback function will be called.
//  The download callback function will be called when the all HTTP response has
//  been recieved or an error has occured. The thread in which the callback
//  function will be called may differ from the thread in which you call
//  StartTask. So, you have to consider thread-safety.
//
//  5. Cancel the HTTP request task.
//  If you don't want the callback function to be called anymore such as
//  the callback object will be deleted, you have to cancel the task.
//  http_client_task_manager->CancelTask(http_task_token);

#ifndef MOZC_NET_HTTP_TASK_MANAGER_H_
#define MOZC_NET_HTTP_TASK_MANAGER_H_

#include <string>

#include "base/task_manager.h"
#include "net/http_client.h"
#include "net/http_client_common.h"

namespace mozc {
class TaskToken;

namespace net {
class HttpTaskRequest : public TaskRequestInterface {
 public:
  HttpTaskRequest(const HTTPMethodType &type,
                  const string &url, const string &data,
                  const HTTPClient::Option &option)
      : type_(type), url_(url), data_(data), option_(option) {
  }

  const HTTPMethodType &type() const { return type_; }
  const string &url() const { return url_; }
  const string &data() const { return data_; }
  const HTTPClient::Option &option() const { return option_; }

 private:
  HTTPMethodType type_;
  string url_;
  string data_;
  HTTPClient::Option option_;
  DISALLOW_COPY_AND_ASSIGN(HttpTaskRequest);
};

class HttpTaskResponse : public TaskResponseInterface {
 public:
  HttpTaskResponse(bool result, const string &output)
      : result_(result), output_(output) {
  }

  // Returns false if an error has occured.
  bool result() const { return result_; }

  // Returns the output of the HTTP method.
  const string &output() const { return output_; }

 private:
  const bool result_;
  const string output_;
  DISALLOW_COPY_AND_ASSIGN(HttpTaskResponse);
};

class HTTPTaskManager : public TaskManager {
 public:
  HTTPTaskManager();
  static HTTPTaskManager *GetInstance();

 private:
  DISALLOW_COPY_AND_ASSIGN(HTTPTaskManager);
};

}  // namespace net
}  // namespace mozc

#endif  // MOZC_NET_HTTP_TASK_MANAGER_H_

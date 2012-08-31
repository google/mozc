// Copyright 2010-2012, Google Inc.
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

#include <map>
#include <vector>

#include "base/base.h"
#include "base/mutex.h"
#include "base/stl_util.h"
#include "base/task_token.h"
#include "base/thread.h"
#include "base/unnamed_event.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

class TestRequest : public TaskRequestInterface {
 public:
  TestRequest() : initial_sleep_msec_(0) {}
  explicit TestRequest(const string &data)
      : initial_sleep_msec_(0), data_(data) {}

  const string &data() const {
    return data_;
  }
  void set_data(const string &new_data) {
    data_ = new_data;
  }
  uint32 initial_sleep_msec() const {
    return initial_sleep_msec_;
  }
  void set_initial_sleep_msec(uint32 msec) {
    initial_sleep_msec_ = msec;
  }

 private:
  uint32 initial_sleep_msec_;
  string data_;
};

class TestResponse : public TaskResponseInterface {
 public:
  TestResponse() {}
  explicit TestResponse(const string &data)
      : data_(data) {}
  const string &data() const {
    return data_;
  }
  void set_data(const string &new_data) {
    data_ = new_data;
  }

 private:
  string data_;
};

class TestRunner : public TaskRunner {
 public:
  static TestRunner* Create(const TaskToken &token, const TestRequest *request,
                            TaskRunnerCallbackInterface *callback) {
    return new TestRunner(token, request, callback);
  }

  virtual const TestRequest *request() const {
    return dynamic_cast<const TestRequest*>(TaskRunner::request());
  }

  virtual void StartTask() {
    if (request()->initial_sleep_msec() != 0) {
      Util::Sleep(request()->initial_sleep_msec());
    }
    if (request()->data() == "CompleteInStartTask") {
      CompleteTask(new TestResponse("CompleteInStartTask:done"));
    }
  }
  void FinishTask(const string &str) {
    if (canceled()) {
      CompleteTask(new TestResponse(request()->data() + ":cancelled:" + str));
    } else {
      CompleteTask(new TestResponse(request()->data() + ":" + str));
    }
  }

 private:
  //  In order to prevent auto variable of TaskRunner being declared,
  //  the constructor and the destructor of TaskRunner must be in private.
  TestRunner(const TaskToken &token, const TestRequest *request,
             TaskRunnerCallbackInterface *callback)
      : TaskRunner(token, request, callback) {
  }
  virtual ~TestRunner() {}
  DISALLOW_IMPLICIT_CONSTRUCTORS(TestRunner);
};


class TestCallback : public TaskRunnerCallbackInterface {
 public:
  TestCallback() {}
  virtual ~TestCallback() {
    STLDeleteElements(&response_vector_);
  }
  virtual void OnTaskDone(const TaskRunner *task_runner,
                          const TaskToken &token,
                          const TaskRequestInterface *request,
                          const TaskResponseInterface *response) {
    scoped_lock l(&mutex_);
    token_vector_.push_back(token);
    response_vector_.push_back(dynamic_cast<const TestResponse*>(response));
  }
  const vector<TaskToken> &token_vector() {
    return token_vector_;
  }
  const vector<const TestResponse*> &response_vector() {
    return response_vector_;
  }

 private:
  Mutex mutex_;
  vector<TaskToken> token_vector_;
  vector<const TestResponse*> response_vector_;
  DISALLOW_COPY_AND_ASSIGN(TestCallback);
};
}  // namespace

TEST(TestRunner, NormalTaskTest) {
  TestCallback callback;
  ThreadSafeTaskTokenManager token_manager;
  const TestRequest request = TestRequest("test");
  const TaskToken token = token_manager.NewToken();
  TestRunner *task_runner = TestRunner::Create(token, &request, &callback);
  task_runner->StartTask();
  EXPECT_EQ(0, callback.token_vector().size());
  EXPECT_EQ(0, callback.response_vector().size());
  task_runner->FinishTask("ok");
  ASSERT_EQ(1, callback.token_vector().size());
  ASSERT_EQ(1, callback.response_vector().size());
  EXPECT_TRUE(token == callback.token_vector()[0]);
  EXPECT_EQ("test:ok", callback.response_vector()[0]->data());
}

TEST(TestRunner, CompleteInStartTaskTest) {
  TestCallback callback;
  ThreadSafeTaskTokenManager token_manager;
  const TestRequest request = TestRequest("CompleteInStartTask");
  const TaskToken token = token_manager.NewToken();
  TestRunner *task_runner = TestRunner::Create(token, &request, &callback);
  task_runner->StartTask();
  ASSERT_EQ(1, callback.token_vector().size());
  ASSERT_EQ(1, callback.response_vector().size());
  EXPECT_TRUE(token == callback.token_vector()[0]);
  EXPECT_EQ("CompleteInStartTask:done", callback.response_vector()[0]->data());
}

TEST(TestRunner, MultiTaskTest) {
  TestCallback callback;
  ThreadSafeTaskTokenManager token_manager;
  const TestRequest request1 = TestRequest("test1");
  const TestRequest request2 = TestRequest("test2");
  const TestRequest request3 = TestRequest("test3");
  const TestRequest request4 = TestRequest("test4");
  const TaskToken token1 = token_manager.NewToken();
  const TaskToken token2 = token_manager.NewToken();
  const TaskToken token3 = token_manager.NewToken();
  const TaskToken token4 = token_manager.NewToken();
  TestRunner *task_runner1 = TestRunner::Create(token1, &request1, &callback);
  TestRunner *task_runner2 = TestRunner::Create(token2, &request2, &callback);
  TestRunner *task_runner3 = TestRunner::Create(token3, &request3, &callback);
  TestRunner *task_runner4 = TestRunner::Create(token4, &request4, &callback);

  task_runner1->StartTask();
  task_runner2->StartTask();
  task_runner3->StartTask();
  task_runner4->StartTask();
  EXPECT_EQ(0, callback.token_vector().size());
  EXPECT_EQ(0, callback.response_vector().size());
  task_runner1->FinishTask("ok");
  EXPECT_EQ(1, callback.token_vector().size());
  EXPECT_EQ(1, callback.response_vector().size());
  task_runner2->FinishTask("ok");
  EXPECT_EQ(2, callback.token_vector().size());
  EXPECT_EQ(2, callback.response_vector().size());
  task_runner4->FinishTask("ok");
  EXPECT_EQ(3, callback.token_vector().size());
  EXPECT_EQ(3, callback.response_vector().size());
  task_runner3->FinishTask("ok");
  ASSERT_EQ(4, callback.token_vector().size());
  ASSERT_EQ(4, callback.response_vector().size());

  EXPECT_TRUE(token1 == callback.token_vector()[0]);
  EXPECT_TRUE(token2 == callback.token_vector()[1]);
  EXPECT_TRUE(token4 == callback.token_vector()[2]);
  EXPECT_TRUE(token3 == callback.token_vector()[3]);
  EXPECT_EQ("test1:ok", callback.response_vector()[0]->data());
  EXPECT_EQ("test2:ok", callback.response_vector()[1]->data());
  EXPECT_EQ("test4:ok", callback.response_vector()[2]->data());
  EXPECT_EQ("test3:ok", callback.response_vector()[3]->data());
}

namespace {
static const int kNumThreads = 5;
static const int kNumTasksPerThread = 10000;

class FinishTaskCallerThread : public Thread {
 public:
  FinishTaskCallerThread(vector<TestRunner*> *task_runner_list,
                         uint32 initial_sleep_msec,
                         uint32 interval_sleep_msec)
      : task_runner_list_(task_runner_list),
        initial_sleep_msec_(initial_sleep_msec),
        interval_sleep_msec_(interval_sleep_msec) {}
  virtual ~FinishTaskCallerThread() {
  }
  virtual void Run() {
    if (initial_sleep_msec_ != 0) {
      Util::Sleep(initial_sleep_msec_);
    }
    for (size_t i = 0; i < task_runner_list_->size(); ++i) {
      if (interval_sleep_msec_ != 0) {
        Util::Sleep(interval_sleep_msec_);
      }
      (*task_runner_list_)[i]->FinishTask("ok");
    }
  }

 private:
  vector<TestRunner*>* const task_runner_list_;
  uint32 initial_sleep_msec_;
  uint32 interval_sleep_msec_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(FinishTaskCallerThread);
};
}  // namespace

TEST(TestRunner, MultiThreadTaskTest) {
  TestCallback callback;
  ThreadSafeTaskTokenManager token_manager;
  vector<FinishTaskCallerThread*> finish_caller_thread(kNumThreads);
  vector<vector<TestRunner*> > task_runner_list_list(kNumThreads);
  vector<vector<TestRequest> > request_list_list(kNumThreads);

  for (size_t i = 0; i < kNumThreads; ++i) {
    task_runner_list_list[i].resize(kNumTasksPerThread);
    request_list_list[i].resize(kNumTasksPerThread);

    for (size_t j = 0; j < kNumTasksPerThread; ++j) {
      const TaskToken token = token_manager.NewToken();
      request_list_list[i][j].set_data("test");
      task_runner_list_list[i][j] =
          TestRunner::Create(token, &request_list_list[i][j], &callback);
      task_runner_list_list[i][j]->StartTask();
    }
    finish_caller_thread[i] =
        new FinishTaskCallerThread(&task_runner_list_list[i], 0, 0);
  }
  for (size_t i = 0; i < kNumThreads; ++i) {
    finish_caller_thread[i]->Start();
  }
  for (size_t i = 0; i < kNumThreads; ++i) {
    finish_caller_thread[i]->Join();
  }
  EXPECT_EQ(kNumThreads * kNumTasksPerThread, callback.token_vector().size());
  EXPECT_EQ(kNumThreads * kNumTasksPerThread,
            callback.response_vector().size());
  STLDeleteElements(&finish_caller_thread);
}

}  // namespace mozc

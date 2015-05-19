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

#include "base/task_manager.h"

#include <map>
#include <vector>

#include "base/logging.h"
#include "base/mutex.h"
#include "base/port.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "base/task_token.h"
#include "base/thread.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {
class TestRequest : public TaskRequestInterface {
 public:
  TestRequest() : initial_sleep_msec_(0) {}
  explicit TestRequest(const string &data)
      : initial_sleep_msec_(0), data_(data) {
  }

  const string &data() const { return data_; }
  void set_data(const string &new_data) { data_ = new_data; }

  int initial_sleep_msec() const { return initial_sleep_msec_; }
  void set_initial_sleep_msec(int msec) { initial_sleep_msec_ = msec; }

 private:
  int initial_sleep_msec_;
  string data_;

  DISALLOW_COPY_AND_ASSIGN(TestRequest);
};

class TestResponse : public TaskResponseInterface {
 public:
  TestResponse() {}
  explicit TestResponse(const string &data) : data_(data) {}

  const string &data() const { return data_; }
  void set_data(const string &new_data) { data_ = new_data; }

 private:
  string data_;
  DISALLOW_COPY_AND_ASSIGN(TestResponse);
};

class TestRunner : public TaskRunner {
 public:
  static TestRunner* Create(
      const TaskToken &token, const TestRequest *request,
      TaskManager *task_manager) {
    return new TestRunner(token, request, task_manager);
  }

  virtual const TestRequest *request() const {
    return static_cast<const TestRequest*>(TaskRunner::request());
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
  // In order to prevent auto variable of TaskRunner being declared,
  // the constructor and the destructor of TaskRunner must be in private.
  TestRunner(const TaskToken &token, const TestRequest *request,
             TaskManager *task_manager)
      : TaskRunner(token, request, task_manager) {
  }
  virtual ~TestRunner() {}
  DISALLOW_COPY_AND_ASSIGN(TestRunner);
};

const int kNumThreads = 5;
const int kNumTasksPerThread = 10000;

class FinishTaskCallerThread : public mozc::Thread {
 public:
  FinishTaskCallerThread(vector<TestRunner*> *task_runner_list,
                         int initial_sleep_msec,
                         int interval_sleep_msec)
      : task_runner_list_(task_runner_list),
        initial_sleep_msec_(initial_sleep_msec),
        interval_sleep_msec_(interval_sleep_msec) {
  }

  virtual ~FinishTaskCallerThread() {
  }

  virtual void Run() {
    if (initial_sleep_msec_ != 0) {
      mozc::Util::Sleep(initial_sleep_msec_);
    }
    for (int i = 0; i < task_runner_list_->size(); ++i) {
      if (interval_sleep_msec_ != 0) {
        mozc::Util::Sleep(interval_sleep_msec_);
      }
      (*task_runner_list_)[i]->FinishTask("ok");
    }
  }

 private:
  vector<TestRunner*>* const task_runner_list_;  // Does not take the ownership.
  int initial_sleep_msec_;
  int interval_sleep_msec_;
  DISALLOW_COPY_AND_ASSIGN(FinishTaskCallerThread);
};

class TestRunnerFactory : public TaskRunnerFactoryInterface {
 public:
  TestRunnerFactory(): mutex_(new mozc::Mutex()) {}
  virtual ~TestRunnerFactory() {}
  virtual TestRunner *NewRunner(
      const TaskToken &token, const TaskRequestInterface *request,
      TaskManager *task_manager) {
    mozc::scoped_lock l(mutex_.get());
    TestRunner *task_runner =
        TestRunner::Create(token, static_cast<const TestRequest*>(request),
                           task_manager);
    task_runner_map_.insert(make_pair(token, task_runner));
    return task_runner;
  }

  TestRunner *GetTaskRunner(const TaskToken &token) {
    mozc::scoped_lock l(mutex_.get());
    map<TaskToken, TestRunner*>::iterator it = task_runner_map_.find(token);
    if (it == task_runner_map_.end()) {
      return NULL;
    }
    return it->second;
  }

 private:
  scoped_ptr<mozc::Mutex> mutex_;
  map<TaskToken, TestRunner*> task_runner_map_;
  DISALLOW_COPY_AND_ASSIGN(TestRunnerFactory);
};

void CheckTaskManagerStatus(TaskManager *task_manager,
                               int expected_total,
                               int expected_not_started,
                               int expected_running,
                               int expected_completed,
                               int expected_canceled) {
  int total, not_started, running, completed, canceled;
  task_manager->GetTaskStatusInfo(
      &total, &not_started, &running, &completed, &canceled);
  EXPECT_EQ(expected_total, total);
  EXPECT_EQ(expected_not_started, not_started);
  EXPECT_EQ(expected_running, running);
  EXPECT_EQ(expected_completed, completed);
  EXPECT_EQ(expected_canceled, canceled);
}

class TaskManager_Test : public testing::Test {
 protected:
  void SetUp() {
    runner_factory_ = new TestRunnerFactory;
    task_manager_.reset(new TaskManager(runner_factory_));
  }
  TestRunnerFactory *runner_factory_;
  scoped_ptr<TaskManager> task_manager_;
};

TEST_F(TaskManager_Test, NonStartTest) {
  TestRequest *request = new TestRequest("test");
  EXPECT_TRUE(request != NULL);
  TaskToken token1 = task_manager_->AddTask(request, NULL, NULL);
  EXPECT_TRUE(token1.isValid());
  EXPECT_TRUE(runner_factory_->GetTaskRunner(token1) == NULL);

  {
    SCOPED_TRACE("task not started");
    CheckTaskManagerStatus(task_manager_.get(), 1, 1, 0, 0, 0);
  }
}

TEST_F(TaskManager_Test, RemainingResponseTest) {
  TestRequest *request = new TestRequest("test");
  EXPECT_TRUE(request != NULL);
  TaskToken token1 = task_manager_->AddTask(request, NULL, NULL);
  EXPECT_TRUE(token1.isValid());
  EXPECT_TRUE(runner_factory_->GetTaskRunner(token1) == NULL);

  {
    SCOPED_TRACE("task not started");
    CheckTaskManagerStatus(task_manager_.get(), 1, 1, 0, 0, 0);
  }

  EXPECT_TRUE(task_manager_->StartTask(token1));
  TestRunner *task_runner = runner_factory_->GetTaskRunner(token1);
  ASSERT_TRUE(task_runner != NULL);

  {
    SCOPED_TRACE("task running");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 1, 0, 0);
  }

  task_runner->FinishTask("ok");

  {
    SCOPED_TRACE("task finished");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 0, 1, 0);
  }
}

TEST_F(TaskManager_Test, SimpleTest) {
  TestRequest *request1 = new TestRequest("test");
  EXPECT_TRUE(request1 != NULL);
  TaskToken token1 = task_manager_->AddTask(request1, NULL, NULL);
  EXPECT_TRUE(token1.isValid());
  EXPECT_TRUE(runner_factory_->GetTaskRunner(token1) == NULL);

  {
    SCOPED_TRACE("task not started");
    CheckTaskManagerStatus(task_manager_.get(), 1, 1, 0, 0, 0);
  }

  EXPECT_TRUE(task_manager_->StartTask(token1));
  TestRunner *task_runner = runner_factory_->GetTaskRunner(token1);
  ASSERT_TRUE(task_runner != NULL);

  {
    SCOPED_TRACE("task running");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 1, 0, 0);
  }

  task_runner->FinishTask("ok");

  {
    SCOPED_TRACE("task finished");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 0, 1, 0);
  }

  scoped_ptr<const TaskRequestInterface> request;
  scoped_ptr<const TaskResponseInterface> response;
  EXPECT_TRUE(task_manager_->TakeCompletedTask(token1, &request, &response));
  ASSERT_TRUE(response.get() != NULL);
  EXPECT_EQ("test:ok",
            static_cast<const TestResponse*>(response.get())->data());

  {
    SCOPED_TRACE("no task");
    CheckTaskManagerStatus(task_manager_.get(), 0, 0, 0, 0, 0);
  }
}

TEST_F(TaskManager_Test, StartOldestTaskTest) {
  TestRequest *request = new TestRequest("test");
  EXPECT_TRUE(request != NULL);
  TaskToken token1 = task_manager_->AddTask(request, NULL, NULL);
  EXPECT_TRUE(token1.isValid());
  EXPECT_TRUE(runner_factory_->GetTaskRunner(token1) == NULL);

  {
    SCOPED_TRACE("task not started");
    CheckTaskManagerStatus(task_manager_.get(), 1, 1, 0, 0, 0);
  }

  TaskToken token2, token3;
  EXPECT_TRUE(task_manager_->StartOldestTask(&token2));
  EXPECT_FALSE(task_manager_->StartOldestTask(&token3));
  EXPECT_TRUE(token2.isValid());
  TestRunner *task_runner = runner_factory_->GetTaskRunner(token2);
  ASSERT_TRUE(task_runner != NULL);

  {
    SCOPED_TRACE("task running");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 1, 0, 0);
  }

  task_runner->FinishTask("ok");

  {
    SCOPED_TRACE("task finished");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 0, 1, 0);
  }
}

TEST_F(TaskManager_Test, AnotherThreadFinishTest) {
  TestRequest *request = new TestRequest("test");
  EXPECT_TRUE(request != NULL);
  TaskToken token1 = task_manager_->AddTask(request, NULL, NULL);
  EXPECT_TRUE(token1.isValid());
  EXPECT_TRUE(runner_factory_->GetTaskRunner(token1) == NULL);

  {
    SCOPED_TRACE("task not started");
    CheckTaskManagerStatus(task_manager_.get(), 1, 1, 0, 0, 0);
  }

  EXPECT_TRUE(task_manager_->StartTask(token1));
  TestRunner *task_runner = runner_factory_->GetTaskRunner(token1);
  ASSERT_TRUE(task_runner != NULL);

  {
    SCOPED_TRACE("task running");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 1, 0, 0);
  }

  vector<TestRunner*> task_runner_list;
  task_runner_list.push_back(task_runner);
  FinishTaskCallerThread finish_caller_thread(&task_runner_list, 100, 0);
  finish_caller_thread.Start();

  {
    SCOPED_TRACE("task running");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 1, 0, 0);
  }

  finish_caller_thread.Join();

  {
    SCOPED_TRACE("task finished");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 0, 1, 0);
  }
}

TEST_F(TaskManager_Test, AnotherThreadFinishWaitTest) {
  TestRequest *request1 = new TestRequest("test1");
  TestRequest *request2 = new TestRequest("test2");
  EXPECT_TRUE(request1 != NULL);
  EXPECT_TRUE(request2 != NULL);
  TaskToken token1 = task_manager_->AddTask(request1, NULL, NULL);
  TaskToken token2 = task_manager_->AddTask(request2, NULL, NULL);
  EXPECT_TRUE(token1.isValid());
  EXPECT_TRUE(runner_factory_->GetTaskRunner(token1) == NULL);
  EXPECT_TRUE(token2.isValid());
  EXPECT_TRUE(runner_factory_->GetTaskRunner(token2) == NULL);

  {
    SCOPED_TRACE("tasks not started");
    CheckTaskManagerStatus(task_manager_.get(), 2, 2, 0, 0, 0);
  }

  EXPECT_TRUE(task_manager_->StartTask(token1));
  {
    SCOPED_TRACE("one task not started, one task running");
    CheckTaskManagerStatus(task_manager_.get(), 2, 1, 1, 0, 0);
  }
  EXPECT_TRUE(task_manager_->StartTask(token2));
  {
    SCOPED_TRACE("two tasks running");
    CheckTaskManagerStatus(task_manager_.get(), 2, 0, 2, 0, 0);
  }

  TestRunner *task_runner1 = runner_factory_->GetTaskRunner(token1);
  ASSERT_TRUE(task_runner1 != NULL);
  TestRunner *task_runner2 = runner_factory_->GetTaskRunner(token2);
  ASSERT_TRUE(task_runner2 != NULL);

  vector<TestRunner*> task_runner_list;
  task_runner_list.push_back(task_runner1);
  task_runner_list.push_back(task_runner2);
  FinishTaskCallerThread finish_caller_thread(&task_runner_list, 100, 200);
  finish_caller_thread.Start();
  {
    SCOPED_TRACE("two tasks running");
    CheckTaskManagerStatus(task_manager_.get(), 2, 0, 2, 0, 0);
  }
  task_manager_.reset(NULL);

  finish_caller_thread.Join();
}

class TestTaskManagerCallback {
 public:
  TestTaskManagerCallback() : delete_task_flag_(false) {}
  virtual ~TestTaskManagerCallback() {
  }
  static bool OnTaskDone(
      const TaskManager *task_manager,
      const TaskToken &token, const TaskRequestInterface *request,
      const TaskResponseInterface *response,
      void *callback_data) {
    TestTaskManagerCallback *self =
        static_cast<TestTaskManagerCallback*>(callback_data);
    mozc::scoped_lock l(&self->mutex_);
    self->token_vector_.push_back(token);
    self->response_data_vector_.push_back(
        static_cast<const TestResponse*>(response)->data());
    return self->delete_task_flag_;
  }
  void set_delete_task_flag(bool delete_task_flag) {
    delete_task_flag_ = delete_task_flag;
  }
  const vector<TaskToken> &token_vector() {
    return token_vector_;
  }
  const vector<string> &response_data_vector() {
    return response_data_vector_;
  }

 private:
  bool delete_task_flag_;
  mozc::Mutex mutex_;
  vector<TaskToken> token_vector_;
  vector<string> response_data_vector_;
  DISALLOW_COPY_AND_ASSIGN(TestTaskManagerCallback);
};

TEST_F(TaskManager_Test, CallbackTest) {
  TestRequest *request1 = new TestRequest("test");
  EXPECT_TRUE(request1 != NULL);

  TestTaskManagerCallback callback;
  callback.set_delete_task_flag(false);
  TaskToken token1 =
      task_manager_->AddTask(request1, &TestTaskManagerCallback::OnTaskDone,
                             &callback);
  EXPECT_TRUE(token1.isValid());
  EXPECT_TRUE(runner_factory_->GetTaskRunner(token1) == NULL);

  {
    SCOPED_TRACE("task not started");
    CheckTaskManagerStatus(task_manager_.get(), 1, 1, 0, 0, 0);
  }

  EXPECT_TRUE(task_manager_->StartTask(token1));
  TestRunner *task_runner = runner_factory_->GetTaskRunner(token1);
  ASSERT_TRUE(task_runner != NULL);

  {
    SCOPED_TRACE("task running");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 1, 0, 0);
  }

  task_runner->FinishTask("ok");

  {
    SCOPED_TRACE("task finished");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 0, 1, 0);
  }

  scoped_ptr<const TaskRequestInterface> request;
  scoped_ptr<const TaskResponseInterface> response;
  EXPECT_TRUE(task_manager_->TakeCompletedTask(token1, &request, &response));
  ASSERT_TRUE(response.get() != NULL);
  EXPECT_EQ("test:ok",
            static_cast<const TestResponse*>(response.get())->data());

  {
    SCOPED_TRACE("no task");
    CheckTaskManagerStatus(task_manager_.get(), 0, 0, 0, 0, 0);
  }

  ASSERT_EQ(1, callback.token_vector().size());
  ASSERT_EQ(1, callback.response_data_vector().size());

  EXPECT_TRUE(token1 == callback.token_vector()[0]);
  EXPECT_EQ("test:ok", callback.response_data_vector()[0]);
}

TEST_F(TaskManager_Test, UnregisterTaskCallbackTest) {
  TestRequest *request1 = new TestRequest("test");
  EXPECT_TRUE(request1 != NULL);

  TestTaskManagerCallback callback;
  callback.set_delete_task_flag(false);
  TaskToken token1 =
      task_manager_->AddTask(request1, &TestTaskManagerCallback::OnTaskDone,
                             &callback);
  EXPECT_TRUE(token1.isValid());
  EXPECT_TRUE(runner_factory_->GetTaskRunner(token1) == NULL);

  {
    SCOPED_TRACE("task not started");
    CheckTaskManagerStatus(task_manager_.get(), 1, 1, 0, 0, 0);
  }

  EXPECT_TRUE(task_manager_->UnregisterTaskCallback(token1));

  EXPECT_TRUE(task_manager_->StartTask(token1));
  TestRunner *task_runner = runner_factory_->GetTaskRunner(token1);
  ASSERT_TRUE(task_runner != NULL);

  {
    SCOPED_TRACE("task running");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 1, 0, 0);
  }

  task_runner->FinishTask("ok");

  {
    SCOPED_TRACE("task finished");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 0, 1, 0);
  }

  scoped_ptr<const TaskRequestInterface> request;
  scoped_ptr<const TaskResponseInterface> response;
  EXPECT_TRUE(task_manager_->TakeCompletedTask(token1, &request, &response));
  ASSERT_TRUE(response.get() != NULL);
  EXPECT_EQ("test:ok",
            static_cast<const TestResponse*>(response.get())->data());

  {
    SCOPED_TRACE("task manager check point");
    CheckTaskManagerStatus(task_manager_.get(), 0, 0, 0, 0, 0);
  }

  EXPECT_EQ(0, callback.token_vector().size());
  EXPECT_EQ(0, callback.response_data_vector().size());
}

TEST_F(TaskManager_Test, CallbackDeleteTaskTest) {
  TestRequest *request1 = new TestRequest("test");
  EXPECT_TRUE(request1 != NULL);
  TestTaskManagerCallback callback;
  callback.set_delete_task_flag(true);
  TaskToken token1 =
      task_manager_->AddTask(request1, &TestTaskManagerCallback::OnTaskDone,
                             &callback);
  EXPECT_TRUE(token1.isValid());
  EXPECT_TRUE(runner_factory_->GetTaskRunner(token1) == NULL);

  {
    SCOPED_TRACE("task not started");
    CheckTaskManagerStatus(task_manager_.get(), 1, 1, 0, 0, 0);
  }

  EXPECT_TRUE(task_manager_->StartTask(token1));
  TestRunner *task_runner = runner_factory_->GetTaskRunner(token1);
  ASSERT_TRUE(task_runner != NULL);

  {
    SCOPED_TRACE("task running");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 1, 0, 0);
  }

  task_runner->FinishTask("ok");

  {
    SCOPED_TRACE("no task");
    CheckTaskManagerStatus(task_manager_.get(), 0, 0, 0, 0, 0);
  }

  scoped_ptr<const TaskRequestInterface> request;
  scoped_ptr<const TaskResponseInterface> response;
  EXPECT_FALSE(task_manager_->TakeCompletedTask(token1, &request, &response));

  ASSERT_EQ(1, callback.token_vector().size());
  ASSERT_EQ(1, callback.response_data_vector().size());

  EXPECT_TRUE(token1 == callback.token_vector()[0]);
  EXPECT_EQ("test:ok", callback.response_data_vector()[0]);
}

TEST_F(TaskManager_Test, CancelTaskTest) {
  TestRequest *request1 = new TestRequest("test");
  EXPECT_TRUE(request1 != NULL);
  TaskToken token1 = task_manager_->AddTask(request1, NULL, NULL);
  EXPECT_TRUE(token1.isValid());
  EXPECT_TRUE(runner_factory_->GetTaskRunner(token1) == NULL);

  {
    SCOPED_TRACE("task not started");
    CheckTaskManagerStatus(task_manager_.get(), 1, 1, 0, 0, 0);
  }

  EXPECT_TRUE(task_manager_->StartTask(token1));
  TestRunner *task_runner = runner_factory_->GetTaskRunner(token1);
  ASSERT_TRUE(task_runner != NULL);

  {
    SCOPED_TRACE("task running");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 1, 0, 0);
  }

  task_manager_->CancelTask(token1);

  {
    SCOPED_TRACE("task canceled");
    CheckTaskManagerStatus(task_manager_.get(), 1, 0, 0, 0, 1);
  }

  task_runner->FinishTask("ok");

  {
    SCOPED_TRACE("no task");
    CheckTaskManagerStatus(task_manager_.get(), 0, 0, 0, 0, 0);
  }
}

class TaskExecutorThread : public mozc::Thread {
 public:
  explicit TaskExecutorThread(TaskManager *task_manager)
      : task_manager_(task_manager) {
  }
  virtual ~TaskExecutorThread() {
    Join();
  }
  virtual void Run() {
    while (true) {
      TaskToken token;
      if (!task_manager_->StartOldestTask(&token)) {
        if (task_manager_->IsShuttingDown()) {
          VLOG(1) << "shutting down";
          return;
        }
        task_manager_->WaitForNewTaskEvent(-1);
      }
    }
  }

 private:
  TaskManager *task_manager_;  // Does not take the ownership.
};

TEST_F(TaskManager_Test, SingleThreadTest) {
  for (int i = 0; i < 10; ++i) {
    TestRequest *request = new TestRequest("CompleteInStartTask");
    request->set_initial_sleep_msec(1000);
    task_manager_->AddTask(request, NULL, NULL);
  }
  {
    SCOPED_TRACE("tasks not started");
    CheckTaskManagerStatus(task_manager_.get(), 10, 10, 0, 0, 0);
  }

  TaskExecutorThread executor_thread(task_manager_.get());
  executor_thread.Start();

  Util::Sleep(2500);

  task_manager_->ShutDown();
  executor_thread.Join();

  {
    SCOPED_TRACE("3 tasks finished");
    CheckTaskManagerStatus(task_manager_.get(), 10, 7, 0, 3, 0);
  }
}

TEST_F(TaskManager_Test, MultiThreadNoTaskTest) {
  vector<TaskExecutorThread*>
      executor_thread_list(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i) {
    executor_thread_list[i] =
        new TaskExecutorThread(task_manager_.get());
  }
  for (int i = 0; i < kNumThreads; ++i) {
    executor_thread_list[i]->Start();
  }

  task_manager_->ShutDown();

  for (int i = 0; i < kNumThreads; ++i) {
    executor_thread_list[i]->Join();
  }

  mozc::STLDeleteElements(&executor_thread_list);
}

TEST_F(TaskManager_Test, MultiThreadTest) {
  vector<TaskExecutorThread*>
      executor_thread_list(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i) {
    executor_thread_list[i] =
        new TaskExecutorThread(task_manager_.get());
  }
  for (int i = 0; i < kNumThreads; ++i) {
    executor_thread_list[i]->Start();
  }
  for (int i = 0; i < 1000; ++i) {
    TestRequest *request = new TestRequest("CompleteInStartTask");
    request->set_initial_sleep_msec(10);
    task_manager_->AddTask(request, NULL, NULL);
  }

  Util::Sleep(250);
  task_manager_->ShutDown();

  for (int i = 0; i < kNumThreads; ++i) {
    executor_thread_list[i]->Join();
  }

  int total, not_started, running, completed, canceled;
  task_manager_->GetTaskStatusInfo(
      &total, &not_started, &running, &completed, &canceled);

  EXPECT_EQ(0, running);
  EXPECT_EQ(1000, not_started + completed);
  DLOG(INFO) << "not_started: " << not_started;
  DLOG(INFO) << "completed: " << completed;

  mozc::STLDeleteElements(&executor_thread_list);
}

}  // namespace
}  // namespace mozc

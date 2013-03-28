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

// A TaskRunner object runs tasks which take one request object as input and
// outputs one response object. TaskManager provides a way to decouple the task
// posting logic and the task processing logic. TaskManager is thread-safe, so
// we can post tasks and start tasks and get the results in different threads.
//
// A TaskManager creates a TaskRunner object to start a task. After the task is
// started, the TaskRunner must call CompleteTask method to return the result to
// the TaskManager and delete itself.
//
// To recieve the task completion callback, we can register one callback
// function per one task. The callback function is called in the thread in which
// CompleteTask is called in the TaskRunner.
//
// How to use TaskManager:
//
//  1) Implementation
//   To implement a new task manager, we have to implement the followings.
//    - TaskRequestInterface
//        This class represents the input data of the task.
//    - TaskResponseInterface
//        This class represents the output data of the task.
//    - TaskRunner
//        This class represents how the task is processed.
//    - TaskRunnerFactoryInterface
//        This class is used in TaskManager to create a TaskRunner.
//    - TaskManagerCallbackFunc
//        This function type is used to recieve the task completion callback.
//
//  Example:
//
//   class DownloadTaskRequest : public TaskRequestInterface {
//    public:
//     DownloadTaskRequest(const string &url) : url_(url) {}
//     const string &url() const { return url_; }
//    private:
//     const string url_;
//     DISALLOW_COPY_AND_ASSIGN(DownloadTaskRequest);
//   };
//
//   class DownloadTaskResponse : public TaskResponseInterface {
//    public:
//     DownloadTaskResponse(const string &data) : data_(data) {}
//     const string &data() const { return data_; }
//    private:
//     const string data_;
//     DISALLOW_COPY_AND_ASSIGN(DownloadTaskResponse);
//   };
//
//   class DownloadTaskRunner : public TaskRunner {
//    public:
//     static DownloadTaskRunner* Create(
//         const TaskToken &token, const DownloadTaskRequest *request,
//         TaskManager *task_manager) {
//       return new DownloadTaskRunner(token, request, task_manager);
//     }
//
//     virtual void StartTask() {
//       const string &url =
//           static_cast<const DownloadTaskRequest*>(request())->url();
//
//       const string data = ... // Dowload data from the url.
//       CompleteTask(new DownloadTaskResponse(data));
//     }
//
//    private:
//     // In order to prevent auto variable of TaskRunner being declared,
//     // the constructor and the destructor of TaskRunner must be in private.
//     DownloadTaskRunner(
//         const TaskToken &token, const DownloadTaskRequest *request,
//         TaskManager *task_manager)
//         : TaskRunner(token, request, task_manager) {
//     }
//     virtual ~DownloadTaskRunner() {}
//     DISALLOW_COPY_AND_ASSIGN(DownloadTaskRunner);
//   };
//
//   class DownloadTaskRunnerFactory {
//    public:
//     DownloadTaskRunnerFactory() {}
//     virtual ~DownloadTaskRunnerFactory() {}
//     virtual DownloadTaskRunner *NewRunner(
//         const TaskToken &token, const TaskRequestInterfaTaskManager *request,
//         TaskManager *task_manager) {
//       return DownloadTaskRunner::Create(
//           token, static_cast<const DownloadTaskRequest*>(request),
//           task_manager);
//     }
//
//    private:
//     DISALLOW_COPY_AND_ASSIGN(DownloadTaskRunnerFactory);
//   };
//
//   bool OnDownloadTaskDone(const TaskManager *task_manager,
//                           const TaskToken &token,
//                           const TaskRequestInterface *request,
//                           const TaskResponseInterface *response,
//                           void *callback_data) {
//     LOG(INFO) << "Download completed";
//     return false;  // Do not delete the task data from the TaskManager.
//   }
//
//  2) Creating TaskManager
//   To create a TaskManager, we have to set a TaskRunnerFactory object,
//   which will be used to create TaskRunner objects which process the target
//   tasks.
//
//  Example:
//
//   TaskManager task_manager(new DownloadTaskRunnerFactory);
//
//  3) Adding tasks
//   We can add a task calling TaskManager::AddTask(). When we add a task, we
//   can register a callback function which will be called at the task
//   completion. We can add multiple tasks in a TaskManager.
//
//  Example:
//
//   TaskToken token =
//       task_manager.AddTask(new DownloadTaskRequest("http://www..."),
//                            OnDownloadTaskDone, NULL);
//
//  4) Starting task
//   We can start a task calling TaskManager::StartTask(). This function creates
//   a new TaskRunner object using the TaskRunnerFactory object and calls
//   TaskRunner::StartTask() of the new TaskRunner object. We can call
//   TaskManager::StartOldestTask() to start the least recently registered
//   task.
//
//   The task is started in the thread in which TaskManager::StartTask() or
//   TaskManager::StartOldestTask() is called. TaskRunner::StartTask() blocks
//   the thread. If we want to avoid blocking the thread, we should use another
//   thread to run the time-consuming task or use asynchronous callback API.
//
//  Example:
//
//   task_manager.StartTask(token);
//
//  5) Finishing task
//   When the task has finished or has been canceled, the TaskRunner has to call
//   TaskRunner::CompleteTask(). See the codes in DownloadTaskRunner.
//       CompleteTask(new DownloadTaskResponse(data));
//
//   The thread in which TaskRunner::CompleteTask() is called can be different
//   from the thread in which TaskRunner::StartTask() is called.
//
//   When TaskRunner::CompleteTask() is called, TaskManager::OnTaskDone() will
//   be called, and the registered callback function will be called, and the
//   TaskRunner object will be deleted by itself.
//   This sequence is processed in the thread in which CompleteTask() is called.
//   So the callback function must be thread-safe if the CompleteTask() can be
//   called from several threads.
//   If the return value of the registered callback function is true, the
//   TaskManager deletes the task data in it.
//
//  6) Getting the result of the task
//   We can get the result of the completed task calling TakeCompletedTask().
//   The task data in the TaskManager will be deleted.
//
//  Example:
//
//   scoped_ptr<const TaskRequestInterface> request;
//   scoped_ptr<const TaskResponseInterface> response;
//   task_manager.TakeCompletedTask(token, &request, &response);
//
//  7) Cancelling task
//   We can cancel the task calling CancelTask().
//   If TaskManager.CancelTask() is called, the TaskManager marks the task as
//   CANCELED and calls TaskRunner.CancelTask(). When TaskRunner.CancelTask() is
//   called the TaskRunner has to stop the task and call CompleteTask() as soon
//   as possible. The task data in the TaskManager will be deleted when
//   OnTaskDone() is called via CompleteTask().
//
//  Example:
//
//   task_manager.CancelTask(token);

#ifndef MOZC_BASE_TASK_MANAGER_H_
#define MOZC_BASE_TASK_MANAGER_H_

#include <map>

#include "base/mutex.h"
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "base/task_token.h"
#include "base/task_runner.h"
#include "base/thread_annotations.h"

namespace mozc {
class Mutex;
class UnnamedEvent;
class TaskManager;

// Factory class of TaskRunner used in TaskManager to create a new TaskRunner.
// To use TaskManager, we have to implement a TaskRunnerFactory that creates
// TaskRunners which process the target task, and set a TaskRunnerFactory in the
// constructor of TaskManager.
class TaskRunnerFactoryInterface {
 public:
  virtual ~TaskRunnerFactoryInterface() {}
  virtual TaskRunner *NewRunner(const TaskToken &token,
                                const TaskRequestInterface *request,
                                TaskManager *task_manager) = 0;
};

// Manager of Tasks. See comment at top of file for a description.
class TaskManager {
 public:
  // Callback function type which is called when the task has completed.
  // callback_data is tha data which is set in AddTask().
  typedef bool (*TaskManagerCallbackFunc)(
      const TaskManager *task_manager, const TaskToken &token,
      const TaskRequestInterface *request,
      const TaskResponseInterface *response, void *callback_data);

  // TaskManager owns runner_factory.
  explicit TaskManager(TaskRunnerFactoryInterface *runner_factory);
  virtual ~TaskManager();

  // Adds a new task and returns the token of the task.
  // Takes ownership of the request object.
  // This method does not start the task.
  // If callback is not needed, set callback_func NULL.
  // callback_data is used when the callback function will be called.
  TaskToken AddTask(const TaskRequestInterface *request,
                    TaskManagerCallbackFunc callback_func,
                    void *callback_data);

  // Unregisters the callback.
  // Returns false if there is no task of the token.
  bool UnregisterTaskCallback(const TaskToken &token);

  // Starts the task of the token.
  // Returns false if there is no task of the token.
  bool StartTask(const TaskToken &token);

  // Starts the oldest not started task, and set the token and returns true.
  // If the TaskManager is shutting down, this method returns false.
  // If there is no not started task, this method returns false.
  bool StartOldestTask(TaskToken *token);

  // Returns false if there is no task of the token.
  // If the task isn't running, deletes the task data from the TaskManager and
  // returns true. If the task is running, cancels the running task and returns
  // true. The canceled task data will be deleted when the OnTaskDone is called
  // from the TaskRunner.
  bool CancelTask(const TaskToken &token);

  // Waits for the next task to be added or the TaskManager to be shut down.
  // Returns true when a new task is added or SetShuttingDown is called from
  // another thread. Returns false when a time-out ("msec"  millisecond) occurs.
  // If "msec" is a negative value, it waits forever.
  bool WaitForNewTaskEvent(int msec);

  // If the task of the token is completed, deletes the data of the task from
  // the TaskManager, and stores the request and response data into "request"
  // and "response" and returns true.
  bool TakeCompletedTask(const TaskToken &token,
                         scoped_ptr<const TaskRequestInterface> *request,
                         scoped_ptr<const TaskResponseInterface> *response);

  // Returns true if the TaskManager is shutting down.
  bool IsShuttingDown();

  // Shuts down the TaskManager.
  void ShutDown();

  // Gets the status of the tasks which are owned by the TaskManager.
  void GetTaskStatusInfo(int *total_task_count,
                         int *not_started_task_count,
                         int *running_task_count,
                         int *completed_task_count,
                         int *canceled_task_count);

 private:
  // Container of the task information such as the request and the response.
  struct TaskInfo;

  // Gets the task information which is stored in the TaskManager.
  TaskInfo* GetTaskInfo(const TaskToken &token)
      SHARED_LOCKS_REQUIRED(mutex_);

  // Called from TaskRunner when the task has finished.
  void OnTaskDone(const TaskRunner *task_runner,
                  const TaskToken &token,
                  const TaskRequestInterface *request,
                  const TaskResponseInterface *response);

  Mutex mutex_;
  // TaskRunnerFactory is set in the constructor.
  scoped_ptr<TaskRunnerFactoryInterface> runner_factory_;
  bool is_shutting_down_ GUARDED_BY(mutex_);
  // Raised when a new task is added.
  scoped_ptr<UnnamedEvent> new_task_event_;
  // Raised when a task is completed.
  scoped_ptr<UnnamedEvent> task_done_event_;
  // TaskTokenManager genetrates the task tokens for each posted tasks.
  ThreadSafeTaskTokenManager token_manager_;
  // Container of the task information.
  map<TaskToken, TaskInfo*> task_info_map_ GUARDED_BY(mutex_);

  // TaskRunner need to call OnTaskDone().
  friend class TaskRunner;

  DISALLOW_COPY_AND_ASSIGN(TaskManager);
};

}  // namespace mozc

#endif  // MOZC_BASE_TASK_MANAGER_H_

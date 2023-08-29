// Copyright 2010-2021, Google Inc.
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

#ifndef MOZC_IPC_PROCESS_WATCH_DOG_H_
#define MOZC_IPC_PROCESS_WATCH_DOG_H_

#include <cstdint>

#include "base/thread2.h"
#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"

#ifdef _WIN32
#include <wil/resource.h>
#else  // _WIN32
#include <sys/types.h>
#endif  // _WIN32

// Usage:
//
// ProcessWatchDog dog([](SignalType signal_type) {
//    std::cout << "signaled!" << std::endl;
// });
// dog.SetId(pid, tid);

namespace mozc {

class ProcessWatchDog {
 public:
  enum class SignalType {
    UNKNOWN = 0,                // default value. nerver be signaled.
    PROCESS_SIGNALED = 1,       // process is signaled,
    PROCESS_NOT_FOUND = 3,      // process id was not found
    PROCESS_ACCESS_DENIED = 4,  // operation was not allowed
    PROCESS_ERROR = 5,          // unknown error in getting process info
    THREAD_SIGNALED = 6,        // thread is signaled
    THREAD_NOT_FOUND = 7,       // thread id was not found
    THREAD_ACCESS_DENIED = 8,   // operation was not allowed
    THREAD_ERROR = 9,           // unknown error in getting thread info
    TIMEOUT = 10,               // timeout is signaled
  };

  using Handler = absl::AnyInvocable<void(SignalType)>;

#ifdef _WIN32
  using ProcessId = uint32_t;
  using ThreadId = uint32_t;
#else   // _WIN32
  using ProcessId = pid_t;
  // Linux/Mac has no way to export ThreadID to other process.
  // For instance, Mac's thread id is just a pointer to the some
  // internal data structure (_opaque_pthread_t*).
  using ThreadId = uint32_t;
#endif  // !_WIN32

  static constexpr ProcessId kUnknownProcessId = ~ProcessId{0};
  static constexpr ThreadId kUnknownThreadId = ~ProcessId{0};

  // Creates and starts the watchdog with `handler`. If the given process or
  // thread is terminated, `handler` is invoked with `SignalType` defined above.
  // Please note that `handler` is invoked from different thread.
  explicit ProcessWatchDog(Handler handler);

  // Stops the watchdog and joins the background thread.
  ~ProcessWatchDog();

  // Resets pid and tid to watch. You can pass
  // kUnknownProcessId/kUnknownProcessId if they are unknown or not needed to be
  // checked.
  bool SetId(ProcessId process_id, ThreadId thread_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // Notifies the background of control operation i.e. pid_, tid_ or stop_ has
  // been changed.
  bool SignalControlOperation() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void ThreadMain();

  absl::Mutex mutex_;
  ProcessId pid_ ABSL_GUARDED_BY(mutex_) = kUnknownProcessId;
  ThreadId tid_ ABSL_GUARDED_BY(mutex_) = kUnknownThreadId;
  // Set to true to notify the background thread of termination.
  // Use mutex-guarded boolean instead of `absl::Notification` so that we can
  // `Await()` for pid/tid change and termination signal all at once.
  bool terminating_ ABSL_GUARDED_BY(mutex_) = false;
#ifdef _WIN32
  // Used to interrupt `WaitForMultipleObjects` to perform control operations.
  wil::unique_event_nothrow event_;
#else   // _WIN32
  // Set to true when any of pid_, tid_ or terminating_ is updated.
  // absl::Condition on this is used to interrupt the sleep between polls to
  // perform control operations.
  bool dirty_ ABSL_GUARDED_BY(mutex_) = false;
#endif  // !_WIN32
  Handler handler_;
  Thread2 thread_;
};

}  // namespace mozc

#endif  // MOZC_IPC_PROCESS_WATCH_DOG_H_

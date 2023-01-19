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

#ifndef OS_WIN
#include <sys/types.h>
#endif  // !OS_WIN

#include <cstdint>
#include <memory>

#include "base/port.h"
#include "base/scoped_handle.h"
#include "base/thread.h"
#include "absl/synchronization/mutex.h"

// Usage:
//
// class MyProcessWatchDog {
//   void Signaled(SignalType sginal_type) {
//      std::cout << "signaled! " << std::endl;
//   }
// }
//
// MyProcessWatchDog dog;
// dog.SetID(pid, tid, -1);

namespace mozc {

class ProcessWatchDog : public Thread {
 public:
  enum SignalType {
    UNKNOWN_SIGNALED = 0,                // default value. nerver be signaled.
    PROCESS_SIGNALED = 1,                // process is signaled,
    PROCESS_NOT_FOUND_SIGNALED = 3,      // process id was not found
    PROCESS_ACCESS_DENIED_SIGNALED = 4,  // operation was not allowed
    PROCESS_ERROR_SIGNALED = 5,         // unknown error in getting process info
    THREAD_SIGNALED = 6,                // thread is signaled
    THREAD_NOT_FOUND_SIGNALED = 7,      // thread id was not found
    THREAD_ACCESS_DENIED_SIGNALED = 8,  // operation was not allowed
    THREAD_ERROR_SIGNALED = 9,          // unknown error in getting thread info
    TIMEOUT_SIGNALED = 10,              // timeout is signaled
  };

#ifdef OS_WIN
  typedef uint32_t ProcessID;
  typedef uint32_t ThreadID;
#else   // OS_WIN
  typedef pid_t ProcessID;
  // Linux/Mac has no way to export ThreadID to other process.
  // For instance, Mac's thread id is just a pointer to the some
  // internal data structure (_opaque_pthread_t*).
  typedef uint32_t ThreadID;
#endif  // OS_WIN

  static constexpr ProcessID UnknownProcessID = static_cast<ProcessID>(-1);
  static constexpr ThreadID UnknownThreadID = static_cast<ThreadID>(-1);

  // Define a signal handler.
  // if the given process or thread is terminated, Signaled() is called
  // with SignalType defined above.
  // Please note that Signaled() is called from different thread.
  virtual void Signaled(ProcessWatchDog::SignalType type) {}

  // Reset process id and thread id.
  // You can set UnknownProcessID/UnknownProcessID if
  // they are unknown or not needed to be checked.
  // This function returns immediately.
  // You can set the timout for the signal.
  // If timeout is negative, it waits forever.
  bool SetID(ProcessID process_id, ThreadID thread_id, int timeout);

  // internally used by thread
  void Run() override;

  ProcessWatchDog();
  ~ProcessWatchDog() override;
  void StartWatchDog();
  void StopWatchDog();

 private:
#ifdef OS_WIN
  ScopedHandle event_;
#endif  // OS_WIN
  ProcessID process_id_;
  ThreadID thread_id_;
  int timeout_;
  volatile bool is_finished_;
  absl::Mutex mutex_;
};

}  // namespace mozc

#endif  // MOZC_IPC_PROCESS_WATCH_DOG_H_

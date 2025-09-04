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

#include "ipc/process_watch_dog.h"

#include <utility>

#include "absl/log/log.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "base/port.h"
#include "base/thread.h"

#ifdef _WIN32
// clang-format off
#include <windows.h>  // wil/resource.h requires windows.h for events
// clang-format on
#include <wil/resource.h>

#include "base/vlog.h"
#include "base/win32/hresult.h"
#else  // _WIN32
#include <errno.h>
#include <signal.h>
#endif  // _WIN32

namespace mozc {

ProcessWatchDog::ProcessWatchDog(Handler handler)
    : handler_(std::move(handler)) {
#ifdef _WIN32
  if (win32::HResult hr(event_.create(wil::EventOptions::ManualReset));
      hr.Failed()) {
    LOG(ERROR) << "::CreateEvent() failed." << hr;
    // Spawn a no-op thread on failure for implementation simplicity.
    thread_ = Thread([] {});
    return;
  }
#endif  // _WIN32
  thread_ = Thread([this] { ThreadMain(); });
}

ProcessWatchDog::~ProcessWatchDog() {
  {
    absl::MutexLock l(mutex_);
    terminating_ = true;
    SignalControlOperation();
  }

  thread_.Join();
}

bool ProcessWatchDog::SetId(ProcessId process_id, ThreadId thread_id) {
  if constexpr (!TargetIsWindows()) {
    LOG_IF(ERROR, thread_id != kUnknownThreadId)
        << "Linux/Mac don't allow to capture ThreadID";
  }

  absl::MutexLock l(mutex_);
  pid_ = process_id;
  tid_ = thread_id;

  return SignalControlOperation();
}

bool ProcessWatchDog::SignalControlOperation() {
#ifdef _WIN32
  if (!event_) {
    return false;
  }
  event_.SetEvent();
#else   // _WIN32
  dirty_ = true;
#endif  // !_WIN32

  return true;
}

#ifdef _WIN32

void ProcessWatchDog::ThreadMain() {
  while (true) {
    {
      absl::MutexLock l(&mutex_);
      if (terminating_) {
        break;
      }
    }

    wil::unique_process_handle process_handle, thread_handle;

    // read the current ids
    {
      absl::MutexLock l(&mutex_);

      if (pid_ != kUnknownProcessId) {
        process_handle.reset(::OpenProcess(SYNCHRONIZE, FALSE, pid_));
        const DWORD error = ::GetLastError();
        if (!process_handle) {
          LOG(ERROR) << "OpenProcess failed: " << pid_ << " " << error;
          switch (error) {
            case ERROR_ACCESS_DENIED:
              handler_(SignalType::PROCESS_ACCESS_DENIED);
              break;
            case ERROR_INVALID_PARAMETER:
              handler_(SignalType::PROCESS_NOT_FOUND);
              break;
            default:
              handler_(SignalType::PROCESS_ERROR);
              break;
          }
        }
      }

      if (tid_ != kUnknownThreadId) {
        thread_handle.reset(::OpenThread(SYNCHRONIZE, FALSE, tid_));
        const DWORD error = ::GetLastError();
        if (!thread_handle) {
          LOG(ERROR) << "OpenThread failed: " << tid_ << " " << error;
          switch (error) {
            case ERROR_ACCESS_DENIED:
              handler_(SignalType::THREAD_ACCESS_DENIED);
              break;
            case ERROR_INVALID_PARAMETER:
              handler_(SignalType::THREAD_NOT_FOUND);
              break;
            default:
              handler_(SignalType::THREAD_ERROR);
              break;
          }
        }
      }

      pid_ = kUnknownProcessId;
      tid_ = kUnknownThreadId;
    }

    SignalType types[3];
    HANDLE handles[3] = {};

    // set event
    handles[0] = event_.get();

    // set handles
    DWORD size = 1;
    if (process_handle.get() != nullptr) {
      MOZC_VLOG(2) << "Inserting process handle";
      handles[size] = process_handle.get();
      types[size] = SignalType::PROCESS_SIGNALED;
      ++size;
    }

    if (thread_handle.get() != nullptr) {
      MOZC_VLOG(2) << "Inserting thread handle";
      handles[size] = thread_handle.get();
      types[size] = SignalType::THREAD_SIGNALED;
      ++size;
    }

    const DWORD result =
        ::WaitForMultipleObjects(size, handles, FALSE, INFINITE);
    SignalType result_type = SignalType::UNKNOWN;
    switch (result) {
      case WAIT_OBJECT_0:
      case WAIT_ABANDONED_0:
        MOZC_VLOG(2) << "event is signaled";
        event_.ResetEvent();  // reset event to wait for the new request
        break;
      case WAIT_OBJECT_0 + 1:
      case WAIT_ABANDONED_0 + 1:
        MOZC_VLOG(2) << "handle 1 is signaled";
        result_type = types[1];
        break;
      case WAIT_OBJECT_0 + 2:
      case WAIT_ABANDONED_0 + 2:
        MOZC_VLOG(2) << "handle 2 is signaled";
        result_type = types[2];
        break;
      case WAIT_TIMEOUT:
        MOZC_VLOG(2) << "timeout is signaled";
        result_type = SignalType::TIMEOUT;
        break;
      default:
        LOG(ERROR) << "WaitForMultipleObjects() failed: " << GetLastError();
        break;
    }

    if (result_type != SignalType::UNKNOWN) {
      MOZC_VLOG(1) << "Sending signal: " << static_cast<int>(result_type);
      handler_(result_type);  // call signal handler
    }
  }
}

#else  // _WIN32

void ProcessWatchDog::ThreadMain() {
  // Polling-based watch-dog.
  // Unlike WaitForMultipleObjects on Windows, no event-driven API seems to be
  // available on Linux.
  // NOTE In theory, there may possibility that some other process
  // reuse same process id in 250ms or write to terminating_ stays
  // forever in another CPU's local cache.
  // TODO(team): use kqueue with EVFILT_PROC/NOTE_EXIT for Mac.
  while (true) {
    absl::MutexLock l(mutex_);
    // Read this as: sleep for 250ms, or return early if signaled.
    mutex_.AwaitWithTimeout(absl::Condition(&dirty_), absl::Milliseconds(250));
    dirty_ = false;

    if (terminating_) {
      return;
    }

    if (pid_ == kUnknownProcessId) {
      continue;
    }

    if (::kill(pid_, 0) != 0) {
      if (errno == EPERM) {
        handler_(SignalType::PROCESS_ACCESS_DENIED);
      } else if (errno == ESRCH) {
        // Since we are polling the process by nullptr signal,
        // it is essentially impossible to tell the process is not found
        // or terminated.
        handler_(SignalType::PROCESS_SIGNALED);
      } else {
        handler_(SignalType::PROCESS_ERROR);
      }

      pid_ = kUnknownProcessId;
    }
  }
}

#endif  // !_WIN32

}  // namespace mozc

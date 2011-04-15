// Copyright 2010-2011, Google Inc.
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

#ifdef OS_WINDOWS
#include <windows.h>
#else
#include <signal.h>
#include <errno.h>
#endif

#include "base/base.h"
#include "base/mutex.h"
#include "base/util.h"

namespace mozc {

#ifdef OS_WINDOWS
ProcessWatchDog::ProcessWatchDog()
    : event_(NULL),
      process_id_(UnknownProcessID),
      thread_id_(UnknownThreadID),
      timeout_(-1),
      is_finished_(false) {
  event_ = ::CreateEventW(NULL, TRUE, FALSE, NULL);
  if (event_ == NULL) {
    LOG(ERROR) << "::CreateEvent() failed: "
               << ::GetLastError();
    return;
  }
  Thread::Start();  // start
}

ProcessWatchDog::~ProcessWatchDog() {
  is_finished_ = true;  // set the flag to terminate the thread

  if (event_ != NULL) {
    ::SetEvent(event_);  // wake up WaitForMultipleObjects
  }

  Join();  // wait for the thread

  if (event_ != NULL) {
    ::CloseHandle(event_);   // close the handle
    event_ = NULL;
  }
}

bool ProcessWatchDog::SetID(ProcessID process_id, ThreadID thread_id,
                            int timeout) {
  if (event_ == NULL) {
    LOG(ERROR) << "event is NULL";
    return false;
  }

  if (process_id_ == process_id && thread_id_ == thread_id &&
      timeout_ == timeout) {
    // don't repeat if we are checking the same thread/process
    return true;
  }

  // rewrite the valeus
  {
    scoped_lock l(&mutex_);
    process_id_ = process_id;
    thread_id_ = thread_id;
    timeout_ = timeout;
  }

  // wake up WaitForMultipleObjects
  ::SetEvent(event_);

  return true;
}

void ProcessWatchDog::Run() {
  while (!is_finished_) {
    HANDLE process_handle = NULL;
    HANDLE thread_handle = NULL;
    int timeout = -1;

    // read the current ids/timeout
    {
      scoped_lock l(&mutex_);

      if (process_id_ != UnknownProcessID) {
        process_handle = ::OpenProcess(SYNCHRONIZE, FALSE, process_id_);
        if (process_handle == NULL) {
          const DWORD error = ::GetLastError();
          LOG(ERROR) << "OpenProcess failed: " << process_id_ << " " << error;
          switch (error) {
            case ERROR_ACCESS_DENIED:
              Signaled(ProcessWatchDog::PROCESS_ACCESS_DENIED_SIGNALED);
              break;
            case ERROR_INVALID_PARAMETER:
              Signaled(ProcessWatchDog::PROCESS_NOT_FOUND_SIGNALED);
              break;
            default:
              Signaled(ProcessWatchDog::PROCESS_ERROR_SIGNALED);
              break;
          }
        }
      }

      if (thread_id_ != UnknownThreadID) {
        thread_handle = ::OpenThread(SYNCHRONIZE, FALSE, thread_id_);
        if (thread_handle == NULL) {
          const DWORD error = ::GetLastError();
          LOG(ERROR) << "OpenThread failed: " << thread_id_ << " " << error;
          switch (error) {
            case ERROR_ACCESS_DENIED:
              Signaled(ProcessWatchDog::THREAD_ACCESS_DENIED_SIGNALED);
              break;
            case ERROR_INVALID_PARAMETER:
              Signaled(ProcessWatchDog::THREAD_NOT_FOUND_SIGNALED);
              break;
            default:
              Signaled(ProcessWatchDog::THREAD_ERROR_SIGNALED);
              break;
          }
        }
      }

      timeout = timeout_;
      if (timeout_ < 0) {
        timeout = INFINITE;
      }

      process_id_ = UnknownProcessID;
      thread_id_ = UnknownThreadID;
      timeout_ = -1;
    }

    SignalType types[3];
    HANDLE handles[3];

    // set event
    handles[0] = event_;

    // set handles
    DWORD size = 1;
    if (process_handle != NULL) {
      VLOG(2) << "Inserting process handle";
      handles[size] = process_handle;
      types[size] = ProcessWatchDog::PROCESS_SIGNALED;
      ++size;
    }

    if (thread_handle != NULL) {
      VLOG(2) << "Inserting thread handle";
      handles[size] = thread_handle;
      types[size] = ProcessWatchDog::THREAD_SIGNALED;
      ++size;
    }

    const DWORD result = ::WaitForMultipleObjects(size, handles,
                                                  FALSE, timeout);
    SignalType result_type = ProcessWatchDog::UNKNOWN_SIGNALED;
    switch (result) {
      case WAIT_OBJECT_0:
      case WAIT_ABANDONED_0:
        VLOG(2) << "event is signaled";
        ::ResetEvent(event_);  // reset event to wait for the new request
        break;
      case WAIT_OBJECT_0 + 1:
      case WAIT_ABANDONED_0 + 1:
        VLOG(2) << "handle 1 is signaled";
        result_type = types[1];
        break;
      case WAIT_OBJECT_0 + 2:
      case WAIT_ABANDONED_0 + 2:
        VLOG(2) << "handle 2 is signaled";
        result_type = types[2];
        break;
      case WAIT_TIMEOUT:
        VLOG(2) << "timeout is signaled";
        result_type = ProcessWatchDog::TIMEOUT_SIGNALED;
        break;
      default:
        LOG(ERROR) << "WaitForMultipleObjects() failed: " << GetLastError();
        break;
    }

    if (process_handle != NULL) {
      ::CloseHandle(process_handle);
    }

    if (thread_handle != NULL) {
      ::CloseHandle(thread_handle);
    }

    if (result_type != ProcessWatchDog::UNKNOWN_SIGNALED) {
      VLOG(1) << "Sending signal: " << static_cast<int>(result_type);
      Signaled(result_type);   // call signal handler
    }
  }
}

#else  // OS_WINDOWS

ProcessWatchDog::ProcessWatchDog()
    : process_id_(UnknownProcessID),
      thread_id_(UnknownProcessID),
      is_finished_(false) {
  Thread::Start();
}

ProcessWatchDog::~ProcessWatchDog() {
  is_finished_ = true;
  Join();
}

bool ProcessWatchDog::SetID(ProcessWatchDog::ProcessID process_id,
                            ProcessWatchDog::ThreadID thread_id,
                            int timeout) {
  if (process_id_ == process_id && thread_id_ == thread_id &&
      timeout_ == timeout) {
    // don't repeat if we are checking the same thread/process
    return true;
  }

  LOG_IF(ERROR, thread_id != UnknownThreadID)
      << "Linux/Mac don't allow to capture ThreadID";
  LOG_IF(ERROR, timeout > 0) << "timeout is not supported";

  if (::kill(process_id, 0) != 0) {
    if (errno == ESRCH) {
      // emit PROCESS_NOT_FOUND_SIGNALED immediately,
      // if the process id is not found NOW.
      Signaled(ProcessWatchDog::PROCESS_NOT_FOUND_SIGNALED);
      process_id = UnknownProcessID;
    }
  }

  {
    scoped_lock l(&mutex_);
    process_id_ = process_id;
    thread_id_ = thread_id;
    timeout_ = -1;
  }

  return true;
}

void ProcessWatchDog::Run() {
  // Polling-based watch-dog.
  // Seems Linux/Mac have no event-driven API
  // like WaitForMultipleObjects on Windows.
  // NOTE In theory, there may possibility that some other process
  // reuse same process id in 250ms or write to is_finished_ stays
  // forever in another CPU's local cache.
  while (!is_finished_) {
    Util::Sleep(250);
    if (process_id_ == UnknownProcessID) {
      continue;
    }
    if (::kill(process_id_, 0) != 0) {
      if (errno == EPERM) {
        Signaled(ProcessWatchDog::PROCESS_ACCESS_DENIED_SIGNALED);
      } else if (errno == ESRCH) {
        // Since we are polling the process by NULL signal,
        // it is essentially impossible to tell the process is not found
        // or terminated.
        Signaled(ProcessWatchDog::PROCESS_SIGNALED);
      } else {
        Signaled(ProcessWatchDog::PROCESS_ERROR_SIGNALED);
      }
      scoped_lock l(&mutex_);
      process_id_ = UnknownProcessID;
    }
  }
}
#endif  // OS_WINDOWS
}  // mozc

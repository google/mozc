// Copyright 2010, Google Inc.
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

#ifndef MOZC_BASE_THREAD_H_
#define MOZC_BASE_THREAD_H_

#ifdef OS_WINDOWS
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

#ifdef OS_WINDOWS
#define BEGINTHREAD(src, stack, func, arg, flag, id) \
     (HANDLE)_beginthreadex((void *)(src), (unsigned)(stack), \
                       (unsigned(_stdcall *)(void *))(func), (void *)(arg), \
                       (unsigned)(flag), (unsigned *)(id))
#endif

namespace mozc {

class Thread {
 public:
  virtual void Run() {}

  void Start() {
    if (IsRunning()) {
      return;
    }

    Detach();
#ifdef OS_WINDOWS
    DWORD id = 0;
    handle_ = BEGINTHREAD(0, 0, &Thread::Wrapper, this, 0, &id);
#else
    is_running_ = true;
    if (0 != pthread_create(&handle_, 0, &Thread::Wrapper,
                            static_cast<void *>(this))) {
      is_running_ = false;
    }
#endif
  }

  bool IsRunning() const {
#ifdef OS_WINDOWS
    DWORD result = 0;
    if (handle_ == NULL || !::GetExitCodeThread(handle_, &result)) {
      return false;
    }
    return (STILL_ACTIVE == result);
#else
    return is_running_;
#endif
  }

  void SetJoinable(bool joinable) {
    joinable_ = joinable;
  }

  void Join() {
    if (!joinable_) {
      return;
    }
    if (!handle_) {
      return;
    }
#ifdef OS_WINDOWS
    ::WaitForSingleObject(handle_, INFINITE);
    ::CloseHandle(handle_);
    handle_ = NULL;
#else
    pthread_join(handle_, NULL);
    handle_ = 0;
#endif
  }

  // This method is not encouraged especiialy in Windows.
  void Terminate() {
    if (handle_) {
#ifdef OS_WINDOWS
      ::TerminateThread(handle_, 0);
      handle_ = NULL;
#else
      pthread_cancel(handle_);
      handle_ = 0;
#endif
    }
  }

  Thread() : handle_(0),
#ifndef OS_WINDOWS
             is_running_(false),
#endif
             joinable_(true) {}

  virtual ~Thread() {
    Detach();
  }

 private:
  void Detach() {
    if (handle_ != 0) {
#ifdef OS_WINDOWS
      ::CloseHandle(handle_);
#endif
    }
    handle_ = 0;
  }

  static void* Wrapper(void *ptr) {
    Thread *p = static_cast<Thread *>(ptr);
#ifndef OS_WINDOWS
    pthread_cleanup_push(&Thread::Cleanup, static_cast<void *>(&p->is_running_));
#endif
    p->Run();
#ifndef OS_WINDOWS
    pthread_cleanup_pop(1);
#endif
    return NULL;
  }

#ifndef OS_WINDOWS
  static void Cleanup(void *ptr) {
    bool *is_running = static_cast<bool *>(ptr);
    *is_running = false;
  }
#endif

#ifdef OS_WINDOWS
  HANDLE  handle_;
#else
  pthread_t handle_;
  bool is_running_;
#endif
  bool joinable_;
};
}
#ifdef OS_WINDOWS
#undef BEGINTHREAD
#endif
#endif  // MOZC_BASE_THREAD_H_

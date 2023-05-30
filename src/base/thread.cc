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

#include "base/thread.h"

#ifdef _WIN32
#include <process.h>  // for _beginthreadex
#include <windows.h>
#else  // _WIN32
#include <pthread.h>
#endif  // _WIN32

#include <atomic>
#include <memory>
#include <string>

#include "base/logging.h"

namespace mozc {

#ifdef _WIN32
// Win32-based Thread implementation.

namespace {

unsigned __stdcall WrapperForWindows(void *ptr) {
  Thread *p = static_cast<Thread *>(ptr);
  p->Run();
  return 0;
}

}  // namespace

struct ThreadInternalState {
 public:
  ThreadInternalState() : handle(nullptr), joinable(true) {}

  HANDLE handle;
  bool joinable;
};

void Thread::Start(const std::string &thread_name) {
  // TODO(mozc-dev): Set thread name.
  if (IsRunning()) {
    return;
  }

  Detach();
  state_->handle = reinterpret_cast<HANDLE>(
      _beginthreadex(nullptr, 0, WrapperForWindows, this, 0, nullptr));
}

bool Thread::IsRunning() const {
  DWORD result = 0;
  if (state_->handle == nullptr ||
      !::GetExitCodeThread(state_->handle, &result)) {
    return false;
  }
  return (STILL_ACTIVE == result);
}

void Thread::Detach() {
  if (state_->handle != nullptr) {
    ::CloseHandle(state_->handle);
    state_->handle = nullptr;
  }
}

void Thread::Join() {
  if (!state_->joinable) {
    return;
  }
  if (state_->handle == nullptr) {
    return;
  }
  ::WaitForSingleObject(state_->handle, INFINITE);
  ::CloseHandle(state_->handle);
  state_->handle = nullptr;
}

void Thread::Terminate() {
  if (state_->handle != nullptr) {
    ::TerminateThread(state_->handle, 0);
    state_->handle = nullptr;
  }
}

#else  // _WIN32
// Thread implementation for pthread-based platforms. Currently all the
// platforms except for Windows use pthread.

struct ThreadInternalState {
 public:
  ThreadInternalState() : is_running(false), joinable(true) {}

  // As pthread_t is an opaque object, we use (pthread_t *)nullptr to
  // indicate that no thread is attached to this object.
  // When |handle != nullptr|, |*handle| should indicate a
  // valid thread id.
  std::unique_ptr<pthread_t> handle;
  std::atomic<bool> is_running;
  bool joinable;
};

void Thread::Start(const std::string &thread_name) {
  if (IsRunning()) {
    return;
  }

  Detach();
  state_->is_running = true;
  state_->handle.reset(new pthread_t);
  if (0 != pthread_create(state_->handle.get(), nullptr,
                          &Thread::WrapperForPOSIX,
                          static_cast<void *>(this))) {
    state_->is_running = false;
    state_->handle.reset();
  } else {
#if defined(__wasm__)
    // WASM doesn't support setname?
#elif defined(__APPLE__)  // !__wasm__
    pthread_setname_np(thread_name.c_str());
#else                     // !(__wasm__ | __APPLE__)
    pthread_setname_np(*state_->handle, thread_name.c_str());
#endif                    // !(__wasm__ | __APPLE__)
  }
}

bool Thread::IsRunning() const { return state_->is_running; }

void Thread::Detach() {
  if (state_->handle != nullptr) {
    pthread_detach(*state_->handle);
    state_->handle.reset();
  }
}

void Thread::Join() {
  if (!state_->joinable) {
    return;
  }
  if (state_->handle == nullptr) {
    return;
  }
  pthread_join(*state_->handle, nullptr);
  state_->handle.reset();
}

namespace {

#ifdef __ANDROID__

void ExitThread(int sig) { pthread_exit(0); }

// We don't have pthread_cancel for Android, so we'll use SIGUSR1 as
// work around.
void InitPThreadCancel() {
  struct sigaction actions = {};
  sigemptyset(&actions.sa_mask);
  actions.sa_flags = 0;
  actions.sa_handler = ExitThread;
  sigaction(SIGUSR1, &actions, nullptr);
}

void PThreadCancel(pthread_t thread_id) {
  const int pthread_kill_result = pthread_kill(thread_id, SIGUSR1);
  if (pthread_kill_result != 0) {
    // pthread_kill fails if
    //  EINVAL: in case that the specified handle is invalid
    //  ESRCH: in case that the thread is already terminated
    LOG(ERROR) << "Failed to kill a thread. error = " << pthread_kill_result
               << "(" << strerror(pthread_kill_result) << ")";
  }
}

#else  // __ANDROID__

void InitPThreadCancel() {
  // Nothing is required.
}

void PThreadCancel(pthread_t thread_id) { pthread_cancel(thread_id); }

#endif  // __ANDROID__ or others

void PThreadCleanupRoutine(void *ptr) {
  auto *is_running = static_cast<std::atomic<bool> *>(ptr);
  *is_running = false;
}

}  // namespace

void *Thread::WrapperForPOSIX(void *ptr) {
  Thread *p = static_cast<Thread *>(ptr);
  InitPThreadCancel();
  {
    // Caveat: the pthread_cleanup_push/pthread_cleanup_pop pair should be put
    //     in the same function. Never move them into any other function.
    pthread_cleanup_push(PThreadCleanupRoutine,
                         static_cast<void *>(&p->state_->is_running));
    p->Run();
    pthread_cleanup_pop(1);
  }
  return nullptr;
}

void Thread::Terminate() {
  if (state_->handle != nullptr) {
    PThreadCancel(*state_->handle);
    // pthread_cancel (or pthread_kill in PThreadCancel on Android) is
    // asynchronous. Join the thread to behave like TerminateThread on Windows.
    Join();
    state_->handle.reset();
  }
}

#endif  // _WIN32

Thread::Thread() : state_(new ThreadInternalState) {}

Thread::~Thread() { Detach(); }

void Thread::SetJoinable(bool joinable) { state_->joinable = joinable; }

}  // namespace mozc

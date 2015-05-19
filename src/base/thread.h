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

#ifndef MOZC_BASE_THREAD_H_
#define MOZC_BASE_THREAD_H_

#ifdef OS_WINDOWS
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif


// Definition of TLS (Thread Local Storage) keyword.
#ifndef TLS_KEYWORD

#if defined(OS_WINDOWS) && defined(_MSC_VER)
// Use VC's keyword on Windows.
//
// On Windows XP, thread local has a limitation. Since thread local
// is now used inside converter and converter is a stand alone program,
// it's not a issue at this moment.
//
// http://msdn.microsoft.com/en-us/library/9w1sdazb.aspx
// Thread-local variables work for EXEs and DLLs that are statically
// linked to the main executable (directly or via another DLL) -
// i.e. the DLL has to load before the main executable code starts
// running.
#define TLS_KEYWORD __declspec(thread)
#define HAVE_TLS 1
#endif // OS_WINDOWS && _MSC_VER

// Andorid NDK doesn't support TLS.
#if defined(OS_LINUX) && !defined(OS_ANDROID) && \
    (defined(__GNUC__) || defined(__clang__))
// GCC and Clang support TLS.
#define TLS_KEYWORD __thread
#define HAVE_TLS 1
#endif  // OS_LINUX && !OS_ANDROID && (__GNUC__ || __clang__)


#if defined(OS_MACOSX) && \
    defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 405)
// GCC 4.5 and later can *emulate* TLS on Mac even though it is
// expensive operation.
#define TLS_KEYWORD __thread
#define HAVE_TLS 1
#endif  // OS_MACOSX && GCC 4.5 and later

#endif  // !TLS_KEYWORD

#ifndef TLS_KEYWORD
// If TLS is not available, define TLS_KEYWORD as a dummy keyword.
#define TLS_KEYWORD
#endif  // !TLS_KEYWORD

// Hereafter, we can use TLS_KEYWORD like a keyword.
#ifndef TLS_KEYWORD
#error TLS_KEYWORD should be defined here.
#endif  // !TLS_KEYWORD


namespace mozc {

class Thread {
 public:
  virtual void Run() { }

  void Start() {
    if (IsRunning()) {
      return;
    }

    Detach();
#ifdef OS_WINDOWS
    handle_ = reinterpret_cast<HANDLE>(
                  _beginthreadex(NULL, 0, &Thread::WrapperForWindows,
                                 this, 0, NULL));
#else
    is_running_ = true;
    if (0 != pthread_create(&handle_, 0, &Thread::WrapperForPOSIX,
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
#endif  // OS_WINDOWS
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

#if defined(OS_WINDOWS)
  static unsigned __stdcall WrapperForWindows(void *ptr) {
    Thread *p = static_cast<Thread *>(ptr);
    p->Run();
    return 0;
  }
#else
  static void * WrapperForPOSIX(void *ptr) {
    Thread *p = static_cast<Thread *>(ptr);
    pthread_cleanup_push(&Thread::Cleanup, static_cast<void *>(&p->is_running_));
    p->Run();
    pthread_cleanup_pop(1);
    return NULL;
  }

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

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

#ifndef MOZC_BASE_MUTEX_H_
#define MOZC_BASE_MUTEX_H_

#ifdef OS_WINDOWS
#include <windows.h>
#else
#include <pthread.h>
#endif


#include "base/base.h"

namespace mozc {

// Usage:
// static mozc::Mutex mutex;
//
// void foo() {
//   mozc::scoped_lock lock(&mutex);
//   ..
// };

#ifdef OS_WINDOWS
class Mutex : CRITICAL_SECTION {
 public:
  Mutex() {
    ::InitializeCriticalSection(this);
  }

  virtual ~Mutex() {
    ::DeleteCriticalSection(this);
  }

  void Lock() {
    ::EnterCriticalSection(this);
  }

  void Unlock() {
    ::LeaveCriticalSection(this);
  }

  // Fallback implementations in case that ReaderWriterLock is not available.
  void ReaderLock() { Lock(); }
  void WriterLock() { Lock(); }
  void ReaderUnlock() { Unlock(); }
  void WriterUnlock() { Unlock(); }
};

// TODO(taku): implement ReaderWriterMutex for Windows.
// e.g., http://msdn.microsoft.com/en-us/library/windows/desktop/ms683483.aspx
typedef Mutex ReaderWriterMutex;

#else   // OS_WINDOWS

class Mutex {
 public:
  Mutex() {
    // make RECURSIVE lock:
    // default setting is no-recursive lock.
    // when mutex.Lock() is called while the mutex is already locked,
    // mutex will be blocked. This behavior is not compatible with windows.
    // We set PTHREAD_MUTEX_RECURSIVE_NP to make the mutex recursive-lock

    // PTHREAD_MUTEX_RECURSIVE_NP and PTHREAD_MUTEX_RECURSIVE seem to be
    // variants.  For example, Mac OS X 10.4 had
    // PTHREAD_MUTEX_RECURSIVE_NP but Mac OS X 10.5 does not
#ifdef OS_MACOSX
#define PTHREAD_MUTEX_RECURSIVE_VALUE PTHREAD_MUTEX_RECURSIVE
#endif

#ifdef OS_LINUX
#define PTHREAD_MUTEX_RECURSIVE_VALUE PTHREAD_MUTEX_RECURSIVE_NP
#endif

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_VALUE);
    pthread_mutex_init(&mutex_, &attr);
  }

  virtual ~Mutex() {
    pthread_mutex_destroy(&mutex_);
  }

  void Lock() {
    pthread_mutex_lock(&mutex_);
  }

  void Unlock() {
    pthread_mutex_unlock(&mutex_);
  }

  // Fallback implementations in case that ReaderWriterLock is not available.
  void ReaderLock() { Lock(); }
  void WriterLock() { Lock(); }
  void ReaderUnlock() { Unlock(); }
  void WriterUnlock() { Unlock(); }

 private:
  pthread_mutex_t mutex_;
};

#if defined(OS_ANDROID) && defined(__ANDROID_API__) && (__ANDROID_API__ < 9) || \
    defined(__native_client__)
// pthread_rwlock_init is not available < Android ver. 2.3 and NaCl.
// TODO(all): Enables ReaderWriterMutex on Android when API Level >= 9.
typedef Mutex ReaderWriterMutex;
#else  // (!OS_ANDROID || __ANDROID_API__ >= 9) && !__native_client__

#define HAVE_READER_WRITER_MUTEX

class ReaderWriterMutex {
 public:
  ReaderWriterMutex() {
    pthread_rwlock_init(&mutex_, NULL);
  }

  virtual ~ReaderWriterMutex() {
    pthread_rwlock_destroy(&mutex_);
  }

  void ReaderLock() {
    pthread_rwlock_rdlock(&mutex_);
  }

  void ReaderUnlock() {
    pthread_rwlock_unlock(&mutex_);
  }

  void WriterLock() {
    pthread_rwlock_wrlock(&mutex_);
  }

  void WriterUnlock() {
    pthread_rwlock_unlock(&mutex_);
  }

 private:
  pthread_rwlock_t mutex_;
};
#endif  // (!OS_ANDROID || __ANDROID_API__ >= 9) && !__native_client__
#endif  // OS_WINDOWS

class scoped_lock {
 public:
  void Lock() { mutex_->Lock(); }
  void Unlock() { mutex_->Unlock(); }

  explicit scoped_lock(Mutex *mutex): mutex_(mutex) {
    Lock();
  }

  virtual ~scoped_lock() {
    Unlock();
  }

 private:
  Mutex *mutex_;
  scoped_lock() {}
};

class scoped_writer_lock {
 public:
  explicit scoped_writer_lock(ReaderWriterMutex *mutex) : mutex_(mutex) {
    mutex_->WriterLock();
  }
  ~scoped_writer_lock() {
    mutex_->WriterUnlock();
  }
 private:
  ReaderWriterMutex *mutex_;
};

class scoped_reader_lock {
 public:
  explicit scoped_reader_lock(ReaderWriterMutex *mutex) : mutex_(mutex) {
    mutex_->ReaderLock();
  }
  ~scoped_reader_lock() {
    mutex_->ReaderUnlock();
  }
 private:
  ReaderWriterMutex *mutex_;
};

typedef scoped_lock MutexLock;
typedef scoped_reader_lock ReaderMutexLock;
typedef scoped_writer_lock WriterMutexLock;

enum CallOnceState {
  ONCE_INIT = 0,
  ONCE_DONE = 1,
};
#define MOZC_ONCE_INIT { 0, 0 }

struct once_t {
#ifdef OS_WINDOWS
  volatile LONG state;
  volatile LONG counter;
#else
  volatile int state;
  volatile int counter;
#endif
};

// portable re-implementation of pthread_once
void CallOnce(once_t *once, void (*func)());

// reset once_t
void ResetOnce(once_t *once);
}  // mozc
#endif  // MOZC_BASE_MUTEX_H_

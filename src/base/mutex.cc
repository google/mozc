// Copyright 2010-2020, Google Inc.
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

#include "base/mutex.h"

#ifdef OS_WIN
#include <Windows.h>
#else
#include <pthread.h>
#endif

#include <atomic>

#include "base/port.h"

#if defined(OS_WIN)
// We do not use pthread on Windows
#elif defined(OS_NACL) || defined(OS_WASM)
// TODO(team): Consider to use glibc rwlock.
#else
#define MOZC_PTHREAD_HAS_READER_WRITER_LOCK
#endif

namespace mozc {
namespace {

// State for once_t.
enum CallOnceState {
  ONCE_INIT = 0,
  ONCE_RUNNING = 1,
  ONCE_DONE = 2,
};

}  // namespace

#ifdef OS_WIN  // Hereafter, we have Win32 implementations
namespace {

template <typename T>
CRITICAL_SECTION *AsCriticalSection(T *opaque_buffer) {
  static_assert(sizeof(T) >= sizeof(CRITICAL_SECTION),
                "The opaque buffer must have sufficient space to store a "
                "CRITICAL_SECTION structure");
  return reinterpret_cast<CRITICAL_SECTION *>(opaque_buffer);
}

template <typename T>
SRWLOCK *AsSRWLock(T *opaque_buffer) {
  static_assert(sizeof(T) >= sizeof(SRWLOCK),
                "The opaque buffer must have sufficient space to store a "
                "SRWLOCK structure");
  return reinterpret_cast<SRWLOCK *>(opaque_buffer);
}

}  // namespace

Mutex::Mutex() {
  ::InitializeCriticalSection(AsCriticalSection(&opaque_buffer_));
}

Mutex::~Mutex() { ::DeleteCriticalSection(AsCriticalSection(&opaque_buffer_)); }

void Mutex::Lock() {
  ::EnterCriticalSection(AsCriticalSection(&opaque_buffer_));
}

bool Mutex::TryLock() {
  return ::TryEnterCriticalSection(AsCriticalSection(&opaque_buffer_)) != FALSE;
}

void Mutex::Unlock() {
  ::LeaveCriticalSection(AsCriticalSection(&opaque_buffer_));
}

ReaderWriterMutex::ReaderWriterMutex() {
  ::InitializeSRWLock(AsSRWLock(&opaque_buffer_));
}

ReaderWriterMutex::~ReaderWriterMutex() {}

void ReaderWriterMutex::ReaderLock() {
  ::AcquireSRWLockShared(AsSRWLock(&opaque_buffer_));
}

void ReaderWriterMutex::WriterLock() {
  ::AcquireSRWLockExclusive(AsSRWLock(&opaque_buffer_));
}

void ReaderWriterMutex::ReaderUnlock() {
  ::ReleaseSRWLockShared(AsSRWLock(&opaque_buffer_));
}

void ReaderWriterMutex::WriterUnlock() {
  ::ReleaseSRWLockExclusive(AsSRWLock(&opaque_buffer_));
}

bool ReaderWriterMutex::MultipleReadersThreadsSupported() { return true; }

#else  // Hereafter, we have pthread-based implementation

namespace {

template <typename T>
pthread_mutex_t *AsPthreadMutexT(T *opaque_buffer) {
  static_assert(sizeof(T) >= sizeof(pthread_mutex_t),
                "The opaque buffer must have sufficient space to store a "
                "pthread_mutex_t structure");
  return reinterpret_cast<pthread_mutex_t *>(opaque_buffer);
}

}  // namespace

Mutex::Mutex() {
  // make RECURSIVE lock:
  // The default is no-recursive lock.
  // When mutex.Lock() is called while the mutex is already locked,
  // mutex will be blocked. This behavior is not compatible with windows.
  // We set PTHREAD_MUTEX_RECURSIVE_NP to make the mutex recursive-lock

  // PTHREAD_MUTEX_RECURSIVE_NP and PTHREAD_MUTEX_RECURSIVE seem to be
  // variants.  For example, Mac OS X 10.4 had
  // PTHREAD_MUTEX_RECURSIVE_NP but Mac OS X 10.5 does not
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
#if defined(__APPLE__) || defined(OS_WASM)
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
#elif defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_NACL)
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
#else
#error "This platform is not supported."
#endif
  pthread_mutex_init(AsPthreadMutexT(&opaque_buffer_), &attr);
}

Mutex::~Mutex() { pthread_mutex_destroy(AsPthreadMutexT(&opaque_buffer_)); }

void Mutex::Lock() { pthread_mutex_lock(AsPthreadMutexT(&opaque_buffer_)); }

bool Mutex::TryLock() {
  return pthread_mutex_trylock(AsPthreadMutexT(&opaque_buffer_)) == 0;
}

void Mutex::Unlock() { pthread_mutex_unlock(AsPthreadMutexT(&opaque_buffer_)); }

#ifdef MOZC_PTHREAD_HAS_READER_WRITER_LOCK
// Use pthread native reader writer lock.
namespace {

template <typename T>
pthread_rwlock_t *AsPthreadRWLockT(T *opaque_buffer) {
  static_assert(sizeof(T) >= sizeof(pthread_rwlock_t),
                "The opaque buffer must have sufficient space to store a "
                "pthread_rwlock_t structure");
  return reinterpret_cast<pthread_rwlock_t *>(opaque_buffer);
}

}  // namespace

ReaderWriterMutex::ReaderWriterMutex() {
  pthread_rwlock_init(AsPthreadRWLockT(&opaque_buffer_), nullptr);
}

ReaderWriterMutex::~ReaderWriterMutex() {
  pthread_rwlock_destroy(AsPthreadRWLockT(&opaque_buffer_));
}

void ReaderWriterMutex::ReaderLock() {
  pthread_rwlock_rdlock(AsPthreadRWLockT(&opaque_buffer_));
}

void ReaderWriterMutex::ReaderUnlock() {
  pthread_rwlock_unlock(AsPthreadRWLockT(&opaque_buffer_));
}

void ReaderWriterMutex::WriterLock() {
  pthread_rwlock_wrlock(AsPthreadRWLockT(&opaque_buffer_));
}

void ReaderWriterMutex::WriterUnlock() {
  pthread_rwlock_unlock(AsPthreadRWLockT(&opaque_buffer_));
}

bool ReaderWriterMutex::MultipleReadersThreadsSupported() { return true; }

#else

// Fallback implementations as ReaderWriterLock is not available.
ReaderWriterMutex::ReaderWriterMutex() {
  // Non-recursive lock is OK.
  pthread_mutex_init(AsPthreadMutexT(&opaque_buffer_), nullptr);
}

ReaderWriterMutex::~ReaderWriterMutex() {
  pthread_mutex_destroy(AsPthreadMutexT(&opaque_buffer_));
}

void ReaderWriterMutex::ReaderLock() {
  pthread_mutex_lock(AsPthreadMutexT(&opaque_buffer_));
}

void ReaderWriterMutex::WriterLock() {
  pthread_mutex_lock(AsPthreadMutexT(&opaque_buffer_));
}

void ReaderWriterMutex::ReaderUnlock() {
  pthread_mutex_unlock(AsPthreadMutexT(&opaque_buffer_));
}

void ReaderWriterMutex::WriterUnlock() {
  pthread_mutex_unlock(AsPthreadMutexT(&opaque_buffer_));
}

bool ReaderWriterMutex::MultipleReadersThreadsSupported() { return false; }

#endif  // MOZC_PTHREAD_HAS_READER_WRITER_LOCK

#endif  // OS_WIN or pthread

void CallOnce(once_t *once, void (*func)()) {
  if (once == nullptr || func == nullptr) {
    return;
  }
  int expected_state = ONCE_INIT;
  if (once->compare_exchange_strong(expected_state, ONCE_RUNNING)) {
    (*func)();
    *once = ONCE_DONE;
    return;
  }
  // If the above compare_exchange_strong() returns false, it stores the value
  // of once to expected_state, which is ONCE_RUNNING or ONCE_DONE.
  if (expected_state == ONCE_DONE) {
    return;
  }
  // Here's the case where expected_state == ONCE_RUNNING, indicating that
  // another thread is calling func.  Wait for it to complete.
  while (*once == ONCE_RUNNING) {
#ifdef OS_WIN
    ::YieldProcessor();
#endif  // OS_WIN
  }     // Busy loop
}

void ResetOnce(once_t *once) {
  if (once) {
    *once = ONCE_INIT;
  }
}

}  // namespace mozc

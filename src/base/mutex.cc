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

#include "base/mutex.h"

#ifdef OS_WIN
#include <Windows.h>
#else
#include <pthread.h>
#endif

#ifdef OS_MACOSX
#include <libkern/OSAtomic.h>
#endif  // OS_MACOSX

#include "base/base.h"
#include "base/util.h"
#include "base/win_util.h"

#if defined(OS_WIN)
// We do not use pthread on Windows
#elif defined(OS_ANDROID)
// pthread rwlock is supported since API Level 9.
// Currently minimum API Level is 7 so we cannot use it.
// Note that we cannot use __ANDROID_API__ macro in above condition
// because it is equal to target API Level, which is greater than
// min sdk level. Causes runtime crash.
#elif defined(__native_client__)
// TODO(team): Consider to use glibc rwlock.
#else
#define MOZC_PTHREAD_HAS_READER_WRITER_LOCK
#endif

namespace mozc {

// Wrapper for Windows InterlockedCompareExchange
namespace {
#ifdef OS_LINUX
// Linux doesn't provide InterlockedCompareExchange-like function.
inline int InterlockedCompareExchange(volatile int *target,
                                      int new_value,
                                      int old_value) {
  // TODO(yusukes): For now, we use the architecture-neutral implementation,
  // but I believe it's definitely better to port Chromium's singleton to Mozc.
  // The implementation should be much faster and supports ARM Linux.
  // http://src.chromium.org/viewvc/chrome/trunk/src/base/singleton.h

  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&lock);
  int result = *target;
  if (result == old_value) {
    *target = new_value;
  }
  pthread_mutex_unlock(&lock);
  return result;
}
#endif  // OS_LINUX

// Use OSAtomicCompareAndSwapInt on Mac OSX
// http://developer.apple.com/iphone/library/documentation/
// system/conceptual/manpages_iphoneos/man3/OSAtomicCompareAndSwapInt.3.html
// TODO(taku): should we use OSAtomicCompareAndSwapIntBarrier?
#ifdef OS_MACOSX
inline int InterlockedCompareExchange(volatile int *target,
                                      int new_value,
                                      int old_value) {
  return OSAtomicCompareAndSwapInt(old_value, new_value, target)
      ? old_value : *target;
}
#endif  // OX_MACOSX

}  // namespace

#ifdef OS_WIN  // Hereafter, we have Win32 implementations
namespace {

template <typename T>
CRITICAL_SECTION *AsCriticalSection(T* opaque_buffer) {
  COMPILE_ASSERT(sizeof(T) >= sizeof(CRITICAL_SECTION),
                 opaque_buffer_size_check);
  return reinterpret_cast<CRITICAL_SECTION *>(opaque_buffer);
}

template <typename T>
SRWLOCK *AsSRWLock(T* opaque_buffer) {
  COMPILE_ASSERT(sizeof(T) >= sizeof(SRWLOCK), opaque_buffer_size_check);
  return reinterpret_cast<SRWLOCK *>(opaque_buffer);
}

// SlimReaderWriterLock is available on Windows Vista and later.
class SlimReaderWriterLock {
 public:
  static bool IsAvailable() {
    CallOnce(&g_once_, InitializeInternal);
    return g_is_available_;
  }
  static void InitializeSRWLock(__out SRWLOCK* lock) {
    g_initialize_srw_lock_(lock);
  }
  static void AcquireSRWLockExclusive(__inout SRWLOCK* lock) {
    g_acquire_srw_lock_exclusive_(lock);
  }
  static void AcquireSRWLockShared(__inout SRWLOCK* lock) {
    g_acquire_srw_lock_shared_(lock);
  }
  static void ReleaseSRWLockExclusive(__inout SRWLOCK* lock) {
    g_release_srw_lock_exclusive_(lock);
  }
  static void ReleaseSRWLockShared(__inout SRWLOCK* lock) {
    g_release_srw_lock_shared_(lock);
  }

 private:
  typedef void (WINAPI *FPInitializeSRWLock)(__out SRWLOCK*);
  typedef void (WINAPI *FPAcquireSRWLockExclusive)(__inout SRWLOCK*);
  typedef void (WINAPI *FPAcquireSRWLockShared)(__inout SRWLOCK*);
  typedef void (WINAPI *FPReleaseSRWLockExclusive)(__inout SRWLOCK*);
  typedef void (WINAPI *FPReleaseSRWLockShared)(__inout SRWLOCK*);

  static void InitializeInternal() {
    g_is_available_ = false;
    const HMODULE module = WinUtil::GetSystemModuleHandle(L"kernel32.dll");
    if (module == NULL) {
      return;
    }
    g_initialize_srw_lock_ = reinterpret_cast<FPInitializeSRWLock>(
        ::GetProcAddress(module, "InitializeSRWLock"));
    if (g_initialize_srw_lock_ == NULL) {
      return;
    }
    g_acquire_srw_lock_exclusive_ =
        reinterpret_cast<FPAcquireSRWLockExclusive>(
            ::GetProcAddress(module, "AcquireSRWLockExclusive"));
    if (g_acquire_srw_lock_exclusive_ == NULL) {
      return;
    }
    g_acquire_srw_lock_shared_ = reinterpret_cast<FPAcquireSRWLockShared>(
        ::GetProcAddress(module, "AcquireSRWLockShared"));
    if (g_acquire_srw_lock_shared_ == NULL) {
      return;
    }
    g_release_srw_lock_exclusive_ =
        reinterpret_cast<FPReleaseSRWLockExclusive>(
            ::GetProcAddress(module, "ReleaseSRWLockExclusive"));
    if (g_release_srw_lock_exclusive_ == NULL) {
      return;
    }
    g_release_srw_lock_shared_ = reinterpret_cast<FPReleaseSRWLockShared>(
        ::GetProcAddress(module, "ReleaseSRWLockShared"));
    if (g_release_srw_lock_shared_ == NULL) {
      return;
    }
    g_is_available_ = true;
  }

  static once_t g_once_;
  static bool g_is_available_;
  static FPInitializeSRWLock g_initialize_srw_lock_;
  static FPAcquireSRWLockExclusive g_acquire_srw_lock_exclusive_;
  static FPAcquireSRWLockShared g_acquire_srw_lock_shared_;
  static FPReleaseSRWLockExclusive g_release_srw_lock_exclusive_;
  static FPReleaseSRWLockShared g_release_srw_lock_shared_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SlimReaderWriterLock);
};

once_t SlimReaderWriterLock::g_once_ = MOZC_ONCE_INIT;
bool SlimReaderWriterLock::g_is_available_ = false;
SlimReaderWriterLock::FPInitializeSRWLock
    SlimReaderWriterLock::g_initialize_srw_lock_ = NULL;
SlimReaderWriterLock::FPAcquireSRWLockExclusive
    SlimReaderWriterLock::g_acquire_srw_lock_exclusive_  = NULL;
SlimReaderWriterLock::FPAcquireSRWLockShared
    SlimReaderWriterLock::g_acquire_srw_lock_shared_  = NULL;
SlimReaderWriterLock::FPReleaseSRWLockExclusive
    SlimReaderWriterLock::g_release_srw_lock_exclusive_  = NULL;
SlimReaderWriterLock::FPReleaseSRWLockShared
    SlimReaderWriterLock::g_release_srw_lock_shared_  = NULL;

}  // namespace

Mutex::Mutex() {
  ::InitializeCriticalSection(AsCriticalSection(&opaque_buffer_));
}

Mutex::~Mutex() {
  ::DeleteCriticalSection(AsCriticalSection(&opaque_buffer_));
}

void Mutex::Lock() {
  ::EnterCriticalSection(AsCriticalSection(&opaque_buffer_));
}

void Mutex::Unlock() {
  ::LeaveCriticalSection(AsCriticalSection(&opaque_buffer_));
}

ReaderWriterMutex::ReaderWriterMutex() {
  if (MultipleReadersThreadsSupported()) {
    SlimReaderWriterLock::InitializeSRWLock(AsSRWLock(&opaque_buffer_));
  } else {
    ::InitializeCriticalSection(AsCriticalSection(&opaque_buffer_));
  }
}

ReaderWriterMutex::~ReaderWriterMutex() {
  if (!MultipleReadersThreadsSupported()) {
    ::DeleteCriticalSection(AsCriticalSection(&opaque_buffer_));
  }
}

void ReaderWriterMutex::ReaderLock() {
  if (MultipleReadersThreadsSupported()) {
    SlimReaderWriterLock::AcquireSRWLockShared(AsSRWLock(&opaque_buffer_));
  } else {
    ::EnterCriticalSection(AsCriticalSection(&opaque_buffer_));
  }
}

void ReaderWriterMutex::WriterLock() {
  if (MultipleReadersThreadsSupported()) {
    SlimReaderWriterLock::AcquireSRWLockExclusive(AsSRWLock(&opaque_buffer_));
  } else {
    ::EnterCriticalSection(AsCriticalSection(&opaque_buffer_));
  }
}

void ReaderWriterMutex::ReaderUnlock() {
  if (MultipleReadersThreadsSupported()) {
    SlimReaderWriterLock::ReleaseSRWLockShared(AsSRWLock(&opaque_buffer_));
  } else {
    ::LeaveCriticalSection(AsCriticalSection(&opaque_buffer_));
  }
}

void ReaderWriterMutex::WriterUnlock() {
  if (MultipleReadersThreadsSupported()) {
    SlimReaderWriterLock::ReleaseSRWLockExclusive(AsSRWLock(&opaque_buffer_));
  } else {
    ::LeaveCriticalSection(AsCriticalSection(&opaque_buffer_));
  }
}

bool ReaderWriterMutex::MultipleReadersThreadsSupported() {
  return SlimReaderWriterLock::IsAvailable();
}

#else  // Hereafter, we have pthread-based implementation

namespace {

template <typename T>
pthread_mutex_t *AsPthreadMutexT(T* opaque_buffer) {
  COMPILE_ASSERT(sizeof(T) >= sizeof(pthread_mutex_t),
                 opaque_buffer_size_check);
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
#if defined(OS_MACOSX)
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
#elif defined(OS_LINUX)
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
#else
#error "This platform is not supported."
#endif
  pthread_mutex_init(AsPthreadMutexT(&opaque_buffer_), &attr);
}

Mutex::~Mutex() {
  pthread_mutex_destroy(AsPthreadMutexT(&opaque_buffer_));
}

void Mutex::Lock() {
  pthread_mutex_lock(AsPthreadMutexT(&opaque_buffer_));
}

void Mutex::Unlock() {
  pthread_mutex_unlock(AsPthreadMutexT(&opaque_buffer_));
}

#ifdef MOZC_USE_PEPPER_FILE_IO
pthread_mutex_t *Mutex::raw_mutex() {
  return AsPthreadMutexT(&opaque_buffer_);
}
#endif  // MOZC_USE_PEPPER_FILE_IO

#ifdef MOZC_PTHREAD_HAS_READER_WRITER_LOCK
// Use pthread native reader writer lock.
namespace {

template <typename T>
pthread_rwlock_t *AsPthreadRWLockT(T* opaque_buffer) {
  COMPILE_ASSERT(sizeof(T) >= sizeof(pthread_rwlock_t),
                 opaque_buffer_size_check);
  return reinterpret_cast<pthread_rwlock_t *>(opaque_buffer);
}

}  // namespace

ReaderWriterMutex::ReaderWriterMutex() {
  pthread_rwlock_init(AsPthreadRWLockT(&opaque_buffer_), NULL);
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

bool ReaderWriterMutex::MultipleReadersThreadsSupported() {
  return true;
}

#else

// Fallback implementations as ReaderWriterLock is not available.
ReaderWriterMutex::ReaderWriterMutex() {
  // Non-recursive lock is OK.
  pthread_mutex_init(AsPthreadMutexT(&opaque_buffer_), NULL);
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

bool ReaderWriterMutex::MultipleReadersThreadsSupported() {
  return false;
}

#endif  // MOZC_PTHREAD_HAS_READER_WRITER_LOCK

#endif  // OS_WIN or pthread

void CallOnce(once_t *once, void (*func)()) {
  if (once == NULL || func == NULL) {
    return;
  }

  if (once->state != ONCE_INIT) {
    return;
  }

  // change the counter in atomic
  if (0 == InterlockedCompareExchange(&(once->counter), 1, 0)) {
    // call func
    (*func)();
    // change the status to be ONCE_DONE in atomic
    // Maybe we won't use it, but in order to make memory barrier,
    // we use InterlockedCompareExchange just in case.
    InterlockedCompareExchange(&(once->state), ONCE_DONE, ONCE_INIT);
  } else {
    while (once->state == ONCE_INIT) {
#ifdef OS_WIN
      ::YieldProcessor();
#endif  // OS_WIN
    }  // busy loop
  }
}

void ResetOnce(once_t *once) {
  InterlockedCompareExchange(&(once->state), ONCE_INIT, ONCE_DONE);
  InterlockedCompareExchange(&(once->counter), 0, 1);
}

}  // namespace mozc

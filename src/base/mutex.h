// Copyright 2010-2014, Google Inc.
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

#ifdef MOZC_USE_PEPPER_FILE_IO
#include <pthread.h>
#endif  // MOZC_USE_PEPPER_FILE_IO

#include "base/port.h"
#include "base/thread_annotations.h"

namespace mozc {

// Usage:
// static mozc::Mutex mutex;
//
// void foo() {
//   mozc::scoped_lock lock(&mutex);
//   ..
// };

// To remove dependencies against plafrom specific headers such as
// <Windows.h> or <pthread.h>, we use an array of pointers as an opaque buffer
// where platform specific mutex structure will be placed.
#if defined(OS_MACOSX)
// Mac requires relatively large buffer for pthread mutex object.
#define MOZC_MUTEX_PTR_ARRAYSIZE 11
#define MOZC_RW_MUTEX_PTR_ARRAYSIZE 32
#else
// Currently following sizes seem to be enough to support all other platforms.
#define MOZC_MUTEX_PTR_ARRAYSIZE 8
#define MOZC_RW_MUTEX_PTR_ARRAYSIZE 12
#endif

class LOCKABLE Mutex {
 public:
  Mutex();
  ~Mutex();
  void Lock() EXCLUSIVE_LOCK_FUNCTION();
  bool TryLock() EXCLUSIVE_TRYLOCK_FUNCTION(true);
  void Unlock() UNLOCK_FUNCTION();

#ifdef MOZC_USE_PEPPER_FILE_IO
  // Returns the pointer to pthread_mutex_t.
  // This method is used for a condition object in a BlockingQueue.
  pthread_mutex_t *raw_mutex();
#endif  // MOZC_USE_PEPPER_FILE_IO

 private:
  void *opaque_buffer_[MOZC_MUTEX_PTR_ARRAYSIZE];

  DISALLOW_COPY_AND_ASSIGN(Mutex);
};

#undef MOZC_MUTEX_PTR_ARRAYSIZE

// IMPORTANT NOTE: Unlike mozc::Mutex, this class does not always support
//     recursive lock.
// TODO(yukawa): Rename this as ReaderWriterNonRecursiveMutex.
class LOCKABLE ReaderWriterMutex {
 public:
  ReaderWriterMutex();
  ~ReaderWriterMutex();

  void ReaderLock() SHARED_LOCK_FUNCTION();
  void WriterLock() EXCLUSIVE_LOCK_FUNCTION();
  void ReaderUnlock() UNLOCK_FUNCTION();
  void WriterUnlock() UNLOCK_FUNCTION();

  // Returns true if multiple reader thread can grant the lock at the same time
  // on the current platform.
  static bool MultipleReadersThreadsSupported();

 private:
  void *opaque_buffer_[MOZC_RW_MUTEX_PTR_ARRAYSIZE];

  DISALLOW_COPY_AND_ASSIGN(ReaderWriterMutex);
};

#undef MOZC_RW_MUTEX_PTR_ARRAYSIZE

// Implementation of this class is left in header for poor compilers where link
// time code generation has not been proven well.
class SCOPED_LOCKABLE scoped_lock {
 public:
  explicit scoped_lock(Mutex *mutex) EXCLUSIVE_LOCK_FUNCTION(mutex)
      : mutex_(mutex) {
    mutex_->Lock();
  }
  ~scoped_lock() UNLOCK_FUNCTION() {
    mutex_->Unlock();
  }

 private:
  Mutex *mutex_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(scoped_lock);
};

// Implementation of this class is left in header for poor compilers where link
// time code generation has not been proven well.
class SCOPED_LOCKABLE scoped_try_lock {
 public:
  explicit scoped_try_lock(Mutex *mutex) EXCLUSIVE_TRYLOCK_FUNCTION(true, mutex)
      : mutex_(mutex) {
    locked_ = mutex_->TryLock();
  }
  ~scoped_try_lock() UNLOCK_FUNCTION() {
    if (locked_) {
      mutex_->Unlock();
    }
  }
  bool locked() const {
    return locked_;
  }

 private:
  Mutex *mutex_;
  bool locked_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(scoped_try_lock);
};

// Implementation of this class is left in header for poor compilers where link
// time code generation has not been proven well.
// TODO(yukawa): Rename this as scoped_nonrecursive_writer_lock.
class SCOPED_LOCKABLE scoped_writer_lock {
 public:
  explicit scoped_writer_lock(ReaderWriterMutex *mutex)
      SHARED_LOCK_FUNCTION(mutex)
      : mutex_(mutex) {
    mutex_->WriterLock();
  }
  ~scoped_writer_lock() UNLOCK_FUNCTION() {
    mutex_->WriterUnlock();
  }

 private:
  ReaderWriterMutex *mutex_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(scoped_writer_lock);
};

// Implementation of this class is left in header for poor compilers where link
// time code generation has not been proven well.
// TODO(yukawa): Rename this as scoped_nonrecursive_reader_lock.
class SCOPED_LOCKABLE scoped_reader_lock {
 public:
  explicit scoped_reader_lock(ReaderWriterMutex *mutex)
      EXCLUSIVE_LOCK_FUNCTION(mutex)
      : mutex_(mutex) {
    mutex_->ReaderLock();
  }
  ~scoped_reader_lock() UNLOCK_FUNCTION() {
    mutex_->ReaderUnlock();
  }

 private:
  ReaderWriterMutex *mutex_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(scoped_reader_lock);
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
#ifdef OS_WIN
  volatile long state;    // NOLINT
  volatile long counter;  // NOLINT
#else
  volatile int state;
  volatile int counter;
#endif
};

// portable re-implementation of pthread_once
void CallOnce(once_t *once, void (*func)());

// reset once_t
void ResetOnce(once_t *once);

}  // namespace mozc

#endif  // MOZC_BASE_MUTEX_H_

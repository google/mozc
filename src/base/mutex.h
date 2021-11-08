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

#ifndef MOZC_BASE_MUTEX_H_
#define MOZC_BASE_MUTEX_H_

#include <atomic>

#include "base/port.h"
#include "absl/base/thread_annotations.h"

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
#if defined(__APPLE__)
// Mac requires relatively large buffer for pthread mutex object.
#define MOZC_MUTEX_PTR_ARRAYSIZE 11
#else  // defined(__APPLE__)
// Currently following sizes seem to be enough to support all other platforms.
#define MOZC_MUTEX_PTR_ARRAYSIZE 8
#endif  // defined(__APPLE__)

class ABSL_LOCKABLE Mutex {
 public:
  Mutex();
  ~Mutex();
  void Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION();
  bool TryLock() ABSL_EXCLUSIVE_TRYLOCK_FUNCTION(true);
  void Unlock() ABSL_UNLOCK_FUNCTION();

 private:
  void *opaque_buffer_[MOZC_MUTEX_PTR_ARRAYSIZE];

  DISALLOW_COPY_AND_ASSIGN(Mutex);
};

#undef MOZC_MUTEX_PTR_ARRAYSIZE

// Implementation of this class is left in header for poor compilers where link
// time code generation has not been proven well.
class ABSL_SCOPED_LOCKABLE scoped_lock {
 public:
  explicit scoped_lock(Mutex *mutex) ABSL_EXCLUSIVE_LOCK_FUNCTION(mutex)
      : mutex_(mutex) {
    mutex_->Lock();
  }
  ~scoped_lock() ABSL_UNLOCK_FUNCTION() { mutex_->Unlock(); }

 private:
  Mutex *mutex_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(scoped_lock);
};

// Implementation of this class is left in header for poor compilers where link
// time code generation has not been proven well.
class ABSL_SCOPED_LOCKABLE scoped_try_lock {
 public:
  explicit scoped_try_lock(Mutex *mutex)
      ABSL_EXCLUSIVE_TRYLOCK_FUNCTION(true, mutex)
      : mutex_(mutex) {
    locked_ = mutex_->TryLock();
  }
  ~scoped_try_lock() ABSL_UNLOCK_FUNCTION() {
    if (locked_) {
      mutex_->Unlock();
    }
  }
  bool locked() const { return locked_; }

 private:
  Mutex *mutex_;
  bool locked_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(scoped_try_lock);
};

using MutexLock = scoped_lock;

using once_t = std::atomic<int>;

#define MOZC_ONCE_INIT ATOMIC_VAR_INIT(0)

// Portable re-implementation of pthread_once.
void CallOnce(once_t *once, void (*func)());

// Resets once_t.
void ResetOnce(once_t *once);

}  // namespace mozc

#endif  // MOZC_BASE_MUTEX_H_

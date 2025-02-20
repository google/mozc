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

#ifndef MOZC_BASE_SINGLETON_H_
#define MOZC_BASE_SINGLETON_H_

#include <atomic>

#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
#include "absl/base/no_destructor.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace mozc {
namespace internal {

// Do not call this method directly. Use `Singleton<T>` instead.
void AddSingletonFinalizer(void (*finalizer)());

}  // namespace internal

// Destructs all singletons created by `Singleton<T>`. The primary usage is to
// call this right before unloading the Mozc UI for Windows DLL to avoid memory
// leaks.
//
// Generally speaking, you SHOULD try to avoid singletons by injecting
// dependencies instead.
//
// NOTE: This is a dangerous operation that can cause use-after-free when
// misused.
void FinalizeSingletons();

// Thread-safe Singleton class.
// Usage:
//
// class Foo {}
//
// Foo *instance = mozc::Singleton<Foo>::get();
template <typename T>
class Singleton {
 public:
  static T *get() ABSL_LOCKS_EXCLUDED(mutex_) {
    {
      absl::ReaderMutexLock lock(&mutex_);  // NOLINT: In the program's steady
                                            // state there's no write lock.
      if (instance_ != nullptr) {
        return instance_;
      }
    }

    absl::MutexLock lock(&mutex_);
    if (instance_ == nullptr) {
      instance_ = new T();
      internal::AddSingletonFinalizer(&Singleton<T>::Delete);
    }
    return instance_;
  }

  // TEST ONLY! Do not call this method in production code.
  static void Delete() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    delete instance_;
    instance_ = nullptr;
  }

 private:
  ABSL_CONST_INIT static inline absl::Mutex mutex_ =
      absl::Mutex(absl::kConstInit);
  ABSL_CONST_INIT static inline T *instance_ ABSL_GUARDED_BY(mutex_) = nullptr;
};

// SingletonMockable class.
// Usage: (quote from clock.cc)
//
//   using ClockSingleton = SingletonMockable<ClockInterface, ClockImpl>;
//
//   uint64_t Clock::GetTime() {
//     return ClockSingleton::Get()->GetTime();
//   }
//
//   void Clock::SetClockForUnitTest(ClockInterface *clock_mock) {
//    ClockSingleton::SetMock(clock_mock);
//   }
template <class Interface, class Impl>
class SingletonMockable {
 public:
  static Interface *Get() {
    if (Interface *mock = mock_.load(std::memory_order_acquire)) {
      return mock;
    }
    static absl::NoDestructor<Impl> impl;
    return impl.get();
  }

  static void SetMock(Interface *mock) {
    mock_.store(mock, std::memory_order_release);
  }

 private:
  ABSL_CONST_INIT static inline std::atomic<Interface *> mock_ = nullptr;
};

}  // namespace mozc

#endif  // MOZC_BASE_SINGLETON_H_

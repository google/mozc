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

#ifndef MOZC_BASE_SINGLETON_H_
#define MOZC_BASE_SINGLETON_H_

#include "base/mutex.h"

namespace mozc {

class SingletonFinalizer {
 public:
  typedef void (*FinalizerFunc)();

  // Do not call this method directly.
  // use Singleton<Typename> instead.
  static void AddFinalizer(FinalizerFunc func);

  // Call Finalize() if you want to finalize
  // all instances created by Sigleton.
  //
  // Mozc UI for Windows (DLL) can call
  // SigletonFinalizer::Finalize()
  // at an appropriate timing.
  static void Finalize();
};

// Thread-safe Singleton class.
// Usage:
//
// class Foo {}
//
// Foo *instance = mozc::Singleton<Foo>::get();
template <typename T>
class Singleton {
 public:
  static T *get() {
    CallOnce(&once_, &Singleton<T>::Init);
    return instance_;
  }

 private:
  static void Init() {
    SingletonFinalizer::AddFinalizer(&Singleton<T>::Delete);
    instance_ = new T;
  }

  static void Delete() {
    delete instance_;
    instance_ = nullptr;
    ResetOnce(&once_);
  }

  static once_t once_;
  static T *instance_;
};

template <typename T>
once_t Singleton<T>::once_ = MOZC_ONCE_INIT;

template <typename T>
T *Singleton<T>::instance_ = nullptr;


// SingletonMockable class.
// Usage: (quote from clock.cc)
//
//   using ClockSingleton = SingletonMockable<ClockInterface, ClockImpl>;
//
//   uint64 Clock::GetTime() {
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
    if (mock_) {
      return mock_;
    }
    static Impl *impl = new Impl();
    return impl;
  }
  static void SetMock(Interface *mock) {
    mock_ = mock;
  }
 private:
  static Interface *mock_;
};

template <class Interface, class Impl>
Interface *SingletonMockable<Interface, Impl>::mock_ = nullptr;

}  // namespace mozc

#endif  // MOZC_BASE_SINGLETON_H_

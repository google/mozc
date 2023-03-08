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

#ifndef MOZC_BASE_THREAD2_H_
#define MOZC_BASE_THREAD2_H_

#include <functional>
#include <memory>
#include <utility>

#include "base/thread.h"
#include "absl/functional/bind_front.h"
#include "absl/synchronization/notification.h"

namespace mozc {

// Represents a thread, exposing a subset of `std::thread` APIs.
//
// Most notabley, threads are undetachable unlike `std::thread`, thus must be
// `join()`ed before destruction. This means that the `mozc::Thread2` instance
// must be retained even for a long-running one, though which may be until
// the end of the process.
//
// The semantics of the present APIs are mostly the same as `std::thread`
// counterpart of the same (but lowercased) name, except that the behavior of
// situations where `std::thread` would throw an exception (e.g. destruction
// before `join()`) is unspecified. It's guaranteed to be either the same
// exception as `std::thread`, silently ignored or process termination, but
// since there ins't really a way to robustly handle all of them you should
// avoid such a situation as if they're UB.
//
// NOTE: This serves as a compatibility layer for Google where we use a
// different threading implementation internally.
//
// TODO(tomokinat): Rename this (and the filename) to `Thread` once the old
// `mozc::Thread` usages are all removed.
class Thread2 {
 public:
  Thread2() noexcept = default;
  template <class Function, class... Args>
  explicit Thread2(Function &&f, Args &&...args);
  ~Thread2() = default;

  Thread2(const Thread2 &) = delete;
  Thread2(Thread2 &&) noexcept = default;

  Thread2 &operator=(const Thread2 &) = delete;
  Thread2 &operator=(Thread2 &&) noexcept = default;

  void Join();

 private:
  // TODO(tomokinat): Change this to `std::thread` (and Google stuff internally)
  // once the entire codebase is migrated from `mozc::Thread`.
  //
  // For now, we use `mozc::Thread` as backend so that `mozc::Thread` usages can
  // be incrementally migrated to `mozc::Thread2`.
  std::unique_ptr<Thread> thread_;
};

// Creates a `mozc::Thread2`, but also accepsts an `absl::Notification` that
// will be notified once `f` returns.
//
// `done` must be a non-null pointer to an `absl::Notification` that is valid
// until notified.
template <class F, class... Args>
Thread2 CreateThreadWithDoneNotification(absl::Notification *done, F &&f,
                                         Args &&...args);

////////////////////////////////////////////////////////////////////////////////
// Implementations
////////////////////////////////////////////////////////////////////////////////

namespace internal {

template <class Function>
class FunctorThread : public Thread {
 public:
  // Callers below should be (decay-)copying things, thus accepts only rvalue to
  // make sure that we don't copy anything here.
  explicit FunctorThread(Function &&f) : f_(std::move(f)) {}

  void Run() override { std::invoke(std::move(f_)); }

 private:
  Function f_;
};

// Workaround for `-Wctad-maybe-unsupported`.
template <class Function>
std::unique_ptr<FunctorThread<Function>> CreateFunctorThread(Function &&f) {
  return std::make_unique<FunctorThread<Function>>(std::forward<Function>(f));
}

}  // namespace internal

template <class Function, class... Args>
Thread2::Thread2(Function &&f, Args &&...args)
    // ref: https://en.cppreference.com/w/cpp/thread/thread/thread
    // Note that `absl::bind_front` handles decay-copying things for us.
    : thread_(internal::CreateFunctorThread(absl::bind_front(
          std::forward<Function>(f), std::forward<Args>(args)...))) {
  thread_->Start("mozc::Thread2");
}

template <class Function, class... Args>
Thread2 CreateThreadWithDoneNotification(absl::Notification *done, Function &&f,
                                         Args &&...args) {
  return Thread2([done, f = absl::bind_front(std::forward<Function>(f),
                                             std::forward<Args>(args)...)]() {
    std::invoke(std::move(f));
    done->Notify();
  });
}

}  // namespace mozc

#endif  // MOZC_BASE_THREAD2_H_

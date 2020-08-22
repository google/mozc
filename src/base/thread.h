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

#ifndef MOZC_BASE_THREAD_H_
#define MOZC_BASE_THREAD_H_

#include <memory>
#include <string>

#include "base/port.h"

namespace mozc {

struct ThreadInternalState;

class Thread {
 public:
  Thread();
  virtual ~Thread();

  virtual void Run() = 0;

  void Start(const std::string &thread_name);
  bool IsRunning() const;
  void SetJoinable(bool joinable);
  void Join();
  // This method is not encouraged especiialy in Windows.
  void Terminate();

 private:
  void Detach();

#ifndef OS_WIN
  static void *WrapperForPOSIX(void *ptr);
#endif  // OS_WIN

  std::unique_ptr<ThreadInternalState> state_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

class ThreadJoiner final {
 public:
  explicit ThreadJoiner(Thread *thread)
      : joiner_(thread, &ThreadJoiner::JoinThread) {}

  // Non-copyable
  ThreadJoiner(const ThreadJoiner &) = delete;
  ThreadJoiner &operator=(const ThreadJoiner &) = delete;

  // Movable
  ThreadJoiner(ThreadJoiner &&) = default;
  ThreadJoiner &operator=(ThreadJoiner &&) = default;

  ~ThreadJoiner() = default;

  static void JoinThread(Thread *thread) {
    if (thread) {
      thread->Join();
    }
  }

 private:
  std::unique_ptr<Thread, void (*)(Thread *)> joiner_;
};

}  // namespace mozc

#endif  // MOZC_BASE_THREAD_H_

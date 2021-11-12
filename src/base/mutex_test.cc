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

#include "base/mutex.h"

#include <atomic>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/thread.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

std::atomic<int> g_num_called;

void IncrementGNumCalled() {
  ++g_num_called;
  Util::Sleep(20);
}

class OnceIncrementGNumCalledThread : public Thread {
 public:
  explicit OnceIncrementGNumCalledThread(once_t *once) : once_(once) {}
  ~OnceIncrementGNumCalledThread() override = default;

  void Run() override {
    for (int i = 0; i < 10; ++i) {
      CallOnce(once_, &IncrementGNumCalled);
      Util::Sleep(5);
    }
  }

 private:
  once_t *once_;
};

TEST(CallOnceTest, SingleThread) {
  g_num_called = 0;
  once_t once = MOZC_ONCE_INIT;
  for (int i = 0; i < 3; ++i) {
    CallOnce(&once, &IncrementGNumCalled);
    EXPECT_EQ(1, g_num_called);
  }
}

TEST(CallOnceTest, MultiThread) {
  g_num_called = 0;
  once_t once = MOZC_ONCE_INIT;
  std::vector<std::unique_ptr<OnceIncrementGNumCalledThread>> threads;
  std::vector<ThreadJoiner> joiners;
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back(new OnceIncrementGNumCalledThread(&once));
    joiners.emplace_back(threads.back().get());
  }
  for (auto &thread : threads) {
    thread->Start("Increment");
  }
  for (int i = 0; i < 3; ++i) {
    CallOnce(&once, &IncrementGNumCalled);
  }
  EXPECT_EQ(1, g_num_called);
}

}  // namespace
}  // namespace mozc

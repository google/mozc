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

#include "base/task_token.h"

#include <set>
#include <vector>

#include "base/base.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/thread.h"
#include "testing/base/public/gunit.h"

using mozc::TaskToken;
using mozc::ThreadSafeTaskTokenManager;

TEST(TaskToken, TaskTokenTest) {
  EXPECT_TRUE(TaskToken(0) == TaskToken(0));
  EXPECT_FALSE(TaskToken(0) == TaskToken(1));
  EXPECT_FALSE(TaskToken(0) == TaskToken(2));
  EXPECT_FALSE(TaskToken(1) == TaskToken(0));
  EXPECT_TRUE(TaskToken(1) == TaskToken(1));
  EXPECT_FALSE(TaskToken(1) == TaskToken(2));
  EXPECT_FALSE(TaskToken(2) == TaskToken(0));
  EXPECT_FALSE(TaskToken(2) == TaskToken(1));
  EXPECT_TRUE(TaskToken(2) == TaskToken(2));

  EXPECT_FALSE(TaskToken(0) != TaskToken(0));
  EXPECT_TRUE(TaskToken(0) != TaskToken(1));
  EXPECT_TRUE(TaskToken(0) != TaskToken(2));
  EXPECT_TRUE(TaskToken(1) != TaskToken(0));
  EXPECT_FALSE(TaskToken(1) != TaskToken(1));
  EXPECT_TRUE(TaskToken(1) != TaskToken(2));
  EXPECT_TRUE(TaskToken(2) != TaskToken(0));
  EXPECT_TRUE(TaskToken(2) != TaskToken(1));
  EXPECT_FALSE(TaskToken(2) != TaskToken(2));

  EXPECT_FALSE(TaskToken(0) > TaskToken(0));
  EXPECT_FALSE(TaskToken(0) > TaskToken(1));
  EXPECT_FALSE(TaskToken(0) > TaskToken(2));
  EXPECT_TRUE(TaskToken(1) > TaskToken(0));
  EXPECT_FALSE(TaskToken(1) > TaskToken(1));
  EXPECT_FALSE(TaskToken(1) > TaskToken(2));
  EXPECT_TRUE(TaskToken(2) > TaskToken(0));
  EXPECT_TRUE(TaskToken(2) > TaskToken(1));
  EXPECT_FALSE(TaskToken(2) > TaskToken(2));

  EXPECT_TRUE(TaskToken(0) >= TaskToken(0));
  EXPECT_FALSE(TaskToken(0) >= TaskToken(1));
  EXPECT_FALSE(TaskToken(0) >= TaskToken(2));
  EXPECT_TRUE(TaskToken(1) >= TaskToken(0));
  EXPECT_TRUE(TaskToken(1) >= TaskToken(1));
  EXPECT_FALSE(TaskToken(1) >= TaskToken(2));
  EXPECT_TRUE(TaskToken(2) >= TaskToken(0));
  EXPECT_TRUE(TaskToken(2) >= TaskToken(1));
  EXPECT_TRUE(TaskToken(2) >= TaskToken(2));

  EXPECT_FALSE(TaskToken(0) < TaskToken(0));
  EXPECT_TRUE(TaskToken(0) < TaskToken(1));
  EXPECT_TRUE(TaskToken(0) < TaskToken(2));
  EXPECT_FALSE(TaskToken(1) < TaskToken(0));
  EXPECT_FALSE(TaskToken(1) < TaskToken(1));
  EXPECT_TRUE(TaskToken(1) < TaskToken(2));
  EXPECT_FALSE(TaskToken(2) < TaskToken(0));
  EXPECT_FALSE(TaskToken(2) < TaskToken(1));
  EXPECT_FALSE(TaskToken(2) < TaskToken(2));

  EXPECT_TRUE(TaskToken(0) <= TaskToken(0));
  EXPECT_TRUE(TaskToken(0) <= TaskToken(1));
  EXPECT_TRUE(TaskToken(0) <= TaskToken(2));
  EXPECT_FALSE(TaskToken(1) <= TaskToken(0));
  EXPECT_TRUE(TaskToken(1) <= TaskToken(1));
  EXPECT_TRUE(TaskToken(1) <= TaskToken(2));
  EXPECT_FALSE(TaskToken(2) <= TaskToken(0));
  EXPECT_FALSE(TaskToken(2) <= TaskToken(1));
  EXPECT_TRUE(TaskToken(2) <= TaskToken(2));

  EXPECT_FALSE(TaskToken(0).isValid());
  EXPECT_TRUE(TaskToken(1).isValid());
  EXPECT_TRUE(TaskToken(2).isValid());
}

TEST(ThreadSafeTaskTokenManager, SimpleTest) {
  ThreadSafeTaskTokenManager token_manager;

  const TaskToken token1 = token_manager.NewToken();
  const TaskToken token2 = token_manager.NewToken();
  const TaskToken token3 = token_manager.NewToken();
  EXPECT_TRUE(token1.isValid());
  EXPECT_TRUE(token2.isValid());
  EXPECT_TRUE(token3.isValid());
  EXPECT_TRUE(token1 != token2);
  EXPECT_TRUE(token2 != token3);
  EXPECT_TRUE(token1 != token3);
}

TEST(ThreadSafeTaskTokenManager, LargeTest) {
  ThreadSafeTaskTokenManager token_manager;
  const int kNumTokens = 100000;
  set<TaskToken> token_set;

  for (size_t i = 0; i < kNumTokens; ++i) {
    token_set.insert(token_manager.NewToken());
  }
  EXPECT_EQ(kNumTokens, token_set.size());
}

namespace {
static const int kNumThreads = 10;
static const int kNumTokensPerThread = 10000;

class TokenConsumer : public mozc::Thread {
 public:
  explicit TokenConsumer(ThreadSafeTaskTokenManager *token_manager)
      : token_manager_(token_manager) {}
  void Run() {
    CHECK(token_manager_);
    for (size_t i = 0; i < kNumTokensPerThread; ++i) {
      token_set_.insert(token_manager_->NewToken());
    }
  }
  const set<TaskToken> &token_set() {
    return token_set_;
  }

 private:
  ThreadSafeTaskTokenManager *token_manager_;
  set<TaskToken> token_set_;
  DISALLOW_COPY_AND_ASSIGN(TokenConsumer);
};

}  // namespace

TEST(ThreadSafeTaskTokenManager, MultiThreadTest) {
  ThreadSafeTaskTokenManager token_manager;
  vector<TokenConsumer*> consumers(kNumThreads);
  for (size_t i = 0; i < kNumThreads; ++i) {
    consumers[i] = new TokenConsumer(&token_manager);
  }
  for (size_t i = 0; i < kNumThreads; ++i) {
    consumers[i]->Start();
  }
  for (size_t i = 0; i < kNumThreads; ++i) {
    consumers[i]->Join();
  }

  set<TaskToken> token_set;
  for (size_t i = 0; i < kNumThreads; ++i) {
    for (set<TaskToken>::const_iterator it = consumers[i]->token_set().begin();
         it != consumers[i]->token_set().end(); ++it) {
      token_set.insert(*it);
    }
  }
  EXPECT_EQ(kNumTokensPerThread * kNumThreads, token_set.size());
  mozc::STLDeleteElements(&consumers);
}

// Copyright 2010-2011, Google Inc.
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

// Test of handwriting module manager

#include "handwriting/handwriting_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace handwriting {
namespace {
class MockHandwriting : public HandwritingInterface {
 public:
  MockHandwriting()
      : commit_counter_(0) {
  }

  virtual void Recognize(const Strokes &unused_strokes,
                         vector<string> *candidates) const {
    CHECK(candidates);
    candidates->clear();
    for (size_t i = 0; i < candidates_.size(); ++i) {
      candidates->push_back(candidates_[i]);
    }
  }

  virtual void Commit(const Strokes &unused_strokes,
                      const string &unused_result) {
    ++commit_counter_;
  }

  void SetCandidates(const vector<string> &candidates) {
    candidates_.clear();
    for (size_t i = 0; i < candidates.size(); ++i) {
      candidates_.push_back(candidates[i]);
    }
  }

  int GetCommitCounter() {
    return commit_counter_;
  }

  void ClearCommitCounter() {
    commit_counter_ = 0;
  }
 private:
  vector<string> candidates_;
  int commit_counter_;
};
}  // anonymous namespace

class HandwritingManagerTest : public testing::Test {
 public:
  virtual void SetUp() {
    HandwritingManager::AddHandwritingModule(&mock_handwriting_);
  }

  virtual void TearDown() {
    HandwritingManager::ClearHandwritingModules();
  }

 protected:
  MockHandwriting mock_handwriting_;
};

TEST_F(HandwritingManagerTest, Recognize) {
  vector<string> expected_candidates;
  expected_candidates.push_back("foo");
  expected_candidates.push_back("bar");
  expected_candidates.push_back("baz");
  mock_handwriting_.SetCandidates(expected_candidates);

  vector<string> result;
  Strokes dummy_strokes;
  HandwritingManager::Recognize(dummy_strokes, &result);
  EXPECT_EQ(expected_candidates.size(), result.size());
  for (size_t i = 0; i < expected_candidates.size(); ++i) {
    EXPECT_EQ(expected_candidates[i], result[i])
        << i << "-th candidate mismatch";
  }
}

TEST_F(HandwritingManagerTest, Commit) {
  mock_handwriting_.ClearCommitCounter();
  EXPECT_EQ(0, mock_handwriting_.GetCommitCounter());

  Strokes dummy_strokes;
  string dummy_result;
  HandwritingManager::Commit(dummy_strokes, dummy_result);
  EXPECT_EQ(1, mock_handwriting_.GetCommitCounter());
}
}  // namespace handwriting
}  // namespace mozc

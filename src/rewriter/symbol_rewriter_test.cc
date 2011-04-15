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

#include <string>

#include "base/util.h"
#include "converter/segments.h"
#include "rewriter/symbol_rewriter.h"
#include "testing/base/public/gunit.h"
#include "session/config_handler.h"
#include "session/config.pb.h"

DECLARE_string(test_tmpdir);

namespace mozc {

namespace {
void AddSegment(const string &key, const string &value,
                Segments *segments) {
  Segment *seg = segments->push_back_segment();
  seg->set_key(key);
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = key;
  candidate->content_key = key;
  candidate->content_value = value;
}

void AddCandidate(const string &value, Segment *segment) {
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
  candidate->value = value;
  candidate->content_key = segment->key();
  candidate->content_value = value;
}

bool HasCandidate(const Segments &segments, int index, const string &value) {
  CHECK_GT(segments.segments_size(), index);
  for (size_t i = 0; i < segments.segment(index).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(index).candidate(i);
    if (candidate.value == value) {
      return true;
    }
  }
  return false;
}
}  // namespace

class SymbolRewriterTest : public testing::Test {
 protected:
  SymbolRewriterTest() {}
  ~SymbolRewriterTest() {}

  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    // Just in case, reset the config in test_tmpdir
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }
};

// Note that these tests are using default symbol dictionary.
// Test result can be changed if symbol dictionary is modified.
// TODO(toshiyuki): Modify symbol rewriter so that we can use symbol dictionary
// for testing.
TEST_F(SymbolRewriterTest, TriggerRewriteTest) {
  SymbolRewriter symbol_rewriter;
  {
    Segments segments;
    // "ー"
    AddSegment("\xe3\x83\xbc", "test", &segments);
    // ">"
    AddSegment("\x3e", "test", &segments);
    EXPECT_TRUE(symbol_rewriter.Rewrite(&segments));
    // "→"
    EXPECT_TRUE(HasCandidate(segments, 0, "\xe2\x86\x92"));
  }
  {
    Segments segments;
    // "ー"
    AddSegment("\xe3\x83\xbc", "test", &segments);
    // "ー"
    AddSegment("\xe3\x83\xbc", "test", &segments);
    EXPECT_TRUE(symbol_rewriter.Rewrite(&segments));
    // "―"
    EXPECT_TRUE(HasCandidate(segments, 0, "\xe2\x80\x95"));
    // "―"
    EXPECT_TRUE(HasCandidate(segments, 1, "\xe2\x80\x95"));
  }
}

TEST_F(SymbolRewriterTest, TriggerRewriteEntireTest) {
  SymbolRewriter symbol_rewriter;
  {
    Segments segments;
    // "ー"
    AddSegment("\xe3\x83\xbc", "test", &segments);
    // ">"
    AddSegment("\x3e", "test", &segments);
    EXPECT_TRUE(symbol_rewriter.RewriteEntireCandidate(&segments));
    // "→"
    EXPECT_TRUE(HasCandidate(segments, 0, "\xe2\x86\x92"));
  }
  {
    Segments segments;
    // "ー"
    AddSegment("\xe3\x83\xbc", "test", &segments);
    // "ー"
    AddSegment("\xe3\x83\xbc", "test", &segments);
    EXPECT_FALSE(symbol_rewriter.RewriteEntireCandidate(&segments));
  }
}

TEST_F(SymbolRewriterTest, TriggerRewriteEachTest) {
  SymbolRewriter symbol_rewriter;
  {
    Segments segments;
    // "ー"
    AddSegment("\xe3\x83\xbc", "test", &segments);
    // ">"
    AddSegment("\x3e", "test", &segments);
    EXPECT_TRUE(symbol_rewriter.RewriteEachCandidate(&segments));
    EXPECT_EQ(2, segments.segments_size());
    // "―"
    EXPECT_TRUE(HasCandidate(segments, 0, "\xe2\x80\x95"));
    // "→"
    EXPECT_FALSE(HasCandidate(segments, 0, "\xe2\x86\x92"));
    // "〉"
    EXPECT_TRUE(HasCandidate(segments, 1, "\xe3\x80\x89"));
  }
}

TEST_F(SymbolRewriterTest, InsertAfterSingleKanjiAndT13n) {
  SymbolRewriter symbol_rewriter;
  {
    Segments segments;
    // "てん", "てん"
    AddSegment("\xe3\x81\xa6\xe3\x82\x93", "\xe3\x81\xa6\xe3\x82\x93",
               &segments);
    Segment *seg = segments.mutable_segment(0);
    // Add 15 single-kanji and transliterated candidates
    // "点"
    AddCandidate("\xe7\x82\xb9", seg);
    // "転"
    AddCandidate("\xe8\xbb\xa2", seg);
    // "天"
    AddCandidate("\xe5\xa4\xa9", seg);
    // "てん"
    AddCandidate("\xe3\x81\xa6\xe3\x82\x93", seg);
    // "テン"
    AddCandidate("\xe3\x83\x86\xe3\x83\xb3", seg);
    // "展"
    AddCandidate("\xe5\xb1\x95", seg);
    // "店"
    AddCandidate("\xe5\xba\x97", seg);
    // "典"
    AddCandidate("\xe5\x85\xb8", seg);
    // "添"
    AddCandidate("\xe6\xb7\xbb", seg);
    // "填"
    AddCandidate("\xe5\xa1\xab", seg);
    // "顛"
    AddCandidate("\xe9\xa1\x9b", seg);
    // "辿"
    AddCandidate("\xe8\xbe\xbf", seg);
    // "纏"
    AddCandidate("\xe7\xba\x8f", seg);
    // "甜"
    AddCandidate("\xe7\x94\x9c", seg);
    // "貼"
    AddCandidate("\xe8\xb2\xbc", seg);

    EXPECT_TRUE(symbol_rewriter.Rewrite(&segments));
    EXPECT_GT(segments.segment(0).candidates_size(), 16);
    for (int i = 0; i < 16; ++i) {
      const string &value = segments.segment(0).candidate(i).value;
      EXPECT_FALSE(Util::IsScriptType(value, Util::UNKNOWN_SCRIPT))
          << i << ": " << value;
    }
  }
}
}  // namespace mozc

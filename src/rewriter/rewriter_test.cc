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

#include "rewriter/rewriter.h"

#include <cstddef>
#include <memory>
#include <string>

#include "absl/log/check.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/modules.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

size_t CommandCandidatesSize(const Segment& segment) {
  size_t result = 0;
  for (int i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).attributes &
        converter::Attribute::COMMAND_CANDIDATE) {
      result++;
    }
  }
  return result;
}

}  // namespace

class RewriterTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    modules_ =
        engine::Modules::Create(std::make_unique<testing::MockDataManager>())
            .value();
    rewriter_ = std::make_unique<Rewriter>(*modules_);
  }

  const RewriterInterface* GetRewriter() const { return rewriter_.get(); }

  std::unique_ptr<const engine::Modules> modules_;
  std::unique_ptr<Rewriter> rewriter_;
};

// Command rewriter should be disabled on Android build. b/5851240
TEST_F(RewriterTest, CommandRewriterAvailability) {
  Segments segments;
  const ConversionRequest request;
  Segment* seg = segments.push_back_segment();

  {
    converter::Candidate* candidate = seg->add_candidate();
    seg->set_key("こまんど");
    candidate->value = "コマンド";
    EXPECT_TRUE(GetRewriter()->Rewrite(request, &segments));
#ifdef __ANDROID__
    EXPECT_EQ(CommandCandidatesSize(*seg), 0);
#else  // __ANDROID__
    EXPECT_EQ(CommandCandidatesSize(*seg), 2);
#endif  // __ANDROID__
    seg->clear_candidates();
  }

  {
    converter::Candidate* candidate = seg->add_candidate();
    seg->set_key("さじぇすと");
    candidate->value = "サジェスト";
    EXPECT_TRUE(GetRewriter()->Rewrite(request, &segments));
#ifdef __ANDROID__
    EXPECT_EQ(CommandCandidatesSize(*seg), 0);
#else  // __ANDROID__
    EXPECT_EQ(CommandCandidatesSize(*seg), 1);
#endif  // __ANDROID__
    seg->clear_candidates();
  }
}

TEST_F(RewriterTest, EmoticonsAboveSymbols) {
  constexpr char kKey[] = "かおもじ";
  constexpr char kEmoticon[] = "^^;";
  constexpr char kSymbol[] = "☹";  // A platform-dependent symbol

  const ConversionRequest request;
  Segments segments;
  Segment* seg = segments.push_back_segment();
  converter::Candidate* candidate = seg->add_candidate();
  seg->set_key(kKey);
  candidate->value = kKey;
  EXPECT_EQ(seg->candidates_size(), 1);
  EXPECT_TRUE(GetRewriter()->Rewrite(request, &segments));
  EXPECT_LT(1, seg->candidates_size());

  int emoticon_index = -1;
  int symbol_index = -1;
  for (size_t i = 0; i < seg->candidates_size(); ++i) {
    if (seg->candidate(i).value == kEmoticon) {
      emoticon_index = i;
    } else if (seg->candidate(i).value == kSymbol) {
      symbol_index = i;
    }
  }
  EXPECT_NE(emoticon_index, -1);
  EXPECT_NE(symbol_index, -1);
  EXPECT_LT(emoticon_index, symbol_index);
}

}  // namespace mozc

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
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/flags/reflection.h"
#include "absl/log/check.h"
#include "config/character_form_manager.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/inner_segment.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/modules.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

ABSL_DECLARE_FLAG(bool, use_history_rewriter);

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
    auto modules =
        engine::Modules::Create(std::make_unique<testing::MockDataManager>());
    CHECK_OK(modules);
    modules_ = *std::move(modules);
    rewriter_ = std::make_unique<Rewriter>(*modules_);
  }

  void TearDown() override {
    auto* manager = config::CharacterFormManager::GetCharacterFormManager();
    manager->SetDefaultRule();
    manager->ClearHistory();
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

TEST_F(RewriterTest, UserSegmentHistoryAndVariantsRewriterIntegration) {
  const absl::FlagSaver flag_saver;
  absl::SetFlag(&FLAGS_use_history_rewriter, true);
  rewriter_ = std::make_unique<Rewriter>(*modules_);

  auto* manager = config::CharacterFormManager::GetCharacterFormManager();
  manager->SetDefaultRule();
  manager->ClearHistory();
  manager->AddConversionRule("0", config::Config::FULL_WIDTH);

  const ConversionRequest request;

  Segments segments;
  Segment* seg = segments.push_back_segment();
  seg->set_key("ふつか");

  // Step 1: Learn "2日" (half-width '2') for "ふつか".
  {
    converter::Candidate* c0 = seg->add_candidate();
    c0->key = "ふつか";
    c0->content_key = "ふつか";
    c0->value = "二日";
    c0->content_value = "二日";

    converter::Candidate* c1 = seg->add_candidate();
    c1->key = "ふつか";
    c1->content_key = "ふつか";
    c1->value = "2日";
    c1->content_value = "2日";

    seg->move_candidate(1, 0);
    seg->mutable_candidate(0)->attributes |= converter::Attribute::RERANKED;
    seg->set_segment_type(Segment::FIXED_VALUE);

    rewriter_->Finish(request, segments);
  }

  // Step 2: Rewrite "ふつか".
  // USHR promotes "2日", and VariantsRewriter immediately normalizes it to
  // "２日" according to user preference, updates inner_segment_boundary, and
  // expands alternatives.
  {
    segments.Clear();
    seg = segments.push_back_segment();
    seg->set_key("ふつか");

    converter::Candidate* c0 = seg->add_candidate();
    c0->key = "ふつか";
    c0->content_key = "ふつか";
    c0->value = "二日";
    c0->content_value = "二日";

    converter::Candidate* c1 = seg->add_candidate();
    c1->key = "ふつか";
    c1->content_key = "ふつか";
    c1->value = "2日";
    c1->content_value = "2日";
    c1->inner_segment_boundary = converter::BuildInnerSegmentBoundary(
        {{strlen("ふつか"), strlen("2日"), strlen("ふつか"), strlen("2日")}},
        c1->key, c1->value);

    EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
    ASSERT_GT(seg->candidates_size(), 0);
    const converter::Candidate& top = seg->candidate(0);
    EXPECT_EQ(top.value, "２日");
    EXPECT_EQ(top.content_value, "２日");
    EXPECT_EQ(top.inner_segment_boundary,
              converter::BuildInnerSegmentBoundary(
                  {{strlen("ふつか"), strlen("２日"), strlen("ふつか"),
                    strlen("２日")}},
                  "ふつか", "２日"));
  }
}

TEST_F(RewriterTest, ZipcodeCandidatesDescription) {
  const ConversionRequest request;
  Segments segments;
  Segment* seg = segments.push_back_segment();
  seg->set_key("107-0052");
  converter::Candidate* candidate = seg->add_candidate();
  candidate->key = "107-0052";
  candidate->content_key = "107-0052";
  candidate->value = "東京都港区赤坂";
  candidate->content_value = "東京都港区赤坂";
  candidate->lid = modules_->GetPosMatcher().GetZipcodeId();
  candidate->rid = modules_->GetPosMatcher().GetZipcodeId();

  EXPECT_TRUE(GetRewriter()->Rewrite(request, &segments));
  bool found_zipcode_candidate = false;
  for (size_t i = 0; i < seg->candidates_size(); ++i) {
    if (seg->candidate(i).description == "郵便番号と住所") {
      found_zipcode_candidate = true;
      break;
    }
  }
  EXPECT_TRUE(found_zipcode_candidate);
}

TEST_F(RewriterTest, PreservesNoVariantsExpansionThroughPipeline) {
  const absl::FlagSaver flag_saver;
  absl::SetFlag(&FLAGS_use_history_rewriter, true);
  rewriter_ = std::make_unique<Rewriter>(*modules_);

  auto* manager = config::CharacterFormManager::GetCharacterFormManager();
  manager->SetDefaultRule();
  manager->ClearHistory();
  // Set default rule for alphabet to FULL_WIDTH.
  manager->AddConversionRule("A", config::Config::FULL_WIDTH);

  const ConversionRequest request;
  Segments segments;
  Segment* seg = segments.push_back_segment();
  seg->set_key("ぐーぐる");

  // Step 1: Learn "Google" with NO_VARIANTS_EXPANSION.
  {
    converter::Candidate* c0 = seg->add_candidate();
    c0->key = "ぐーぐる";
    c0->content_key = "ぐーぐる";
    c0->value = "グーグル";
    c0->content_value = "グーグル";

    converter::Candidate* c1 = seg->add_candidate();
    c1->key = "ぐーぐる";
    c1->content_key = "ぐーぐる";
    c1->value = "Google";
    c1->content_value = "Google";
    c1->attributes |= converter::Attribute::NO_VARIANTS_EXPANSION;

    seg->move_candidate(1, 0);
    seg->mutable_candidate(0)->attributes |= converter::Attribute::RERANKED;
    seg->set_segment_type(Segment::FIXED_VALUE);

    rewriter_->Finish(request, segments);
  }

  // Step 2: Rewrite "ぐーぐる".
  // Even though alphabet preference is FULL_WIDTH, promoted "Google" must
  // retain NO_VARIANTS_EXPANSION and NOT be converted to full-width
  // "Ｇｏｏｇｌｅ".
  {
    segments.Clear();
    seg = segments.push_back_segment();
    seg->set_key("ぐーぐる");

    converter::Candidate* c0 = seg->add_candidate();
    c0->key = "ぐーぐる";
    c0->content_key = "ぐーぐる";
    c0->value = "グーグル";
    c0->content_value = "グーグル";

    converter::Candidate* c1 = seg->add_candidate();
    c1->key = "ぐーぐる";
    c1->content_key = "ぐーぐる";
    c1->value = "Google";
    c1->content_value = "Google";
    c1->attributes |= converter::Attribute::NO_VARIANTS_EXPANSION;

    EXPECT_TRUE(rewriter_->Rewrite(request, &segments));
    ASSERT_GT(seg->candidates_size(), 0);
    const converter::Candidate& top = seg->candidate(0);
    EXPECT_EQ(top.value, "Google");
    EXPECT_TRUE(top.attributes & converter::Attribute::NO_VARIANTS_EXPANSION);
  }
}

}  // namespace mozc

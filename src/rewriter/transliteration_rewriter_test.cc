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

#include "rewriter/transliteration_rewriter.h"

#include <cctype>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/strings/str_format.h"

namespace mozc {
namespace {

void InsertASCIISequence(const std::string &text,
                         composer::Composer *composer) {
  for (size_t i = 0; i < text.size(); ++i) {
    commands::KeyEvent key;
    key.set_key_code(text[i]);
    composer->InsertCharacterKeyEvent(key);
  }
}

void SetAkann(composer::Composer *composer) {
  InsertASCIISequence("akann", composer);
  std::string query;
  composer->GetQueryForConversion(&query);
  EXPECT_EQ("あかん", query);
}

}  // namespace

class TransliterationRewriterTest : public ::testing::Test {
 protected:
  // Workaround for C2512 error (no default appropriate constructor) on MSVS.
  TransliterationRewriterTest() {}
  ~TransliterationRewriterTest() override {}

  void SetUp() override {
    usage_stats::UsageStats::ClearAllStatsForTest();
    config::ConfigHandler::GetDefaultConfig(&default_config_);
  }

  void TearDown() override { usage_stats::UsageStats::ClearAllStatsForTest(); }

  TransliterationRewriter *CreateTransliterationRewriter() const {
    return new TransliterationRewriter(
        dictionary::PosMatcher(mock_data_manager_.GetPosMatcherData()));
  }

  const commands::Request &default_request() const { return default_request_; }

  const config::Config &default_config() const { return default_config_; }

  usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;

  const testing::MockDataManager mock_data_manager_;

 private:
  const testing::ScopedTmpUserProfileDirectory tmp_profile_dir_;
  const commands::Request default_request_;
  config::Config default_config_;
};

TEST_F(TransliterationRewriterTest, T13nFromKeyTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());
  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("あかん");
  EXPECT_EQ(0, segment->meta_candidates_size());
  const ConversionRequest default_request;
  EXPECT_TRUE(t13n_rewriter->Rewrite(default_request, &segments));
  {
    EXPECT_EQ("あかん",
              segment->meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("アカン",
              segment->meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("akan",
              segment->meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("AKAN",
              segment->meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("akan",
              segment->meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "Akan",
        segment->meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ａｋａｎ",
              segment->meta_candidate(transliteration::FULL_ASCII).value);
    EXPECT_EQ("ＡＫＡＮ",
              segment->meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    EXPECT_EQ("ａｋａｎ",
              segment->meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    EXPECT_EQ(
        "Ａｋａｎ",
        segment->meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ｱｶﾝ",
              segment->meta_candidate(transliteration::HALF_KATAKANA).value);
    for (size_t i = 0; i < segment->meta_candidates_size(); ++i) {
      EXPECT_NE(0, segment->meta_candidate(i).lid);
      EXPECT_NE(0, segment->meta_candidate(i).rid);
      EXPECT_FALSE(segment->meta_candidate(i).key.empty());
    }
  }
}

TEST_F(TransliterationRewriterTest, T13nFromComposerTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.InitializeWithRequestAndConfig(default_request(), default_config(),
                                       mock_data_manager_);
  composer::Composer composer(&table, &default_request(), &default_config());
  SetAkann(&composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  ConversionRequest request(&composer, &default_request(), &default_config());
  segment->set_key("あかん");
  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  {
    EXPECT_EQ(1, segments.conversion_segments_size());
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ("あかん", seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("アカン",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("akann", seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("AKANN",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("akann",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "Akann",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ａｋａｎｎ",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    EXPECT_EQ("ＡＫＡＮＮ",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    EXPECT_EQ("ａｋａｎｎ",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    EXPECT_EQ(
        "Ａｋａｎｎ",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ｱｶﾝ", seg.meta_candidate(transliteration::HALF_KATAKANA).value);
    for (size_t i = 0; i < segment->meta_candidates_size(); ++i) {
      EXPECT_NE(0, segment->meta_candidate(i).lid);
      EXPECT_NE(0, segment->meta_candidate(i).rid);
      EXPECT_FALSE(segment->meta_candidate(i).key.empty());
    }
  }
}

TEST_F(TransliterationRewriterTest, KeyOfT13nFromComposerTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.InitializeWithRequestAndConfig(default_request(), default_config(),
                                       mock_data_manager_);
  composer::Composer composer(&table, &default_request(), &default_config());
  InsertASCIISequence("ssh", &composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  commands::Request input;
  input.set_mixed_conversion(true);
  ConversionRequest request(&composer, &input, &default_config());
  request.set_request_type(ConversionRequest::SUGGESTION);
  {
    // Although the segment key is "っ" as a partical string of the full
    // composition, the transliteration key should be "っsh" as the
    // whole composition string.

    segment->set_key("っ");
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    EXPECT_EQ(1, segments.conversion_segments_size());
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ("っｓｈ", seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("っsh", seg.meta_candidate(transliteration::HIRAGANA).key);
    EXPECT_EQ("ssh", seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("っsh", seg.meta_candidate(transliteration::HALF_ASCII).key);
  }
}

TEST_F(TransliterationRewriterTest, T13nWithMultiSegmentsTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.InitializeWithRequestAndConfig(default_request(), default_config(),
                                       mock_data_manager_);
  composer::Composer composer(&table, &default_request(), &default_config());

  // Set kamabokoinbou to composer.
  {
    InsertASCIISequence("kamabokonoinbou", &composer);
    std::string query;
    composer.GetQueryForConversion(&query);
    EXPECT_EQ("かまぼこのいんぼう", query);
  }

  Segments segments;
  ConversionRequest request(&composer, &default_request(), &default_config());
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
    segment->set_key("かまぼこの");
    segment = segments.add_segment();
    CHECK(segment);
    segment->set_key("いんぼう");
  }

  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  EXPECT_EQ(2, segments.conversion_segments_size());
  {
    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ("かまぼこの",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("kamabokono",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
  }
  {
    const Segment &seg = segments.conversion_segment(1);
    EXPECT_EQ("いんぼう", seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("inbou", seg.meta_candidate(transliteration::HALF_ASCII).value);
  }
}

TEST_F(TransliterationRewriterTest, ComposerValidationTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.InitializeWithRequestAndConfig(default_request(), default_config(),
                                       mock_data_manager_);
  composer::Composer composer(&table, &default_request(), &default_config());

  // Set kan to composer.
  {
    InsertASCIISequence("kan", &composer);
    std::string query;
    composer.GetQueryForConversion(&query);
    EXPECT_EQ("かん", query);
  }

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  ConversionRequest request(&composer, &default_request(), &default_config());
  segment->set_key("かん");
  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  // Should not use composer
  {
    EXPECT_EQ(1, segments.conversion_segments_size());
    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ("かん", seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("カン", seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("kan", seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("KAN",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("kan",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "Kan",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ｋａｎ", seg.meta_candidate(transliteration::FULL_ASCII).value);
    EXPECT_EQ("ＫＡＮ",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    EXPECT_EQ("ｋａｎ",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    EXPECT_EQ(
        "Ｋａｎ",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ｶﾝ", seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
}

TEST_F(TransliterationRewriterTest, RewriteWithSameComposerTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.InitializeWithRequestAndConfig(default_request(), default_config(),
                                       mock_data_manager_);
  composer::Composer composer(&table, &default_request(), &default_config());
  SetAkann(&composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  ConversionRequest request(&composer, &default_request(), &default_config());
  segment->set_key("あかん");
  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  {
    EXPECT_EQ(1, segments.conversion_segments_size());
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ("あかん", seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("アカン",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("akann", seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("AKANN",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("akann",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "Akann",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ａｋａｎｎ",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    EXPECT_EQ("ＡＫＡＮＮ",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    EXPECT_EQ("ａｋａｎｎ",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    EXPECT_EQ(
        "Ａｋａｎｎ",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ｱｶﾝ", seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }

  // Resegmentation
  segment = segments.mutable_segment(0);
  CHECK(segment);
  segment->set_key("あか");

  segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("ん");

  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

  EXPECT_EQ(2, segments.conversion_segments_size());
  {
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ("あか", seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("アカ", seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("aka", seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("AKA",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("aka",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "Aka",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ａｋａ", seg.meta_candidate(transliteration::FULL_ASCII).value);
    EXPECT_EQ("ＡＫＡ",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    EXPECT_EQ("ａｋａ",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    EXPECT_EQ(
        "Ａｋａ",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ｱｶ", seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
  {
    const Segment &seg = segments.conversion_segment(1);

    EXPECT_EQ("ん", seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("ン", seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("nn", seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("NN",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("nn",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "Nn",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ｎｎ", seg.meta_candidate(transliteration::FULL_ASCII).value);
    EXPECT_EQ("ＮＮ",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    EXPECT_EQ("ｎｎ",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    EXPECT_EQ(
        "Ｎｎ",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ﾝ", seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
}

TEST_F(TransliterationRewriterTest, NoKeyTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("あ");
  segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("");  // void key

  ConversionRequest request;
  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  EXPECT_EQ(2, segments.conversion_segments_size());
  EXPECT_NE(0, segments.conversion_segment(0).meta_candidates_size());
  EXPECT_EQ(0, segments.conversion_segment(1).meta_candidates_size());
}

TEST_F(TransliterationRewriterTest, NoKeyWithComposerTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.InitializeWithRequestAndConfig(default_request(), default_config(),
                                       mock_data_manager_);
  composer::Composer composer(&table, &default_request(), &default_config());
  InsertASCIISequence("a", &composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  ConversionRequest request(&composer, &default_request(), &default_config());

  segment->set_key("あ");
  segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("");  // void key

  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  EXPECT_EQ(2, segments.conversion_segments_size());
  EXPECT_NE(0, segments.conversion_segment(0).meta_candidates_size());
  EXPECT_EQ(0, segments.conversion_segment(1).meta_candidates_size());
}

TEST_F(TransliterationRewriterTest, NoRewriteTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.InitializeWithRequestAndConfig(default_request(), default_config(),
                                       mock_data_manager_);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("亜");
  ConversionRequest request;
  EXPECT_FALSE(t13n_rewriter->Rewrite(request, &segments));
  EXPECT_EQ(0, segments.conversion_segment(0).meta_candidates_size());
}

TEST_F(TransliterationRewriterTest, MobileEnvironmentTest) {
  ConversionRequest convreq;
  commands::Request request;
  convreq.set_request(&request);
  std::unique_ptr<TransliterationRewriter> rewriter(
      CreateTransliterationRewriter());
  {
    request.set_mixed_conversion(true);
    EXPECT_EQ(RewriterInterface::ALL, rewriter->capability(convreq));
  }

  {
    request.set_mixed_conversion(false);
    EXPECT_EQ(RewriterInterface::CONVERSION, rewriter->capability(convreq));
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTestWith12KeysHiragana) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_special_romanji_table(commands::Request::TWELVE_KEYS_TO_HIRAGANA);

  composer::Table table;
  table.InitializeWithRequestAndConfig(request, default_config(),
                                       mock_data_manager_);
  composer::Composer composer(&table, &request, &default_config());

  {
    InsertASCIISequence("11#", &composer);
    std::string query;
    composer.GetQueryForConversion(&query);
    EXPECT_EQ("い、", query);
  }
  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("い、");
  ConversionRequest rewrite_request(&composer, &request, &default_config());
  EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments));

  // Do not want to show raw keys for implementation
  {
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ("い、", seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("イ、", seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("い、", seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("い、",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("い、",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "い、",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    EXPECT_EQ("い、", seg.meta_candidate(transliteration::FULL_ASCII).value);
    EXPECT_EQ("い、",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    EXPECT_EQ("い、",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    EXPECT_EQ(
        "い、",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ｲ､", seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTestWith12KeysToNumber) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_special_romanji_table(commands::Request::TWELVE_KEYS_TO_HIRAGANA);

  composer::Table table;
  table.InitializeWithRequestAndConfig(request, default_config(),
                                       mock_data_manager_);
  composer::Composer composer(&table, &request, &default_config());

  {
    InsertASCIISequence("1212", &composer);
    std::string query;
    composer.GetQueryForConversion(&query);
    EXPECT_EQ("あかあか", query);
  }
  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("あかあか");
  ConversionRequest rewrite_request(&composer, &request, &default_config());
  EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments));

  // Because NoTransliteration attribute is specified in the mobile romaji
  // table's entries, raw-key based meta-candidates are not shown.
  {
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ("あかあか", seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("アカアカ",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("あかあか",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("あかあか",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("あかあか",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "あかあか",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    EXPECT_EQ("あかあか",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    EXPECT_EQ("あかあか",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    EXPECT_EQ("あかあか",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    EXPECT_EQ(
        "あかあか",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ｱｶｱｶ", seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTestWith12KeysFlick) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_special_romanji_table(
      commands::Request::TOGGLE_FLICK_TO_HIRAGANA);

  composer::Table table;
  table.InitializeWithRequestAndConfig(request, default_config(),
                                       mock_data_manager_);
  composer::Composer composer(&table, &request, &default_config());

  {
    InsertASCIISequence("1a", &composer);
    std::string query;
    composer.GetQueryForConversion(&query);
    EXPECT_EQ("あき", query);
  }
  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("あき");
  ConversionRequest rewrite_request(&composer, &request, &default_config());
  EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments));

  // Do not want to show raw keys for implementation
  {
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ("あき", seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("アキ", seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("あき", seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("あき",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("あき",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "あき",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    EXPECT_EQ("あき", seg.meta_candidate(transliteration::FULL_ASCII).value);
    EXPECT_EQ("あき",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    EXPECT_EQ("あき",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    EXPECT_EQ(
        "あき",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ｱｷ", seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTestWithQwertyHiragana) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request client_request;
  client_request.set_zero_query_suggestion(true);
  client_request.set_mixed_conversion(true);
  client_request.set_special_romanji_table(
      commands::Request::QWERTY_MOBILE_TO_HIRAGANA);

  const std::string kShi = "し";
  composer::Table table;
  table.InitializeWithRequestAndConfig(client_request, default_config(),
                                       mock_data_manager_);

  {
    composer::Composer composer(&table, &client_request, &default_config());

    InsertASCIISequence("shi", &composer);
    std::string query;
    composer.GetQueryForConversion(&query);
    EXPECT_EQ(kShi, query);

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kShi);
    ConversionRequest request(&composer, &client_request, &default_config());
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ("shi", seg.meta_candidate(transliteration::HALF_ASCII).value);
  }

  {
    composer::Composer composer(&table, &client_request, &default_config());

    InsertASCIISequence("si", &composer);
    std::string query;
    composer.GetQueryForConversion(&query);
    EXPECT_EQ(kShi, query);

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kShi);
    ConversionRequest request(&composer, &client_request, &default_config());
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ("si", seg.meta_candidate(transliteration::HALF_ASCII).value);
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTestWithGodan) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_special_romanji_table(commands::Request::GODAN_TO_HIRAGANA);

  composer::Table table;
  table.InitializeWithRequestAndConfig(request, default_config(),
                                       mock_data_manager_);
  composer::Composer composer(&table, &request, &default_config());
  {
    InsertASCIISequence("<'de", &composer);
    std::string query;
    composer.GetQueryForConversion(&query);
    EXPECT_EQ("あん゜で", query);
  }
  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("あん゜で");
  ConversionRequest rewrite_request(&composer, &request, &default_config());
  EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments));

  // Do not show raw keys.
  {
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ("あん゜で", seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("アン゜デ",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("annde", seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("ANNDE",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("annde",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "Annde",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ａｎｎｄｅ",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    EXPECT_EQ("ＡＮＮＤＥ",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    EXPECT_EQ("ａｎｎｄｅ",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    EXPECT_EQ(
        "Ａｎｎｄｅ",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    EXPECT_EQ("ｱﾝﾟﾃﾞ",
              seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTestValidateGodanT13nTable) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_special_romanji_table(commands::Request::GODAN_TO_HIRAGANA);

  composer::Table table;
  table.InitializeWithRequestAndConfig(request, default_config(),
                                       mock_data_manager_);

  // Expected t13n of Godan keyboard.
  std::vector<const char *> keycode_to_t13n_map(
      128, static_cast<const char *>(nullptr));
  keycode_to_t13n_map['"'] = "";
  keycode_to_t13n_map['\''] = "";
  keycode_to_t13n_map['`'] = "";
  keycode_to_t13n_map['$'] = "axtu";
  keycode_to_t13n_map['%'] = "ixtu";
  keycode_to_t13n_map['&'] = "uxtu";
  keycode_to_t13n_map['='] = "extu";
  keycode_to_t13n_map['@'] = "oxtu";
  keycode_to_t13n_map['#'] = "ya";
  keycode_to_t13n_map['+'] = "xi";
  keycode_to_t13n_map['^'] = "yu";
  keycode_to_t13n_map['_'] = "xe";
  keycode_to_t13n_map['|'] = "yo";
  keycode_to_t13n_map['<'] = "ann";
  keycode_to_t13n_map['>'] = "inn";
  keycode_to_t13n_map['{'] = "unn";
  keycode_to_t13n_map['}'] = "enn";
  keycode_to_t13n_map['~'] = "onn";
  keycode_to_t13n_map['\\'] = "nn";
  keycode_to_t13n_map[';'] = "";

  for (int i = 0; i < keycode_to_t13n_map.size(); ++i) {
    if (iscntrl(i)) {
      continue;
    }

    composer::Composer composer(&table, &request, &default_config());

    std::string ascii_input(1, static_cast<char>(i));
    InsertASCIISequence(ascii_input, &composer);
    std::string query;
    composer.GetQueryForConversion(&query);
    SCOPED_TRACE(absl::StrFormat("char code = %d, ascii_input = %s, query = %s",
                                 i, ascii_input.c_str(), query.c_str()));

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(query);
    ConversionRequest rewrite_request(&composer, &request, &default_config());
    EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments) ||
                query.empty());

    const Segment &seg = segments.conversion_segment(0);
    if (seg.meta_candidates_size() <= transliteration::HALF_ASCII) {
      // If no t13n happened, then should be no custom t13n.
      EXPECT_TRUE(keycode_to_t13n_map[i] == nullptr ||
                  keycode_to_t13n_map[i] == std::string(""));
    } else {
      const std::string &half_ascii =
          seg.meta_candidate(transliteration::HALF_ASCII).value;
      if (keycode_to_t13n_map[i] && keycode_to_t13n_map[i] != std::string("")) {
        EXPECT_EQ(keycode_to_t13n_map[i], half_ascii);
      } else {
        EXPECT_TRUE(half_ascii == ascii_input || half_ascii == query);
      }
    }
  }
}

TEST_F(TransliterationRewriterTest, T13nOnSuggestion) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request client_request;
  client_request.set_mixed_conversion(true);

  const std::string kXtsu = "っ";

  composer::Table table;
  table.InitializeWithRequestAndConfig(client_request, default_config(),
                                       mock_data_manager_);
  {
    composer::Composer composer(&table, &client_request, &default_config());

    InsertASCIISequence("ssh", &composer);
    std::string query;
    composer.GetQueryForPrediction(&query);
    EXPECT_EQ(kXtsu, query);

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kXtsu);
    ConversionRequest request(&composer, &client_request, &default_config());
    request.set_request_type(ConversionRequest::SUGGESTION);
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ("ssh", seg.meta_candidate(transliteration::HALF_ASCII).value);
  }
}

TEST_F(TransliterationRewriterTest, T13nOnPartialSuggestion) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request client_request;
  client_request.set_mixed_conversion(true);

  const std::string kXtsu = "っ";

  composer::Table table;
  table.InitializeWithRequestAndConfig(client_request, default_config(),
                                       mock_data_manager_);
  {
    composer::Composer composer(&table, &client_request, &default_config());

    InsertASCIISequence("ssh", &composer);  // "っsh|"
    std::string query;
    composer.GetQueryForPrediction(&query);
    EXPECT_EQ(kXtsu, query);

    composer.MoveCursorTo(1);  // "っ|sh"

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kXtsu);
    ConversionRequest request(&composer, &client_request, &default_config());
    request.set_request_type(ConversionRequest::PARTIAL_SUGGESTION);
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ("s", seg.meta_candidate(transliteration::HALF_ASCII).value);
  }
}

}  // namespace mozc

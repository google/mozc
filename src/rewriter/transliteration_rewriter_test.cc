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

#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace {

void InsertASCIISequence(const absl::string_view text,
                         composer::Composer *composer) {
  for (size_t i = 0; i < text.size(); ++i) {
    commands::KeyEvent key;
    key.set_key_code(text[i]);
    composer->InsertCharacterKeyEvent(key);
  }
}

void SetAkann(composer::Composer *composer) {
  InsertASCIISequence("akann", composer);
  EXPECT_EQ(composer->GetQueryForConversion(), "あかん");
}

}  // namespace

class TransliterationRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    config::ConfigHandler::GetDefaultConfig(&default_config_);
  }

  TransliterationRewriter *CreateTransliterationRewriter() const {
    return new TransliterationRewriter(
        dictionary::PosMatcher(mock_data_manager_.GetPosMatcherData()));
  }

  const commands::Request &default_request() const { return default_request_; }

  const config::Config &default_config() const { return default_config_; }

  const testing::MockDataManager mock_data_manager_;

 private:
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
  EXPECT_EQ(segment->meta_candidates_size(), 0);
  const ConversionRequest default_request;
  EXPECT_TRUE(t13n_rewriter->Rewrite(default_request, &segments));
  {
    EXPECT_EQ(segment->meta_candidate(transliteration::HIRAGANA).value,
              "あかん");
    EXPECT_EQ(segment->meta_candidate(transliteration::FULL_KATAKANA).value,
              "アカン");
    EXPECT_EQ(segment->meta_candidate(transliteration::HALF_ASCII).value,
              "akan");
    EXPECT_EQ(segment->meta_candidate(transliteration::HALF_ASCII_UPPER).value,
              "AKAN");
    EXPECT_EQ(segment->meta_candidate(transliteration::HALF_ASCII_LOWER).value,
              "akan");
    EXPECT_EQ(
        segment->meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value,
        "Akan");
    EXPECT_EQ(segment->meta_candidate(transliteration::FULL_ASCII).value,
              "ａｋａｎ");
    EXPECT_EQ(segment->meta_candidate(transliteration::FULL_ASCII_UPPER).value,
              "ＡＫＡＮ");
    EXPECT_EQ(segment->meta_candidate(transliteration::FULL_ASCII_LOWER).value,
              "ａｋａｎ");
    EXPECT_EQ(
        segment->meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value,
        "Ａｋａｎ");
    EXPECT_EQ(segment->meta_candidate(transliteration::HALF_KATAKANA).value,
              "ｱｶﾝ");
    for (size_t i = 0; i < segment->meta_candidates_size(); ++i) {
      EXPECT_NE(segment->meta_candidate(i).lid, 0);
      EXPECT_NE(segment->meta_candidate(i).rid, 0);
      EXPECT_FALSE(segment->meta_candidate(i).key.empty());
    }
  }
}

TEST_F(TransliterationRewriterTest, T13nFromComposerTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(default_request(), default_config());
  composer::Composer composer(table, default_request(), default_config());
  SetAkann(&composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  const ConversionRequest request =
      ConversionRequestBuilder().SetComposer(composer).Build();
  segment->set_key("あかん");
  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  {
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ(seg.meta_candidate(transliteration::HIRAGANA).value, "あかん");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_KATAKANA).value,
              "アカン");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "akann");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value,
              "AKANN");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value,
              "akann");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value,
              "Akann");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII).value,
              "ａｋａｎｎ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value,
              "ＡＫＡＮＮ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value,
              "ａｋａｎｎ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value,
              "Ａｋａｎｎ");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_KATAKANA).value, "ｱｶﾝ");
    for (size_t i = 0; i < segment->meta_candidates_size(); ++i) {
      EXPECT_NE(segment->meta_candidate(i).lid, 0);
      EXPECT_NE(segment->meta_candidate(i).rid, 0);
      EXPECT_FALSE(segment->meta_candidate(i).key.empty());
    }
  }
}

TEST_F(TransliterationRewriterTest, KeyOfT13nFromComposerTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(default_request(), default_config());
  composer::Composer composer(table, default_request(), default_config());
  InsertASCIISequence("ssh", &composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  commands::Request input;
  input.set_mixed_conversion(true);
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetComposer(composer)
          .SetRequest(input)
          .SetRequestType(ConversionRequest::SUGGESTION)
          .Build();
  {
    // Although the segment key is "っ" as a partial string of the full
    // composition, the transliteration key should be "っsh" as the
    // whole composition string.

    segment->set_key("っ");
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    EXPECT_EQ(segments.conversion_segments_size(), 1);
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ(seg.meta_candidate(transliteration::HIRAGANA).value, "っｓｈ");
    EXPECT_EQ(seg.meta_candidate(transliteration::HIRAGANA).key, "っsh");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "ssh");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).key, "っsh");
  }
}

TEST_F(TransliterationRewriterTest, T13nWithMultiSegmentsTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(default_request(), default_config());
  composer::Composer composer(table, default_request(), default_config());

  // Set kamabokoinbou to composer.
  {
    InsertASCIISequence("kamabokonoinbou", &composer);
    EXPECT_EQ(composer.GetQueryForConversion(), "かまぼこのいんぼう");
  }

  Segments segments;
  const ConversionRequest request =
      ConversionRequestBuilder().SetComposer(composer).Build();
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
    segment->set_key("かまぼこの");
    segment = segments.add_segment();
    CHECK(segment);
    segment->set_key("いんぼう");
  }

  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  EXPECT_EQ(segments.conversion_segments_size(), 2);
  {
    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ(seg.meta_candidate(transliteration::HIRAGANA).value,
              "かまぼこの");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value,
              "kamabokono");
  }
  {
    const Segment &seg = segments.conversion_segment(1);
    EXPECT_EQ(seg.meta_candidate(transliteration::HIRAGANA).value, "いんぼう");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "inbou");
  }
}

TEST_F(TransliterationRewriterTest, ComposerValidationTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(default_request(), default_config());
  composer::Composer composer(table, default_request(), default_config());

  // Set kan to composer.
  {
    InsertASCIISequence("kan", &composer);
    EXPECT_EQ(composer.GetQueryForConversion(), "かん");
  }

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  const ConversionRequest request =
      ConversionRequestBuilder().SetComposer(composer).Build();
  segment->set_key("かん");
  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  // Should not use composer
  {
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ(seg.meta_candidate(transliteration::HIRAGANA).value, "かん");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_KATAKANA).value, "カン");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "kan");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value,
              "KAN");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value,
              "kan");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value,
              "Kan");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII).value, "ｋａｎ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value,
              "ＫＡＮ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value,
              "ｋａｎ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value,
              "Ｋａｎ");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_KATAKANA).value, "ｶﾝ");
  }
}

TEST_F(TransliterationRewriterTest, RewriteWithSameComposerTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(default_request(), default_config());

  composer::Composer composer(table, default_request(), default_config());
  SetAkann(&composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  const ConversionRequest request =
      ConversionRequestBuilder().SetComposer(composer).Build();
  segment->set_key("あかん");
  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  {
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ(seg.meta_candidate(transliteration::HIRAGANA).value, "あかん");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_KATAKANA).value,
              "アカン");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "akann");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value,
              "AKANN");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value,
              "akann");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value,
              "Akann");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII).value,
              "ａｋａｎｎ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value,
              "ＡＫＡＮＮ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value,
              "ａｋａｎｎ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value,
              "Ａｋａｎｎ");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_KATAKANA).value, "ｱｶﾝ");
  }

  // Resegmentation
  segment = segments.mutable_segment(0);
  CHECK(segment);
  segment->set_key("あか");

  segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("ん");

  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

  EXPECT_EQ(segments.conversion_segments_size(), 2);
  {
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ(seg.meta_candidate(transliteration::HIRAGANA).value, "あか");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_KATAKANA).value, "アカ");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "aka");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value,
              "AKA");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value,
              "aka");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value,
              "Aka");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII).value, "ａｋａ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value,
              "ＡＫＡ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value,
              "ａｋａ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value,
              "Ａｋａ");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_KATAKANA).value, "ｱｶ");
  }
  {
    const Segment &seg = segments.conversion_segment(1);

    EXPECT_EQ(seg.meta_candidate(transliteration::HIRAGANA).value, "ん");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_KATAKANA).value, "ン");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "nn");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value,
              "NN");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value,
              "nn");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value,
              "Nn");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII).value, "ｎｎ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value,
              "ＮＮ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value,
              "ｎｎ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value,
              "Ｎｎ");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_KATAKANA).value, "ﾝ");
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
  EXPECT_EQ(segments.conversion_segments_size(), 2);
  EXPECT_NE(segments.conversion_segment(0).meta_candidates_size(), 0);
  EXPECT_EQ(segments.conversion_segment(1).meta_candidates_size(), 0);
}

TEST_F(TransliterationRewriterTest, NoKeyWithComposerTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(default_request(), default_config());
  composer::Composer composer(table, default_request(), default_config());
  InsertASCIISequence("a", &composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  const ConversionRequest request =
      ConversionRequestBuilder().SetComposer(composer).Build();

  segment->set_key("あ");
  segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("");  // void key

  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  EXPECT_EQ(segments.conversion_segments_size(), 2);
  EXPECT_NE(segments.conversion_segment(0).meta_candidates_size(), 0);
  EXPECT_EQ(segments.conversion_segment(1).meta_candidates_size(), 0);
}

TEST_F(TransliterationRewriterTest, NoRewriteTest) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(default_request(), default_config());

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("亜");
  ConversionRequest request;
  EXPECT_FALSE(t13n_rewriter->Rewrite(request, &segments));
  EXPECT_EQ(segments.conversion_segment(0).meta_candidates_size(), 0);
}

TEST_F(TransliterationRewriterTest, MobileEnvironmentTest) {
  commands::Request request;
  std::unique_ptr<TransliterationRewriter> rewriter(
      CreateTransliterationRewriter());
  {
    request.set_mixed_conversion(true);
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_EQ(rewriter->capability(convreq), RewriterInterface::ALL);
  }

  {
    request.set_mixed_conversion(false);
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_EQ(rewriter->capability(convreq), RewriterInterface::CONVERSION);
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTestWith12KeysHiragana) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request_test_util::FillMobileRequest(&request);

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(request, default_config());
  composer::Composer composer(table, request, default_config());

  {
    InsertASCIISequence("11#", &composer);
    EXPECT_EQ(composer.GetQueryForConversion(), "い、");
  }
  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("い、");
  const ConversionRequest rewrite_request = ConversionRequestBuilder()
                                                .SetComposer(composer)
                                                .SetRequest(request)
                                                .SetConfig(default_config())
                                                .Build();
  EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments));

  // Do not want to show raw keys for implementation
  {
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ(seg.meta_candidate(transliteration::HIRAGANA).value, "い、");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_KATAKANA).value, "イ、");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "い、");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value,
              "い、");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value,
              "い、");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value,
              "い、");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII).value, "い、");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value,
              "い、");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value,
              "い、");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value,
              "い、");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_KATAKANA).value, "ｲ､");
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTestWith12KeysToNumber) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request_test_util::FillMobileRequest(&request);

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(request, default_config());
  composer::Composer composer(table, request, default_config());

  {
    InsertASCIISequence("1212", &composer);
    EXPECT_EQ(composer.GetQueryForConversion(), "あかあか");
  }
  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("あかあか");
  const ConversionRequest rewrite_request = ConversionRequestBuilder()
                                                .SetComposer(composer)
                                                .SetRequest(request)
                                                .Build();
  EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments));

  // Because NoTransliteration attribute is specified in the mobile romaji
  // table's entries, raw-key based meta-candidates are not shown.
  {
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ(seg.meta_candidate(transliteration::HIRAGANA).value, "あかあか");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_KATAKANA).value,
              "アカアカ");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value,
              "あかあか");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value,
              "あかあか");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value,
              "あかあか");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value,
              "あかあか");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII).value,
              "あかあか");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value,
              "あかあか");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value,
              "あかあか");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value,
              "あかあか");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_KATAKANA).value, "ｱｶｱｶ");
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

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(request, default_config());
  composer::Composer composer(table, request, default_config());

  {
    InsertASCIISequence("1a", &composer);
    EXPECT_EQ(composer.GetQueryForConversion(), "あき");
  }
  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("あき");
  const ConversionRequest rewrite_request = ConversionRequestBuilder()
                                                .SetComposer(composer)
                                                .SetRequest(request)
                                                .Build();
  EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments));

  // Do not want to show raw keys for implementation
  {
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ(seg.meta_candidate(transliteration::HIRAGANA).value, "あき");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_KATAKANA).value, "アキ");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "あき");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value,
              "あき");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value,
              "あき");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value,
              "あき");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII).value, "あき");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value,
              "あき");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value,
              "あき");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value,
              "あき");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_KATAKANA).value, "ｱｷ");
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
  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(client_request, default_config());

  {
    composer::Composer composer(table, client_request, default_config());

    InsertASCIISequence("shi", &composer);
    EXPECT_EQ(composer.GetQueryForConversion(), kShi);

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kShi);
    const ConversionRequest request = ConversionRequestBuilder()
                                          .SetComposer(composer)
                                          .SetRequest(client_request)
                                          .Build();
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "shi");
  }

  {
    composer::Composer composer(table, client_request, default_config());

    InsertASCIISequence("si", &composer);
    EXPECT_EQ(composer.GetQueryForConversion(), kShi);

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kShi);
    const ConversionRequest request = ConversionRequestBuilder()
                                          .SetComposer(composer)
                                          .SetRequest(client_request)
                                          .Build();
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "si");
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTestWithGodan) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_special_romanji_table(commands::Request::GODAN_TO_HIRAGANA);

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(request, default_config());
  composer::Composer composer(table, request, default_config());
  {
    InsertASCIISequence("<'de", &composer);
    EXPECT_EQ(composer.GetQueryForConversion(), "あん゜で");
  }
  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("あん゜で");
  const ConversionRequest rewrite_request = ConversionRequestBuilder()
                                                .SetComposer(composer)
                                                .SetRequest(request)
                                                .Build();
  EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments));

  // Do not show raw keys.
  {
    const Segment &seg = segments.conversion_segment(0);

    EXPECT_EQ(seg.meta_candidate(transliteration::HIRAGANA).value, "あん゜で");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_KATAKANA).value,
              "アン゜デ");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "annde");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value,
              "ANNDE");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value,
              "annde");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value,
              "Annde");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII).value,
              "ａｎｎｄｅ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value,
              "ＡＮＮＤＥ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value,
              "ａｎｎｄｅ");
    EXPECT_EQ(seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value,
              "Ａｎｎｄｅ");
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_KATAKANA).value,
              "ｱﾝﾟﾃﾞ");
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTestValidateGodanT13nTable) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_special_romanji_table(commands::Request::GODAN_TO_HIRAGANA);

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(request, default_config());

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

    composer::Composer composer(table, request, default_config());

    std::string ascii_input(1, static_cast<char>(i));
    InsertASCIISequence(ascii_input, &composer);
    const std::string query = composer.GetQueryForConversion();
    SCOPED_TRACE(absl::StrFormat("char code = %d, ascii_input = %s, query = %s",
                                 i, ascii_input, query));

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(query);
    const ConversionRequest rewrite_request = ConversionRequestBuilder()
                                                  .SetComposer(composer)
                                                  .SetRequest(request)
                                                  .Build();
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
        EXPECT_EQ(half_ascii, keycode_to_t13n_map[i]);
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

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(client_request, default_config());
  {
    composer::Composer composer(table, client_request, default_config());

    InsertASCIISequence("ssh", &composer);
    EXPECT_EQ(composer.GetQueryForPrediction(), kXtsu);

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kXtsu);
    const ConversionRequest request =
        ConversionRequestBuilder()
            .SetComposer(composer)
            .SetRequest(client_request)
            .SetRequestType(ConversionRequest::SUGGESTION)
            .Build();
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "ssh");
  }
}

TEST_F(TransliterationRewriterTest, T13nOnPartialSuggestion) {
  std::unique_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request client_request;
  client_request.set_mixed_conversion(true);

  const std::string kXtsu = "っ";

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(client_request, default_config());
  {
    composer::Composer composer(table, client_request, default_config());

    InsertASCIISequence("ssh", &composer);  // "っsh|"
    EXPECT_EQ(composer.GetQueryForPrediction(), kXtsu);

    composer.MoveCursorTo(1);  // "っ|sh"

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kXtsu);
    const ConversionRequest request =
        ConversionRequestBuilder()
            .SetComposer(composer)
            .SetRequest(client_request)
            .SetRequestType(ConversionRequest::SUGGESTION)
            .Build();
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ(seg.meta_candidate(transliteration::HALF_ASCII).value, "s");
  }
}

}  // namespace mozc

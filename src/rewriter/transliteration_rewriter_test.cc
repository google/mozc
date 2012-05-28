// Copyright 2010-2012, Google Inc.
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

#include <string>
#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/pos_matcher.h"
#include "session/commands.pb.h"
#include "session/request_handler.h"
#include "session/request_test_util.h"
#include "testing/base/public/gunit.h"
#include "transliteration/transliteration.h"

DECLARE_string(test_tmpdir);

namespace mozc {

using mozc::commands::ScopedRequestForUnittest;

namespace {
void InsertASCIISequence(const string &text, composer::Composer *composer) {
  for (size_t i = 0; i < text.size(); ++i) {
    commands::KeyEvent key;
    key.set_key_code(text[i]);
    composer->InsertCharacterKeyEvent(key);
  }
}

void SetAkann(composer::Composer *composer) {
  InsertASCIISequence("akann", composer);
  string query;
  composer->GetQueryForConversion(&query);
  // "あかん"
  EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b\xe3\x82\x93", query);
}
}  // namespace

class TransliterationRewriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    prev_preference_.CopyFrom(commands::RequestHandler::GetRequest());
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    commands::RequestHandler::SetRequest(prev_preference_);
  }

  TransliterationRewriter *CreateTransliterationRewriter() const {
    return new TransliterationRewriter(
        *UserPosManager::GetUserPosManager()->GetPOSMatcher());
  }

 private:
  commands::Request prev_preference_;
};

TEST_F(TransliterationRewriterTest, T13NFromKeyTest) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());
  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  // "あかん"
  segment->set_key("\xe3\x81\x82\xe3\x81\x8b\xe3\x82\x93");
  EXPECT_EQ(0, segment->meta_candidates_size());
  EXPECT_TRUE(t13n_rewriter->Rewrite(ConversionRequest(), &segments));
  {
    // "あかん"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b\xe3\x82\x93",
              segment->meta_candidate(transliteration::HIRAGANA).value);
    // "アカン"
    EXPECT_EQ("\xe3\x82\xa2\xe3\x82\xab\xe3\x83\xb3",
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
    // "ａｋａｎ"
    EXPECT_EQ("\xef\xbd\x81\xef\xbd\x8b\xef\xbd\x81\xef\xbd\x8e",
              segment->meta_candidate(transliteration::FULL_ASCII).value);
    // "ＡＫＡＮ"
    EXPECT_EQ("\xef\xbc\xa1\xef\xbc\xab\xef\xbc\xa1\xef\xbc\xae",
              segment->meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    // "ａｋａｎ"
    EXPECT_EQ("\xef\xbd\x81\xef\xbd\x8b\xef\xbd\x81\xef\xbd\x8e",
              segment->meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    // "Ａｋａｎ"
    EXPECT_EQ(
        "\xef\xbc\xa1\xef\xbd\x8b\xef\xbd\x81\xef\xbd\x8e",
        segment->meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    // "ｱｶﾝ"
    EXPECT_EQ("\xef\xbd\xb1\xef\xbd\xb6\xef\xbe\x9d",
              segment->meta_candidate(transliteration::HALF_KATAKANA).value);
    for (size_t i = 0; i < segment->meta_candidates_size(); ++i) {
      EXPECT_NE(0, segment->meta_candidate(i).lid);
      EXPECT_NE(0, segment->meta_candidate(i).rid);
      EXPECT_FALSE(segment->meta_candidate(i).key.empty());
    }
  }
}

TEST_F(TransliterationRewriterTest, T13NFromComposerTest) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);
  SetAkann(&composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  ConversionRequest request(&composer);
  // "あかん"
  segment->set_key("\xe3\x81\x82\xe3\x81\x8b\xe3\x82\x93");
  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  {
    EXPECT_EQ(1, segments.conversion_segments_size());
    const Segment &seg = segments.conversion_segment(0);

    // "あかん"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b\xe3\x82\x93",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    // "アカン"
    EXPECT_EQ("\xe3\x82\xa2\xe3\x82\xab\xe3\x83\xb3",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("akann",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("AKANN",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("akann",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "Akann",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    // "ａｋａｎｎ"
    EXPECT_EQ("\xef\xbd\x81\xef\xbd\x8b\xef\xbd\x81\xef\xbd\x8e\xef\xbd\x8e",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    // "ＡＫＡＮＮ"
    EXPECT_EQ("\xef\xbc\xa1\xef\xbc\xab\xef\xbc\xa1\xef\xbc\xae\xef\xbc\xae",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    // "ａｋａｎｎ"
    EXPECT_EQ("\xef\xbd\x81\xef\xbd\x8b\xef\xbd\x81\xef\xbd\x8e\xef\xbd\x8e",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    // "Ａｋａｎｎ"
    EXPECT_EQ(
        "\xef\xbc\xa1\xef\xbd\x8b\xef\xbd\x81\xef\xbd\x8e\xef\xbd\x8e",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    // "ｱｶﾝ"
    EXPECT_EQ("\xef\xbd\xb1\xef\xbd\xb6\xef\xbe\x9d",
              seg.meta_candidate(transliteration::HALF_KATAKANA).value);
    for (size_t i = 0; i < segment->meta_candidates_size(); ++i) {
      EXPECT_NE(0, segment->meta_candidate(i).lid);
      EXPECT_NE(0, segment->meta_candidate(i).rid);
      EXPECT_FALSE(segment->meta_candidate(i).key.empty());
    }
  }
}

TEST_F(TransliterationRewriterTest, T13NWithMultiSegmentsTest) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);
  SetAkann(&composer);

  Segments segments;
  ConversionRequest request(&composer);
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
    // "かまぼこの"
    segment->set_key(
        "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae");
    segment = segments.add_segment();
    CHECK(segment);
    // "いんぼう"
    segment->set_key("\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86");
  }

  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  EXPECT_EQ(2, segments.conversion_segments_size());
  {
    const Segment &seg = segments.conversion_segment(0);
    // "かまぼこの"
    EXPECT_EQ("\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("kamabokono",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
  }
  {
    const Segment &seg = segments.conversion_segment(1);
    // "いんぼう"
    EXPECT_EQ("\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    EXPECT_EQ("inbou",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
  }
}

TEST_F(TransliterationRewriterTest, ComposerValidationTest) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);
  SetAkann(&composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  ConversionRequest request(&composer);
  // "かん"
  segment->set_key("\xe3\x81\x8b\xe3\x82\x93");
  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  // Should not use composer
  {
    EXPECT_EQ(1, segments.conversion_segments_size());
    const Segment &seg = segments.conversion_segment(0);
    // "かん"
    EXPECT_EQ("\xe3\x81\x8b\xe3\x82\x93",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    // "カン"
    EXPECT_EQ("\xe3\x82\xab\xe3\x83\xb3",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("kan",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("KAN",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("kan",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "Kan",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    // "ｋａｎ"
    EXPECT_EQ("\xef\xbd\x8b\xef\xbd\x81\xef\xbd\x8e",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    // "ＫＡＮ"
    EXPECT_EQ("\xef\xbc\xab\xef\xbc\xa1\xef\xbc\xae",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    // "ｋａｎ"
    EXPECT_EQ("\xef\xbd\x8b\xef\xbd\x81\xef\xbd\x8e",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    // "Ｋａｎ"
    EXPECT_EQ(
        "\xef\xbc\xab\xef\xbd\x81\xef\xbd\x8e",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    // "ｶﾝ"
    EXPECT_EQ("\xef\xbd\xb6\xef\xbe\x9d",
              seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
}

TEST_F(TransliterationRewriterTest, RewriteWithSameComposerTest) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);
  SetAkann(&composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  ConversionRequest request(&composer);
  // "あかん"
  segment->set_key("\xe3\x81\x82\xe3\x81\x8b\xe3\x82\x93");
  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  {
    EXPECT_EQ(1, segments.conversion_segments_size());
    const Segment &seg = segments.conversion_segment(0);

    // "あかん"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b\xe3\x82\x93",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    // "アカン"
    EXPECT_EQ("\xe3\x82\xa2\xe3\x82\xab\xe3\x83\xb3",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("akann",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("AKANN",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("akann",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "Akann",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    // "ａｋａｎｎ"
    EXPECT_EQ("\xef\xbd\x81\xef\xbd\x8b\xef\xbd\x81\xef\xbd\x8e\xef\xbd\x8e",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    // "ＡＫＡＮＮ"
    EXPECT_EQ("\xef\xbc\xa1\xef\xbc\xab\xef\xbc\xa1\xef\xbc\xae\xef\xbc\xae",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    // "ａｋａｎｎ"
    EXPECT_EQ("\xef\xbd\x81\xef\xbd\x8b\xef\xbd\x81\xef\xbd\x8e\xef\xbd\x8e",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    // "Ａｋａｎｎ"
    EXPECT_EQ(
        "\xef\xbc\xa1\xef\xbd\x8b\xef\xbd\x81\xef\xbd\x8e\xef\xbd\x8e",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    // "ｱｶﾝ"
    EXPECT_EQ("\xef\xbd\xb1\xef\xbd\xb6\xef\xbe\x9d",
              seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }

  // Resegmentation
  segment = segments.mutable_segment(0);
  CHECK(segment);
  // "あか"
  segment->set_key("\xe3\x81\x82\xe3\x81\x8b");

  segment = segments.add_segment();
  CHECK(segment);
  // "ん"
  segment->set_key("\xe3\x82\x93");

  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

  EXPECT_EQ(2, segments.conversion_segments_size());
  {
    const Segment &seg = segments.conversion_segment(0);

    // "あか"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    // "アカ"
    EXPECT_EQ("\xe3\x82\xa2\xe3\x82\xab",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("aka",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("AKA",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("aka",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "Aka",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    // "ａｋａ"
    EXPECT_EQ("\xef\xbd\x81\xef\xbd\x8b\xef\xbd\x81",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    // "ＡＫＡ"
    EXPECT_EQ("\xef\xbc\xa1\xef\xbc\xab\xef\xbc\xa1",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    // "ａｋａ"
    EXPECT_EQ("\xef\xbd\x81\xef\xbd\x8b\xef\xbd\x81",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    // "Ａｋａ"
    EXPECT_EQ(
        "\xef\xbc\xa1\xef\xbd\x8b\xef\xbd\x81",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    // "ｱｶ"
    EXPECT_EQ("\xef\xbd\xb1\xef\xbd\xb6",
              seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
  {
    const Segment &seg = segments.conversion_segment(1);

    // "ん"
    EXPECT_EQ("\xe3\x82\x93",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    // "ン"
    EXPECT_EQ("\xe3\x83\xb3",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    EXPECT_EQ("nn",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
    EXPECT_EQ("NN",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    EXPECT_EQ("nn",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        "Nn",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    // "ｎｎ"
    EXPECT_EQ("\xef\xbd\x8e\xef\xbd\x8e",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    // "ＮＮ"
    EXPECT_EQ("\xef\xbc\xae\xef\xbc\xae",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    // "ｎｎ"
    EXPECT_EQ("\xef\xbd\x8e\xef\xbd\x8e",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    EXPECT_EQ(
        // "Ｎｎ"
        "\xef\xbc\xae\xef\xbd\x8e",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    // "ﾝ"
    EXPECT_EQ("\xef\xbe\x9d",
              seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
}

TEST_F(TransliterationRewriterTest, NoKeyTest) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  // "あ"
  segment->set_key("\xe3\x81\x82");
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
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);
  InsertASCIISequence("a", &composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  ConversionRequest request(&composer);

  // "あ"
  segment->set_key("\xe3\x81\x82");
  segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("");  // void key

  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  EXPECT_EQ(2, segments.conversion_segments_size());
  EXPECT_NE(0, segments.conversion_segment(0).meta_candidates_size());
  EXPECT_EQ(0, segments.conversion_segment(1).meta_candidates_size());
}

TEST_F(TransliterationRewriterTest, NoRewriteTest) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.Initialize();

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  // "亜"
  segment->set_key("\xe4\xba\x9c");
  ConversionRequest request;
  EXPECT_FALSE(t13n_rewriter->Rewrite(request, &segments));
  EXPECT_EQ(0, segments.conversion_segment(0).meta_candidates_size());
}

TEST_F(TransliterationRewriterTest, NormalizedTransliterations) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);

  // "らゔ"
  composer.InsertCharacterPreedit("\xE3\x82\x89\xE3\x82\x94");

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    CHECK(segment);
    // "らゔ"
    segment->set_key("\xE3\x82\x89\xE3\x82\x94");
    segment->add_candidate()->value = "LOVE";
  }

  ConversionRequest request(&composer);
  EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));
  EXPECT_EQ(1, segments.segments_size());
  const Segment &seg = segments.segment(0);
  // "らヴ"
  EXPECT_EQ("\xE3\x82\x89\xE3\x83\xB4",
            seg.meta_candidate(transliteration::HIRAGANA).value);
}

TEST_F(TransliterationRewriterTest, MobileEnvironmentTest) {
  commands::Request input;
  scoped_ptr<TransliterationRewriter> rewriter(CreateTransliterationRewriter());

  input.set_mixed_conversion(true);
  commands::RequestHandler::SetRequest(input);
  EXPECT_EQ(RewriterInterface::ALL, rewriter->capability());

  input.set_mixed_conversion(false);
  commands::RequestHandler::SetRequest(input);
  EXPECT_EQ(RewriterInterface::CONVERSION, rewriter->capability());
}

TEST_F(TransliterationRewriterTest, MobileT13NTestWith12KeysHiragana) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_combine_all_segments(true);
  request.set_special_romanji_table(
      commands::Request::TWELVE_KEYS_TO_HIRAGANA);

  ScopedRequestForUnittest scoped_request(request);

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);
  {
    InsertASCIISequence("11#", &composer);
    string query;
    composer.GetQueryForConversion(&query);
    // "い、"
    EXPECT_EQ("\xe3\x81\x84\xe3\x80\x81", query);
  }
  Segments segments;
  Segment *segment = segments.add_segment();
  // "い、"
  segment->set_key("\xe3\x81\x84\xe3\x80\x81");
  ConversionRequest rewrite_request(&composer);
  EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments));

  // Do not want to show raw keys for implementation
  {
    const Segment &seg = segments.conversion_segment(0);

    // "い、"
    EXPECT_EQ("\xe3\x81\x84\xe3\x80\x81",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    // "イ、"
    EXPECT_EQ("\xe3\x82\xa4\xe3\x80\x81",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    // "い、"
    EXPECT_EQ("\xe3\x81\x84\xe3\x80\x81",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
    // "い、"
    EXPECT_EQ("\xe3\x81\x84\xe3\x80\x81",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    // "い、"
    EXPECT_EQ("\xe3\x81\x84\xe3\x80\x81",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    EXPECT_EQ(
        // "い、"
        "\xe3\x81\x84\xe3\x80\x81",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    // "い、"
    EXPECT_EQ("\xe3\x81\x84\xe3\x80\x81",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    // "い、"
    EXPECT_EQ("\xe3\x81\x84\xe3\x80\x81",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    // "い、"
    EXPECT_EQ("\xe3\x81\x84\xe3\x80\x81",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    EXPECT_EQ(
        // "い、"
        "\xe3\x81\x84\xe3\x80\x81",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    // "ｲ､"
    EXPECT_EQ("\xef\xbd\xb2\xef\xbd\xa4",
              seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
}

TEST_F(TransliterationRewriterTest, MobileT13NTestWith12KeysToNumber) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_combine_all_segments(true);
  request.set_special_romanji_table(
      commands::Request::TWELVE_KEYS_TO_HIRAGANA);

  ScopedRequestForUnittest scoped_request(request);

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);
  {
    InsertASCIISequence("1212", &composer);
    string query;
    composer.GetQueryForConversion(&query);
    // "あかあか"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x82\xe3\x81\x8b", query);
  }
  Segments segments;
  Segment *segment = segments.add_segment();
  // "あかあか"
  segment->set_key("\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x82\xe3\x81\x8b");
  ConversionRequest rewrite_request(&composer);
  EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments));

  // Do not want to show raw keys for implementation
  {
    const Segment &seg = segments.conversion_segment(0);

    // "あかあか"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x82\xe3\x81\x8b",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    // "アカアカ"
    EXPECT_EQ("\xe3\x82\xa2\xe3\x82\xab\xe3\x82\xa2\xe3\x82\xab",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    // "1212"
    EXPECT_EQ("1212",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
    // "1212"
    EXPECT_EQ("1212",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    // "1212"
    EXPECT_EQ("1212",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    // "1212"
    EXPECT_EQ(
        "1212",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    // "１２１２"
    EXPECT_EQ("\xef\xbc\x91\xef\xbc\x92\xef\xbc\x91\xef\xbc\x92",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    // "１２１２"
    EXPECT_EQ("\xef\xbc\x91\xef\xbc\x92\xef\xbc\x91\xef\xbc\x92",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    // "１２１２"
    EXPECT_EQ("\xef\xbc\x91\xef\xbc\x92\xef\xbc\x91\xef\xbc\x92",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    // "１２１２"
    EXPECT_EQ(
        "\xef\xbc\x91\xef\xbc\x92\xef\xbc\x91\xef\xbc\x92",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    // "ｱｶｱｶ"
    EXPECT_EQ("\xef\xbd\xb1\xef\xbd\xb6\xef\xbd\xb1\xef\xbd\xb6",
              seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
}

TEST_F(TransliterationRewriterTest, MobileT13NTestWith12KeysFlick) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_combine_all_segments(true);
  request.set_special_romanji_table(
      commands::Request::TOGGLE_FLICK_TO_HIRAGANA);

  ScopedRequestForUnittest scoped_request(request);

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);
  {
    InsertASCIISequence("1a", &composer);
    string query;
    composer.GetQueryForConversion(&query);
    // "あき"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8d", query);
  }
  Segments segments;
  Segment *segment = segments.add_segment();
  // "あき"
  segment->set_key("\xe3\x81\x82\xe3\x81\x8d");
  ConversionRequest rewrite_request(&composer);
  EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments));

  // Do not want to show raw keys for implementation
  {
    const Segment &seg = segments.conversion_segment(0);

    // "あき"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8d",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    // "アキ"
    EXPECT_EQ("\xe3\x82\xa2\xe3\x82\xad",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    // "あき"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8d",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
    // "あき"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8d",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    // "あき"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8d",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    // "あき"
    EXPECT_EQ(
        "\xe3\x81\x82\xe3\x81\x8d",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    // "あき"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8d",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    // "あき"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8d",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    // "あき"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8d",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    // "あき"
    EXPECT_EQ(
        "\xe3\x81\x82\xe3\x81\x8d",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    // "ｱｷ"
    EXPECT_EQ("\xef\xbd\xb1\xef\xbd\xb7",
              seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
}

TEST_F(TransliterationRewriterTest, MobileT13NTestWithQwertyHiragana) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_combine_all_segments(true);
  request.set_special_romanji_table(
      commands::Request::QWERTY_MOBILE_TO_HIRAGANA);

  ScopedRequestForUnittest scoped_request(request);

  // "し";
  const string kShi = "\xE3\x81\x97";
  composer::Table table;
  table.Initialize();

  {
    composer::Composer composer;
    composer.SetTable(&table);

    InsertASCIISequence("shi", &composer);
    string query;
    composer.GetQueryForConversion(&query);
    EXPECT_EQ(kShi, query);

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kShi);
    ConversionRequest request(&composer);
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ("shi", seg.meta_candidate(transliteration::HALF_ASCII).value);
  }

  {
    composer::Composer composer;
    composer.SetTable(&table);

    InsertASCIISequence("si", &composer);
    string query;
    composer.GetQueryForConversion(&query);
    EXPECT_EQ(kShi, query);

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kShi);
    ConversionRequest request(&composer);
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ("si", seg.meta_candidate(transliteration::HALF_ASCII).value);
  }
}
}  // namespace mozc

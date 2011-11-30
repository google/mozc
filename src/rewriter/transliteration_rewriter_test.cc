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
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "converter/segments.h"
#include "rewriter/transliteration_rewriter.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"
#include "transliteration/transliteration.h"

DECLARE_string(test_tmpdir);

namespace mozc {

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

void SetKamaboko(composer::Composer *composer) {
  InsertASCIISequence("kamabokonoinbou", composer);
  string query;
  composer->GetQueryForConversion(&query);
  // "かまぼこのいんぼう"
  EXPECT_EQ("\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae"
            "\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86",
            query);
}
}  // namespace

class TransliterationRewriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }
};

TEST_F(TransliterationRewriterTest, T13NFromKeyTest) {
  TransliterationRewriter t13n_rewriter;
  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  // "あかん"
  segment->set_key("\xe3\x81\x82\xe3\x81\x8b\xe3\x82\x93");
  EXPECT_EQ(0, segment->meta_candidates_size());
  EXPECT_TRUE(t13n_rewriter.Rewrite(&segments));
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
  TransliterationRewriter t13n_rewriter;

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);
  SetAkann(&composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  segments.set_composer(&composer);
  // "あかん"
  segment->set_key("\xe3\x81\x82\xe3\x81\x8b\xe3\x82\x93");
  EXPECT_TRUE(t13n_rewriter.Rewrite(&segments));
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
  TransliterationRewriter t13n_rewriter;

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);
  SetAkann(&composer);

  Segments segments;
  segments.set_composer(&composer);
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

  EXPECT_TRUE(t13n_rewriter.Rewrite(&segments));
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
  TransliterationRewriter t13n_rewriter;

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);
  SetAkann(&composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  segments.set_composer(&composer);
  // "かん"
  segment->set_key("\xe3\x81\x8b\xe3\x82\x93");
  EXPECT_TRUE(t13n_rewriter.Rewrite(&segments));
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
  TransliterationRewriter t13n_rewriter;

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);
  SetAkann(&composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  segments.set_composer(&composer);
  // "あかん"
  segment->set_key("\xe3\x81\x82\xe3\x81\x8b\xe3\x82\x93");
  EXPECT_TRUE(t13n_rewriter.Rewrite(&segments));
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

  EXPECT_TRUE(t13n_rewriter.Rewrite(&segments));

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
  TransliterationRewriter t13n_rewriter;

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  // "あ"
  segment->set_key("\xe3\x81\x82");
  segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("");  // void key

  EXPECT_TRUE(t13n_rewriter.Rewrite(&segments));
  EXPECT_EQ(2, segments.conversion_segments_size());
  EXPECT_NE(0, segments.conversion_segment(0).meta_candidates_size());
  EXPECT_EQ(0, segments.conversion_segment(1).meta_candidates_size());
}

TEST_F(TransliterationRewriterTest, NoKeyWithComposerTest) {
  TransliterationRewriter t13n_rewriter;

  composer::Table table;
  table.Initialize();
  composer::Composer composer;
  composer.SetTable(&table);
  InsertASCIISequence("a", &composer);

  Segments segments;
  segments.set_composer(&composer);
  Segment *segment = segments.add_segment();
  CHECK(segment);
  // "あ"
  segment->set_key("\xe3\x81\x82");
  segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("");  // void key

  EXPECT_TRUE(t13n_rewriter.Rewrite(&segments));
  EXPECT_EQ(2, segments.conversion_segments_size());
  EXPECT_NE(0, segments.conversion_segment(0).meta_candidates_size());
  EXPECT_EQ(0, segments.conversion_segment(1).meta_candidates_size());
}

TEST_F(TransliterationRewriterTest, NoRewriteTest) {
  TransliterationRewriter t13n_rewriter;

  composer::Table table;
  table.Initialize();

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  // "亜"
  segment->set_key("\xe4\xba\x9c");
  EXPECT_FALSE(t13n_rewriter.Rewrite(&segments));
  EXPECT_EQ(0, segments.conversion_segment(0).meta_candidates_size());
}

}  // namespace mozc

// Copyright 2010-2014, Google Inc.
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
#include <string>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#ifdef MOZC_USE_PACKED_DICTIONARY
#include "data_manager/packed/packed_data_manager.h"
#include "data_manager/packed/packed_data_mock.h"
#endif  // MOZC_USE_PACKED_DICTIONARY
#include "data_manager/user_pos_manager.h"
#include "dictionary/pos_matcher.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

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
}  // namespace

class TransliterationRewriterTest : public testing::Test {
 protected:
  // Workaround for C2512 error (no default appropriate constructor) on MSVS.
  TransliterationRewriterTest() {}
  virtual ~TransliterationRewriterTest() {}

  virtual void SetUp() {
    usage_stats::UsageStats::ClearAllStatsForTest();
#ifdef MOZC_USE_PACKED_DICTIONARY
    // Registers mocked PackedDataManager.
    scoped_ptr<packed::PackedDataManager>
        data_manager(new packed::PackedDataManager());
    CHECK(data_manager->Init(string(kPackedSystemDictionary_data,
                                    kPackedSystemDictionary_size)));
    packed::RegisterPackedDataManager(data_manager.release());
#endif  // MOZC_USE_PACKED_DICTIONARY
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
  }

  virtual void TearDown() {
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
#ifdef MOZC_USE_PACKED_DICTIONARY
    // Unregisters mocked PackedDataManager.
    packed::RegisterPackedDataManager(NULL);
#endif  // MOZC_USE_PACKED_DICTIONARY
    usage_stats::UsageStats::ClearAllStatsForTest();
  }

  TransliterationRewriter *CreateTransliterationRewriter() const {
    return new TransliterationRewriter(
        *UserPosManager::GetUserPosManager()->GetPOSMatcher());
  }

  const commands::Request &default_request() const {
    return default_request_;
  }

  const config::Config &default_config() const {
    return default_config_;
  }

  usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;

 private:
  const commands::Request default_request_;
  config::Config default_config_;
};

TEST_F(TransliterationRewriterTest, T13nFromKeyTest) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());
  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  // "あかん"
  segment->set_key("\xe3\x81\x82\xe3\x81\x8b\xe3\x82\x93");
  EXPECT_EQ(0, segment->meta_candidates_size());
  const ConversionRequest default_request;
  EXPECT_TRUE(t13n_rewriter->Rewrite(default_request, &segments));
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

TEST_F(TransliterationRewriterTest, T13nFromComposerTest) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.InitializeWithRequestAndConfig(default_request(), default_config());
  composer::Composer composer(&table, &default_request());
  SetAkann(&composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  ConversionRequest request(&composer, &default_request());
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


TEST_F(TransliterationRewriterTest, KeyOfT13nFromComposerTest) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.InitializeWithRequestAndConfig(default_request(), default_config());
  composer::Composer composer(&table, &default_request());
  InsertASCIISequence("ssh", &composer);

  Segments segments;
  segments.set_request_type(Segments::SUGGESTION);
  Segment *segment = segments.add_segment();
  CHECK(segment);

  commands::Request input;
  input.set_mixed_conversion(true);
  ConversionRequest request(&composer, &input);
  {
    // Although the segment key is "っ" as a partical string of the full
    // composition, the transliteration key should be "っsh" as the
    // whole composition string.

    // "っ"
    segment->set_key("\xE3\x81\xA3");
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    EXPECT_EQ(1, segments.conversion_segments_size());
    const Segment &seg = segments.conversion_segment(0);

    // "っｓｈ"
    EXPECT_EQ("\xE3\x81\xA3\xEF\xBD\x93\xEF\xBD\x88",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    // "っsh"
    EXPECT_EQ("\xE3\x81\xA3" "sh",
              seg.meta_candidate(transliteration::HIRAGANA).key);
    EXPECT_EQ("ssh", seg.meta_candidate(transliteration::HALF_ASCII).value);
    // "っsh"
    EXPECT_EQ("\xE3\x81\xA3" "sh",
              seg.meta_candidate(transliteration::HALF_ASCII).key);
  }
}


TEST_F(TransliterationRewriterTest, T13nWithMultiSegmentsTest) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  composer::Table table;
  table.InitializeWithRequestAndConfig(default_request(), default_config());
  composer::Composer composer(&table, &default_request());

  // Set kamabokoinbou to composer.
  {
    InsertASCIISequence("kamabokonoinbou", &composer);
    string query;
    composer.GetQueryForConversion(&query);
    // "かまぼこのいんぼう"
    EXPECT_EQ("\xE3\x81\x8B\xE3\x81\xBE\xE3\x81\xBC\xE3\x81\x93"
              "\xE3\x81\xAE\xE3\x81\x84\xE3\x82\x93\xE3\x81\xBC\xE3\x81\x86",
              query);
  }

  Segments segments;
  ConversionRequest request(&composer, &default_request());
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
  table.InitializeWithRequestAndConfig(default_request(), default_config());
  composer::Composer composer(&table, &default_request());

  // Set kan to composer.
  {
    InsertASCIISequence("kan", &composer);
    string query;
    composer.GetQueryForConversion(&query);
    // "かん"
    EXPECT_EQ("\xE3\x81\x8B\xE3\x82\x93", query);
  }

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  ConversionRequest request(&composer, &default_request());
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
  table.InitializeWithRequestAndConfig(default_request(), default_config());
  composer::Composer composer(&table, &default_request());
  SetAkann(&composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);
  ConversionRequest request(&composer, &default_request());
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
  table.InitializeWithRequestAndConfig(default_request(), default_config());
  composer::Composer composer(&table, &default_request());
  InsertASCIISequence("a", &composer);

  Segments segments;
  Segment *segment = segments.add_segment();
  CHECK(segment);

  ConversionRequest request(&composer, &default_request());

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
  table.InitializeWithRequestAndConfig(default_request(), default_config());

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
  table.InitializeWithRequestAndConfig(default_request(), default_config());
  composer::Composer composer(&table, &default_request());

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

  ConversionRequest request(&composer, &default_request());
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

  {
    input.set_mixed_conversion(true);
    const ConversionRequest request(NULL, &input);
    EXPECT_EQ(RewriterInterface::ALL, rewriter->capability(request));
  }

  {
    input.set_mixed_conversion(false);
    const ConversionRequest request(NULL, &input);
    EXPECT_EQ(RewriterInterface::CONVERSION, rewriter->capability(request));
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTestWith12KeysHiragana) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_combine_all_segments(true);
  request.set_special_romanji_table(
      commands::Request::TWELVE_KEYS_TO_HIRAGANA);

  composer::Table table;
  table.InitializeWithRequestAndConfig(request, default_config());
  composer::Composer composer(&table, &request);

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
  ConversionRequest rewrite_request(&composer, &request);
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

TEST_F(TransliterationRewriterTest, MobileT13nTestWith12KeysToNumber) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_combine_all_segments(true);
  request.set_special_romanji_table(
      commands::Request::TWELVE_KEYS_TO_HIRAGANA);

  composer::Table table;
  table.InitializeWithRequestAndConfig(request, default_config());
  composer::Composer composer(&table, &request);

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
  ConversionRequest rewrite_request(&composer, &request);
  EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments));

  // Because NoTransliteration attribute is specified in the mobile romaji
  // table's entries, raw-key based meta-candidates are not shown.
  {
    const Segment &seg = segments.conversion_segment(0);

    // "あかあか"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x82\xe3\x81\x8b",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    // "アカアカ"
    EXPECT_EQ("\xe3\x82\xa2\xe3\x82\xab\xe3\x82\xa2\xe3\x82\xab",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    // "あかあか"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x82\xe3\x81\x8b",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
    // "あかあか"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x82\xe3\x81\x8b",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    // "あかあか"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x82\xe3\x81\x8b",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    // "あかあか"
    EXPECT_EQ(
        "\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x82\xe3\x81\x8b",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    // "あかあか"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x82\xe3\x81\x8b",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    // "あかあか"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x82\xe3\x81\x8b",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    // "あかあか"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x82\xe3\x81\x8b",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    // "あかあか"
    EXPECT_EQ(
        "\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x82\xe3\x81\x8b",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    // "ｱｶｱｶ"
    EXPECT_EQ("\xef\xbd\xb1\xef\xbd\xb6\xef\xbd\xb1\xef\xbd\xb6",
              seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTestWith12KeysFlick) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_combine_all_segments(true);
  request.set_special_romanji_table(
      commands::Request::TOGGLE_FLICK_TO_HIRAGANA);

  composer::Table table;
  table.InitializeWithRequestAndConfig(request, default_config());
  composer::Composer composer(&table, &request);

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
  ConversionRequest rewrite_request(&composer, &request);
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

TEST_F(TransliterationRewriterTest, MobileT13nTestWithQwertyHiragana) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request client_request;
  client_request.set_zero_query_suggestion(true);
  client_request.set_mixed_conversion(true);
  client_request.set_combine_all_segments(true);
  client_request.set_special_romanji_table(
      commands::Request::QWERTY_MOBILE_TO_HIRAGANA);

  // "し";
  const string kShi = "\xE3\x81\x97";
  composer::Table table;
  table.InitializeWithRequestAndConfig(client_request, default_config());

  {
    composer::Composer composer(&table, &client_request);

    InsertASCIISequence("shi", &composer);
    string query;
    composer.GetQueryForConversion(&query);
    EXPECT_EQ(kShi, query);

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kShi);
    ConversionRequest request(&composer, &client_request);
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ("shi", seg.meta_candidate(transliteration::HALF_ASCII).value);
  }

  {
    composer::Composer composer(&table, &client_request);

    InsertASCIISequence("si", &composer);
    string query;
    composer.GetQueryForConversion(&query);
    EXPECT_EQ(kShi, query);

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kShi);
    ConversionRequest request(&composer, &client_request);
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ("si", seg.meta_candidate(transliteration::HALF_ASCII).value);
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTestWithGodan) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_combine_all_segments(true);
  request.set_special_romanji_table(commands::Request::GODAN_TO_HIRAGANA);

  composer::Table table;
  table.InitializeWithRequestAndConfig(request, default_config());
  composer::Composer composer(&table, &request);
  {
    InsertASCIISequence("<'de", &composer);
    string query;
    composer.GetQueryForConversion(&query);
    // "あん゜で"
    EXPECT_EQ("\xe3\x81\x82\xe3\x82\x93\xe3\x82\x9c\xe3\x81\xa7", query);
  }
  Segments segments;
  Segment *segment = segments.add_segment();
  // "あん゜で"
  segment->set_key("\xe3\x81\x82\xe3\x82\x93\xe3\x82\x9c\xe3\x81\xa7");
  ConversionRequest rewrite_request(&composer, &request);
  EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments));

  // Do not show raw keys.
  {
    const Segment &seg = segments.conversion_segment(0);

    // "あん゜で"
    EXPECT_EQ("\xe3\x81\x82\xe3\x82\x93\xe3\x82\x9c\xe3\x81\xa7",
              seg.meta_candidate(transliteration::HIRAGANA).value);
    // "アン゜デ"
    EXPECT_EQ("\xe3\x82\xa2\xe3\x83\xb3\xe3\x82\x9c\xe3\x83\x87",
              seg.meta_candidate(transliteration::FULL_KATAKANA).value);
    // "annde"
    EXPECT_EQ("annde",
              seg.meta_candidate(transliteration::HALF_ASCII).value);
    // "ANNDE"
    EXPECT_EQ("ANNDE",
              seg.meta_candidate(transliteration::HALF_ASCII_UPPER).value);
    // "annde"
    EXPECT_EQ("annde",
              seg.meta_candidate(transliteration::HALF_ASCII_LOWER).value);
    // "Annde"
    EXPECT_EQ(
        "Annde",
        seg.meta_candidate(transliteration::HALF_ASCII_CAPITALIZED).value);
    // "ａｎｎｄｅ"
    EXPECT_EQ("\xef\xbd\x81\xef\xbd\x8e\xef\xbd\x8e\xef\xbd\x84\xef\xbd\x85",
              seg.meta_candidate(transliteration::FULL_ASCII).value);
    // "ＡＮＮＤＥ"
    EXPECT_EQ("\xef\xbc\xa1\xef\xbc\xae\xef\xbc\xae\xef\xbc\xa4\xef\xbc\xa5",
              seg.meta_candidate(transliteration::FULL_ASCII_UPPER).value);
    // "ａｎｎｄｅ"
    EXPECT_EQ("\xef\xbd\x81\xef\xbd\x8e\xef\xbd\x8e\xef\xbd\x84\xef\xbd\x85",
              seg.meta_candidate(transliteration::FULL_ASCII_LOWER).value);
    // "Ａｎｎｄｅ"
    EXPECT_EQ(
        "\xef\xbc\xa1\xef\xbd\x8e\xef\xbd\x8e\xef\xbd\x84\xef\xbd\x85",
        seg.meta_candidate(transliteration::FULL_ASCII_CAPITALIZED).value);
    // "ｱﾝﾟﾃﾞ"
    EXPECT_EQ("\xef\xbd\xb1\xef\xbe\x9d\xef\xbe\x9f\xef\xbe\x83\xef\xbe\x9e",
              seg.meta_candidate(transliteration::HALF_KATAKANA).value);
  }
}

TEST_F(TransliterationRewriterTest, MobileT13nTest_ValidateGodanT13nTable) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_combine_all_segments(true);
  request.set_special_romanji_table(commands::Request::GODAN_TO_HIRAGANA);

  composer::Table table;
  table.InitializeWithRequestAndConfig(request, default_config());

  // Expected t13n of Godan keyboard.
  vector<const char *> keycode_to_t13n_map(
      128, static_cast<const char *>(NULL));
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

    composer::Composer composer(&table, &request);

    string ascii_input(1, static_cast<char>(i));
    InsertASCIISequence(ascii_input, &composer);
    string query;
    composer.GetQueryForConversion(&query);
    SCOPED_TRACE(
        Util::StringPrintf("char code = %d, ascii_input = %s, query = %s",
                           i, ascii_input.c_str(), query.c_str()));

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(query);
    ConversionRequest rewrite_request(&composer, &request);
    EXPECT_TRUE(t13n_rewriter->Rewrite(rewrite_request, &segments) ||
                query.empty());

    const Segment &seg = segments.conversion_segment(0);
    if (seg.meta_candidates_size() <= transliteration::HALF_ASCII) {
      // If no t13n happened, then should be no custom t13n.
      EXPECT_TRUE(keycode_to_t13n_map[i] == NULL ||
                  keycode_to_t13n_map[i] == string(""));
    } else {
      const string &half_ascii =
          seg.meta_candidate(transliteration::HALF_ASCII).value;
      if (keycode_to_t13n_map[i] && keycode_to_t13n_map[i] != string("")) {
        EXPECT_EQ(keycode_to_t13n_map[i], half_ascii);
      } else {
        EXPECT_TRUE(half_ascii == ascii_input ||
                    half_ascii == query);
      }
    }
  }
}

TEST_F(TransliterationRewriterTest, T13nOnSuggestion) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request client_request;
  client_request.set_mixed_conversion(true);

  // "っ"
  const string kXtsu = "\xE3\x81\xA3";

  composer::Table table;
  table.InitializeWithRequestAndConfig(client_request, default_config());
  {
    composer::Composer composer(&table, &client_request);

    InsertASCIISequence("ssh", &composer);
    string query;
    composer.GetQueryForPrediction(&query);
    EXPECT_EQ(kXtsu, query);

    Segments segments;
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    segment->set_key(kXtsu);
    ConversionRequest request(&composer, &client_request);
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ("ssh", seg.meta_candidate(transliteration::HALF_ASCII).value);
  }
}

TEST_F(TransliterationRewriterTest, T13nOnPartialSuggestion) {
  scoped_ptr<TransliterationRewriter> t13n_rewriter(
      CreateTransliterationRewriter());

  commands::Request client_request;
  client_request.set_mixed_conversion(true);

  // "っ"
  const string kXtsu = "\xE3\x81\xA3";

  composer::Table table;
  table.InitializeWithRequestAndConfig(client_request, default_config());
  {
    composer::Composer composer(&table, &client_request);

    InsertASCIISequence("ssh", &composer);  // "っsh|"
    string query;
    composer.GetQueryForPrediction(&query);
    EXPECT_EQ(kXtsu, query);

    composer.MoveCursorTo(1);  // "っ|sh"

    Segments segments;
    segments.set_request_type(Segments::PARTIAL_SUGGESTION);
    Segment *segment = segments.add_segment();
    segment->set_key(kXtsu);
    ConversionRequest request(&composer, &client_request);
    EXPECT_TRUE(t13n_rewriter->Rewrite(request, &segments));

    const Segment &seg = segments.conversion_segment(0);
    EXPECT_EQ("s", seg.meta_candidate(transliteration::HALF_ASCII).value);
  }
}
}  // namespace mozc

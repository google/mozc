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

#include "rewriter/language_aware_rewriter.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/memory/memory.h"

namespace mozc {
namespace {

using dictionary::DictionaryMock;
using dictionary::Token;

void InsertASCIISequence(const std::string &text,
                         composer::Composer *composer) {
  for (size_t i = 0; i < text.size(); ++i) {
    commands::KeyEvent key;
    key.set_key_code(text[i]);
    composer->InsertCharacterKeyEvent(key);
  }
}

}  // namespace

class LanguageAwareRewriterTest : public ::testing::Test {
 protected:
  // Workaround for C2512 error (no default appropriate constructor) on MSVS.
  LanguageAwareRewriterTest() {}
  ~LanguageAwareRewriterTest() override {}

  void SetUp() override {
    usage_stats::UsageStats::ClearAllStatsForTest();
    dictionary_mock_ = absl::make_unique<DictionaryMock>();
  }

  void TearDown() override {
    dictionary_mock_.reset();
    usage_stats::UsageStats::ClearAllStatsForTest();
  }

  LanguageAwareRewriter *CreateLanguageAwareRewriter() const {
    return new LanguageAwareRewriter(
        dictionary::POSMatcher(data_manager_.GetPOSMatcherData()),
        dictionary_mock_.get());
  }

  bool RewriteWithLanguageAwareInput(const LanguageAwareRewriter *rewriter,
                                     const std::string &key,
                                     std::string *composition,
                                     Segments *segments) {
    commands::Request client_request;
    client_request.set_language_aware_input(
        commands::Request::LANGUAGE_AWARE_SUGGESTION);

    composer::Table table;
    config::Config default_config;
    table.InitializeWithRequestAndConfig(client_request, default_config,
                                         data_manager_);

    composer::Composer composer(&table, &client_request, &default_config);
    InsertASCIISequence(key, &composer);
    composer.GetStringForPreedit(composition);

    // Perform the rewrite command.
    segments->set_request_type(Segments::SUGGESTION);
    if (segments->conversion_segments_size() == 0) {
      segments->add_segment();
    }
    Segment *segment = segments->mutable_conversion_segment(0);
    segment->set_key(*composition);
    ConversionRequest request(&composer, &client_request, &default_config);

    return rewriter->Rewrite(request, segments);
  }

  std::unique_ptr<DictionaryMock> dictionary_mock_;
  usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;

  const testing::MockDataManager data_manager_;

 private:
  const testing::ScopedTmpUserProfileDirectory tmp_profile_dir_;
};

namespace {

void PushFrontCandidate(const std::string &data, Segment *segment) {
  Segment::Candidate *candidate = segment->push_front_candidate();
  candidate->Init();
  candidate->value = data;
  candidate->key = data;
  candidate->content_value = data;
  candidate->content_key = data;
}
}  // namespace

TEST_F(LanguageAwareRewriterTest, LanguageAwareInput) {
  dictionary_mock_->AddLookupExact("house", "house", "house", Token::NONE);
  dictionary_mock_->AddLookupExact("query", "query", "query", Token::NONE);
  dictionary_mock_->AddLookupExact("google", "google", "google", Token::NONE);
  dictionary_mock_->AddLookupExact("naru", "naru", "naru", Token::NONE);
  dictionary_mock_->AddLookupExact("なる", "なる", "naru", Token::NONE);

  std::unique_ptr<LanguageAwareRewriter> rewriter(
      CreateLanguageAwareRewriter());

  const std::string &kPrefix = "→ ";
  const std::string &kDidYouMean = "もしかして";

  {
    // "python" is composed to "ｐｙてょｎ", but "python" should be suggested,
    // because alphabet characters are in the middle of the word.
    std::string composition;
    Segments segments;
    EXPECT_TRUE(RewriteWithLanguageAwareInput(rewriter.get(), "python",
                                              &composition, &segments));

    EXPECT_EQ("ｐｙてょｎ", composition);
    const Segment::Candidate &candidate =
        segments.conversion_segment(0).candidate(0);
    EXPECT_EQ("python", candidate.key);
    EXPECT_EQ("python", candidate.value);
    EXPECT_EQ(kPrefix, candidate.prefix);
    EXPECT_EQ(kDidYouMean, candidate.description);
  }

  {
    // "mozuk" is composed to "もずｋ", then "mozuk" is not suggested.
    // The tailing alphabet characters are not counted.
    std::string composition;
    Segments segments;
    EXPECT_FALSE(RewriteWithLanguageAwareInput(rewriter.get(), "mozuk",
                                               &composition, &segments));

    EXPECT_EQ("もずｋ", composition);
    EXPECT_EQ(0, segments.conversion_segment(0).candidates_size());
  }

  {
    // "house" is composed to "ほうせ".  Since "house" is in the dictionary
    // dislike the above "mozuk" case, "house" should be suggested.
    std::string composition;
    Segments segments;

    if (segments.conversion_segments_size() == 0) {
      segments.add_segment();
    }

    Segment *segment = segments.mutable_conversion_segment(0);
    // Add three candidates.
    // => ["cand0", "cand1", "cand2"]
    PushFrontCandidate("cand2", segment);
    PushFrontCandidate("cand1", segment);
    PushFrontCandidate("cand0", segment);
    EXPECT_EQ(3, segment->candidates_size());

    // "house" should be inserted as the 3rd candidate (b/w cand1 and cand2).
    // => ["cand0", "cand1", "house", "cand2"]
    EXPECT_TRUE(RewriteWithLanguageAwareInput(rewriter.get(), "house",
                                              &composition, &segments));
    EXPECT_EQ(4, segment->candidates_size());

    EXPECT_EQ("ほうせ", composition);
    const Segment::Candidate &candidate =
        segments.conversion_segment(0).candidate(2);
    EXPECT_EQ("house", candidate.key);
    EXPECT_EQ("house", candidate.value);
    EXPECT_EQ(kPrefix, candidate.prefix);
    EXPECT_EQ(kDidYouMean, candidate.description);
  }

  {
    // "query" is composed to "くえｒｙ".  Since "query" is in the dictionary
    // dislike the above "mozuk" case, "query" should be suggested.
    std::string composition;
    Segments segments;
    EXPECT_TRUE(RewriteWithLanguageAwareInput(rewriter.get(), "query",
                                              &composition, &segments));

    EXPECT_EQ("くえｒｙ", composition);
    const Segment::Candidate &candidate =
        segments.conversion_segment(0).candidate(0);
    EXPECT_EQ("query", candidate.key);
    EXPECT_EQ("query", candidate.value);
    EXPECT_EQ(kPrefix, candidate.prefix);
    EXPECT_EQ(kDidYouMean, candidate.description);
  }

  {
    // "google" is composed to "google" by mode_switching_handler.
    // If the suggestion is equal to the composition, that suggestion
    // is not added.
    std::string composition;
    Segments segments;
    EXPECT_FALSE(RewriteWithLanguageAwareInput(rewriter.get(), "google",
                                               &composition, &segments));
    EXPECT_EQ("google", composition);
  }

  {
    // The key "なる" has two value "naru" and "なる".
    // In this case, language aware rewriter should not be triggered.
    std::string composition;
    Segments segments;
    EXPECT_FALSE(RewriteWithLanguageAwareInput(rewriter.get(), "naru",
                                               &composition, &segments));

    EXPECT_EQ("なる", composition);
    EXPECT_EQ(0, segments.conversion_segment(0).candidates_size());
  }
}

TEST_F(LanguageAwareRewriterTest, LanguageAwareInputUsageStats) {
  std::unique_ptr<LanguageAwareRewriter> rewriter(
      CreateLanguageAwareRewriter());

  EXPECT_STATS_NOT_EXIST("LanguageAwareSuggestionTriggered");
  EXPECT_STATS_NOT_EXIST("LanguageAwareSuggestionCommitted");

  const std::string kPyTeyoN = "ｐｙてょｎ";

  {
    // "python" is composed to "ｐｙてょｎ", but "python" should be suggested,
    // because alphabet characters are in the middle of the word.
    std::string composition;
    Segments segments;
    EXPECT_TRUE(RewriteWithLanguageAwareInput(rewriter.get(), "python",
                                              &composition, &segments));
    EXPECT_EQ(kPyTeyoN, composition);

    const Segment::Candidate &candidate =
        segments.conversion_segment(0).candidate(0);
    EXPECT_EQ("python", candidate.key);
    EXPECT_EQ("python", candidate.value);

    EXPECT_COUNT_STATS("LanguageAwareSuggestionTriggered", 1);
    EXPECT_STATS_NOT_EXIST("LanguageAwareSuggestionCommitted");
  }

  {
    // Call Rewrite with "python" again, then call Finish.  Both ...Triggered
    // and ...Committed should be incremented.
    // Note, RewriteWithLanguageAwareInput is not used here, because
    // Finish also requires ConversionRequest.
    std::string composition;
    Segments segments;

    commands::Request client_request;
    client_request.set_language_aware_input(
        commands::Request::LANGUAGE_AWARE_SUGGESTION);

    composer::Table table;
    config::Config default_config;
    table.InitializeWithRequestAndConfig(client_request, default_config,
                                         data_manager_);

    composer::Composer composer(&table, &client_request, &default_config);
    InsertASCIISequence("python", &composer);
    composer.GetStringForPreedit(&composition);
    EXPECT_EQ(kPyTeyoN, composition);

    // Perform the rewrite command.
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    segment->set_key(composition);
    ConversionRequest request(&composer, &client_request, &default_config);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));

    EXPECT_COUNT_STATS("LanguageAwareSuggestionTriggered", 2);

    segment->set_segment_type(Segment::FIXED_VALUE);
    EXPECT_LT(0, segment->candidates_size());
    rewriter->Finish(request, &segments);
    EXPECT_COUNT_STATS("LanguageAwareSuggestionCommitted", 1);
  }
}

TEST_F(LanguageAwareRewriterTest, NotRewriteFullWidthAsciiToHalfWidthAscii) {
  std::unique_ptr<LanguageAwareRewriter> rewriter(
      CreateLanguageAwareRewriter());

  {
    // "1d*=" is composed to "１ｄ＊＝", which are the full width ascii
    // characters of "1d*=". We do not want to rewrite full width ascii to
    // half width ascii by LanguageAwareRewriter.
    std::string composition;
    Segments segments;
    EXPECT_FALSE(RewriteWithLanguageAwareInput(
        rewriter.get(), "1d*=", &composition, &segments));
    EXPECT_EQ("１ｄ＊＝", composition);
  }

  {
    // "xyzw" is composed to "ｘｙｚｗ". Do not rewrite.
    std::string composition;
    Segments segments;
    EXPECT_FALSE(RewriteWithLanguageAwareInput(rewriter.get(), "xyzw",
                                               &composition, &segments));
    EXPECT_EQ("ｘｙｚｗ", composition);
  }
}

TEST_F(LanguageAwareRewriterTest, IsDisabledInTwelveKeyLayout) {
  dictionary_mock_->AddLookupExact("query", "query", "query", Token::NONE);

  struct {
    commands::Request::SpecialRomanjiTable table;
    config::Config::PreeditMethod preedit_method;
    int type;
  } const kParams[] = {
      // Enabled combinations.
      {commands::Request::DEFAULT_TABLE, config::Config::ROMAN,
       RewriterInterface::SUGGESTION | RewriterInterface::PREDICTION},
      {commands::Request::QWERTY_MOBILE_TO_HIRAGANA, config::Config::ROMAN,
       RewriterInterface::SUGGESTION | RewriterInterface::PREDICTION},
      // Disabled combinations.
      {commands::Request::DEFAULT_TABLE, config::Config::KANA,
       RewriterInterface::NOT_AVAILABLE},
      {commands::Request::TWELVE_KEYS_TO_HIRAGANA, config::Config::ROMAN,
       RewriterInterface::NOT_AVAILABLE},
      {commands::Request::TOGGLE_FLICK_TO_HIRAGANA, config::Config::ROMAN,
       RewriterInterface::NOT_AVAILABLE},
      {commands::Request::GODAN_TO_HIRAGANA, config::Config::ROMAN,
       RewriterInterface::NOT_AVAILABLE},
  };

  std::unique_ptr<LanguageAwareRewriter> rewriter(
      CreateLanguageAwareRewriter());
  for (const auto &param : kParams) {
    commands::Request request;
    request.set_language_aware_input(
        commands::Request::LANGUAGE_AWARE_SUGGESTION);
    request.set_special_romanji_table(param.table);

    config::Config config;
    config.set_preedit_method(param.preedit_method);

    composer::Table table;
    table.InitializeWithRequestAndConfig(request, config, data_manager_);

    composer::Composer composer(&table, &request, &config);
    InsertASCIISequence("query", &composer);

    ConversionRequest conv_request(&composer, &request, &config);
    EXPECT_EQ(param.type, rewriter->capability(conv_request));
  }
}

}  // namespace mozc

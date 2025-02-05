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

#include <cstddef>
#include <string>

#include "absl/strings/string_view.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "converter/segments.h"
#include "converter/segments_matchers.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

namespace mozc {
namespace {

using ::mozc::dictionary::MockDictionary;
using ::mozc::dictionary::PosMatcher;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::StrEq;

void InsertASCIISequence(const absl::string_view text,
                         composer::Composer *composer) {
  for (size_t i = 0; i < text.size(); ++i) {
    commands::KeyEvent key;
    key.set_key_code(text[i]);
    composer->InsertCharacterKeyEvent(key);
  }
}

class LanguageAwareRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override { usage_stats::UsageStats::ClearAllStatsForTest(); }

  void TearDown() override { usage_stats::UsageStats::ClearAllStatsForTest(); }

  bool RewriteWithLanguageAwareInput(const LanguageAwareRewriter *rewriter,
                                     const absl::string_view key,
                                     bool is_mobile, std::string *composition,
                                     Segments *segments) {
    commands::Request client_request;
    client_request.set_language_aware_input(
        commands::Request::LANGUAGE_AWARE_SUGGESTION);

    if (is_mobile) {
      client_request.set_zero_query_suggestion(true);
      client_request.set_mixed_conversion(true);
    }

    composer::Table table;
    config::Config default_config;
    table.InitializeWithRequestAndConfig(client_request, default_config);

    composer::Composer composer(table, client_request, default_config);
    InsertASCIISequence(key, &composer);
    *composition = composer.GetStringForPreedit();

    // Perform the rewrite command.
    if (segments->conversion_segments_size() == 0) {
      segments->add_segment();
    }
    Segment *segment = segments->mutable_conversion_segment(0);
    segment->set_key(*composition);
    const ConversionRequest request =
        ConversionRequestBuilder()
            .SetComposer(composer)
            .SetRequest(client_request)
            .SetRequestType(ConversionRequest::SUGGESTION)
            .Build();

    return rewriter->Rewrite(request, segments);
  }

  usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
  const testing::MockDataManager data_manager_;
};

void PushFrontCandidate(const absl::string_view data, Segment *segment) {
  Segment::Candidate *candidate = segment->push_front_candidate();
  candidate->value = std::string(data);
  candidate->key = std::string(data);
  candidate->content_value = std::string(data);
  candidate->content_key = std::string(data);
}

void PushFrontCandidate(const absl::string_view data, int attributes,
                        Segment *segment) {
  PushFrontCandidate(data, segment);
  if (attributes) {
    segment->mutable_candidate(0)->attributes |= attributes;
  }
}

// A matcher for Segment::Candidate to test if a candidate has the given value.
constexpr auto ValueIs =
    [](const auto &matcher) -> Matcher<const Segment::Candidate *> {
  return Field(&Segment::Candidate::value, matcher);
};

// A matcher for Segment::Candidate to test if a candidate has the given value
// with "did you mean" annotation.
constexpr auto IsLangAwareCandidate =
    [](absl::string_view value) -> Matcher<const Segment::Candidate *> {
  return Pointee(AllOf(Field(&Segment::Candidate::key, value),
                       Field(&Segment::Candidate::value, value),
                       Field(&Segment::Candidate::prefix, "→ "),
                       Field(&Segment::Candidate::description, "もしかして")));
};

TEST_F(LanguageAwareRewriterTest, LanguageAwareInput) {
  MockDictionary dictionary;
  LanguageAwareRewriter rewriter(PosMatcher(data_manager_.GetPosMatcherData()),
                                 &dictionary);
  {
    // "python" is composed to "ｐｙてょｎ", but "python" should be suggested,
    // because alphabet characters are in the middle of the word.
    std::string composition;
    Segments segments;
    EXPECT_TRUE(RewriteWithLanguageAwareInput(&rewriter, "python", false,
                                              &composition, &segments));

    EXPECT_EQ(composition, "ｐｙてょｎ");
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_THAT(segments.conversion_segment(0),
                HasSingleCandidate(IsLangAwareCandidate("python")));
    Mock::VerifyAndClearExpectations(&dictionary);
  }
  {
    // On mobile, we insert the new candidate at the third position.
    std::string composition;
    Segments segments;
    Segment *segment = segments.push_back_segment();
    PushFrontCandidate("cand3", segment);
    PushFrontCandidate("cand2", segment);
    PushFrontCandidate("cand1", segment);
    PushFrontCandidate("cand0", segment);
    ASSERT_EQ(segment->candidates_size(), 4);
    EXPECT_TRUE(RewriteWithLanguageAwareInput(&rewriter, "python", true,
                                              &composition, &segments));
    EXPECT_EQ(composition, "ｐｙてょｎ");
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(2).value, "python");
    EXPECT_TRUE(segments.conversion_segment(0).candidate(2).prefix.empty());
    Mock::VerifyAndClearExpectations(&dictionary);
  }
  {
    // On mobile, the candidate is inserted after the typing correction.
    std::string composition;
    Segments segments;
    Segment *segment = segments.push_back_segment();
    PushFrontCandidate("cand4", segment);
    PushFrontCandidate("cand3", Segment::Candidate::TYPING_CORRECTION, segment);
    PushFrontCandidate("cand2", Segment::Candidate::TYPING_CORRECTION, segment);
    PushFrontCandidate("cand1", segment);
    PushFrontCandidate("cand0", segment);
    ASSERT_EQ(segment->candidates_size(), 5);
    EXPECT_TRUE(RewriteWithLanguageAwareInput(&rewriter, "python",
                                              true, /* is_mobile */
                                              &composition, &segments));
    EXPECT_EQ(composition, "ｐｙてょｎ");
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(4).value, "python");
    EXPECT_TRUE(segments.conversion_segment(0).candidate(4).prefix.empty());
    Mock::VerifyAndClearExpectations(&dictionary);
  }
  {
    // "mozuk" is composed to "もずｋ", then "mozuk" is not suggested.
    // The trailing alphabet characters are not counted.
    std::string composition;
    Segments segments;
    EXPECT_FALSE(RewriteWithLanguageAwareInput(
        &rewriter, "mozuk", false, /* is_mobile */ &composition, &segments));

    EXPECT_EQ(composition, "もずｋ");
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 0);
    Mock::VerifyAndClearExpectations(&dictionary);
  }
  {
    // "house" is composed to "ほうせ".  Since "house" is in the dictionary
    // unlike the above "mozuk" case, "house" should be suggested.
    std::string composition;
    Segments segments;
    Segment *segment = segments.push_back_segment();
    // Add three candidates: ["cand0", "cand1", "cand2"].
    PushFrontCandidate("cand2", segment);
    PushFrontCandidate("cand1", segment);
    PushFrontCandidate("cand0", segment);
    ASSERT_EQ(segment->candidates_size(), 3);

    // Set up the mock dictionary: "ほうせ" -> "house" doesn't exist but there's
    // an entry whose value is "house".
    EXPECT_CALL(dictionary, HasKey(StrEq("ほうせ"))).WillOnce(Return(false));
    EXPECT_CALL(dictionary, HasValue(StrEq("house"))).WillOnce(Return(true));

    // "house" should be inserted as the 3rd candidate (b/w cand1 and cand2).
    // => ["cand0", "cand1", "house", "cand2"]
    EXPECT_TRUE(RewriteWithLanguageAwareInput(
        &rewriter, "house", false, /* is_mobile */ &composition, &segments));
    EXPECT_EQ(composition, "ほうせ");
    EXPECT_THAT(*segment, CandidatesAreArray({
                              ValueIs("cand0"),
                              ValueIs("cand1"),
                              IsLangAwareCandidate("house"),
                              ValueIs("cand2"),
                          }));
    Mock::VerifyAndClearExpectations(&dictionary);
  }
  {
    // Set up the mock dictionary: there's an entry whose value is "house".
    EXPECT_CALL(dictionary, HasValue(StrEq("query"))).WillOnce(Return(true));

    // "query" is composed to "くえｒｙ".  Since "query" is in the dictionary
    // unlike the above "mozuk" case, "query" should be suggested.
    std::string composition;
    Segments segments;
    EXPECT_TRUE(RewriteWithLanguageAwareInput(
        &rewriter, "query", false, /* is_mobile */ &composition, &segments));

    EXPECT_EQ(composition, "くえｒｙ");
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_THAT(segments.conversion_segment(0),
                HasSingleCandidate(IsLangAwareCandidate("query")));
    Mock::VerifyAndClearExpectations(&dictionary);
  }
  {
    // "google" is composed to "google" by mode_switching_handler.
    // If the suggestion is equal to the composition, that suggestion
    // is not added.
    std::string composition;
    Segments segments;
    EXPECT_FALSE(RewriteWithLanguageAwareInput(&rewriter, "google",
                                               false, /* is_mobile */
                                               &composition, &segments));
    EXPECT_EQ(composition, "google");
    Mock::VerifyAndClearExpectations(&dictionary);
  }
  {
    // Set up the mock dictionary.
    EXPECT_CALL(dictionary, HasKey(StrEq("なる"))).WillRepeatedly(Return(true));
    EXPECT_CALL(dictionary, HasValue(StrEq("なる")))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(dictionary, HasValue(StrEq("naru")))
        .WillRepeatedly(Return(true));

    // The key "なる" has two value "naru" and "なる".
    // In this case, language aware rewriter should not be triggered.
    std::string composition;
    Segments segments;
    EXPECT_FALSE(RewriteWithLanguageAwareInput(
        &rewriter, "naru", false, /* is_mobile */ &composition, &segments));

    EXPECT_EQ(composition, "なる");
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 0);
    Mock::VerifyAndClearExpectations(&dictionary);
  }
}

TEST_F(LanguageAwareRewriterTest, LanguageAwareInputUsageStats) {
  MockDictionary dictionary;
  LanguageAwareRewriter rewriter(PosMatcher(data_manager_.GetPosMatcherData()),
                                 &dictionary);

  EXPECT_STATS_NOT_EXIST("LanguageAwareSuggestionTriggered");
  EXPECT_STATS_NOT_EXIST("LanguageAwareSuggestionCommitted");

  const std::string kPyTeyoN = "ｐｙてょｎ";

  {
    // "python" is composed to "ｐｙてょｎ", but "python" should be suggested,
    // because alphabet characters are in the middle of the word.
    std::string composition;
    Segments segments;
    EXPECT_TRUE(RewriteWithLanguageAwareInput(
        &rewriter, "python", false, /* is_mobile */ &composition, &segments));
    EXPECT_EQ(composition, kPyTeyoN);
    ASSERT_EQ(1, segments.conversion_segments_size());
    EXPECT_THAT(segments.conversion_segment(0),
                HasSingleCandidate(IsLangAwareCandidate("python")));
    EXPECT_COUNT_STATS("LanguageAwareSuggestionTriggered", 1);
    EXPECT_STATS_NOT_EXIST("LanguageAwareSuggestionCommitted");
    Mock::VerifyAndClearExpectations(&dictionary);
  }
  {
    // Call Rewrite with "python" again, then call Finish.  Both ...Triggered
    // and ...Committed should be incremented.
    // Note, RewriteWithLanguageAwareInput is not used here, because
    // Finish also requires ConversionRequest.
    commands::Request client_request;
    client_request.set_language_aware_input(
        commands::Request::LANGUAGE_AWARE_SUGGESTION);

    composer::Table table;
    config::Config default_config;
    table.InitializeWithRequestAndConfig(client_request, default_config);

    composer::Composer composer(table, client_request, default_config);
    InsertASCIISequence("python", &composer);
    const std::string composition = composer.GetStringForPreedit();
    EXPECT_EQ(composition, kPyTeyoN);

    // Perform the rewrite command.
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(composition);
    const ConversionRequest request =
        ConversionRequestBuilder()
            .SetComposer(composer)
            .SetRequestType(ConversionRequest::SUGGESTION)
            .Build();

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));

    EXPECT_COUNT_STATS("LanguageAwareSuggestionTriggered", 2);

    segment->set_segment_type(Segment::FIXED_VALUE);
    EXPECT_LT(0, segment->candidates_size());
    rewriter.Finish(request, &segments);
    EXPECT_COUNT_STATS("LanguageAwareSuggestionCommitted", 1);
  }
}

TEST_F(LanguageAwareRewriterTest, NotRewriteFullWidthAsciiToHalfWidthAscii) {
  MockDictionary dictionary;
  LanguageAwareRewriter rewriter(PosMatcher(data_manager_.GetPosMatcherData()),
                                 &dictionary);
  {
    // "1d*=" is composed to "１ｄ＊＝", which are the full width ascii
    // characters of "1d*=". We do not want to rewrite full width ascii to
    // half width ascii by LanguageAwareRewriter.
    std::string composition;
    Segments segments;
    EXPECT_FALSE(RewriteWithLanguageAwareInput(
        &rewriter, "1d*=", false, /* is_mobile */ &composition, &segments));
    EXPECT_EQ(composition, "１ｄ＊＝");
  }
  {
    // "xyzw" is composed to "ｘｙｚｗ". Do not rewrite.
    std::string composition;
    Segments segments;
    EXPECT_FALSE(RewriteWithLanguageAwareInput(
        &rewriter, "xyzw", false, /* is_mobile */ &composition, &segments));
    EXPECT_EQ(composition, "ｘｙｚｗ");
  }
}

TEST_F(LanguageAwareRewriterTest, IsDisabledInTwelveKeyLayout) {
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

  MockDictionary dictionary;
  LanguageAwareRewriter rewriter(PosMatcher(data_manager_.GetPosMatcherData()),
                                 &dictionary);
  for (const auto &param : kParams) {
    commands::Request request;
    request.set_language_aware_input(
        commands::Request::LANGUAGE_AWARE_SUGGESTION);
    request.set_special_romanji_table(param.table);

    config::Config config;
    config.set_preedit_method(param.preedit_method);

    composer::Table table;
    table.InitializeWithRequestAndConfig(request, config);

    composer::Composer composer(table, request, config);
    InsertASCIISequence("query", &composer);

    const commands::Context context;
    const ConversionRequest conv_request(composer, request, context, config,
                                         {});
    EXPECT_EQ(rewriter.capability(conv_request), param.type);
  }
}

}  // namespace
}  // namespace mozc

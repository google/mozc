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

#include "rewriter/language_aware_rewriter.h"

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
#include "dictionary/dictionary_mock.h"
#include "dictionary/pos_matcher.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"
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
}  // namespace

class LanguageAwareRewriterTest : public testing::Test {
 protected:
  // Workaround for C2512 error (no default appropriate constructor) on MSVS.
  LanguageAwareRewriterTest() {}
  virtual ~LanguageAwareRewriterTest() {}

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
    dictionary_mock_.reset(new DictionaryMock);
  }

  virtual void TearDown() {
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
#ifdef MOZC_USE_PACKED_DICTIONARY
    // Unregisters mocked PackedDataManager.
    packed::RegisterPackedDataManager(NULL);
#endif  // MOZC_USE_PACKED_DICTIONARY
    dictionary_mock_.reset(NULL);
    usage_stats::UsageStats::ClearAllStatsForTest();
  }

  LanguageAwareRewriter *CreateLanguageAwareRewriter() const {
    return new LanguageAwareRewriter(
        *UserPosManager::GetUserPosManager()->GetPOSMatcher(),
        dictionary_mock_.get());
  }

  scoped_ptr<DictionaryMock> dictionary_mock_;
  usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;

 private:
  config::Config default_config_;
};

namespace {
bool RewriteWithLanguageAwareInput(const LanguageAwareRewriter *rewriter,
                                   const string &key,
                                   string *composition,
                                   Segments *segments) {
  commands::Request client_request;
  client_request.set_language_aware_input(
      commands::Request::LANGUAGE_AWARE_SUGGESTION);

  composer::Table table;
  config::Config default_config;
  table.InitializeWithRequestAndConfig(client_request, default_config);

  composer::Composer composer(&table, &client_request);
  InsertASCIISequence(key, &composer);
  composer.GetStringForPreedit(composition);

  // Perform the rewrite command.
  segments->set_request_type(Segments::SUGGESTION);
  if (segments->conversion_segments_size() == 0) {
    segments->add_segment();
  }
  Segment *segment = segments->mutable_conversion_segment(0);
  segment->set_key(*composition);
  ConversionRequest request(&composer, &client_request);

  return rewriter->Rewrite(request, segments);
}

void PushFrontCandidate(const string &data, Segment *segment) {
  Segment::Candidate *candidate = segment->push_front_candidate();
  candidate->Init();
  candidate->value = data;
  candidate->key = data;
  candidate->content_value = data;
  candidate->content_key = data;
}
}  // namespace

TEST_F(LanguageAwareRewriterTest, LanguageAwareInput) {
  dictionary_mock_->AddLookupExact("house", "house", "house", 0);
  dictionary_mock_->AddLookupExact("query", "query", "query", 0);
  dictionary_mock_->AddLookupExact("google", "google", "google", 0);

  scoped_ptr<LanguageAwareRewriter> rewriter(CreateLanguageAwareRewriter());

  const string &kPrefix = "\xE2\x86\x92 ";  // "→ "
  const string &kDidYouMean =
      // "もしかして"
      "\xE3\x82\x82\xE3\x81\x97\xE3\x81\x8B\xE3\x81\x97\xE3\x81\xA6";

  {
    // "python" is composed to "ｐｙてょｎ", but "python" should be suggested,
    // because alphabet characters are in the middle of the word.
    string composition;
    Segments segments;
    EXPECT_TRUE(RewriteWithLanguageAwareInput(rewriter.get(), "python",
                                              &composition, &segments));

    // "ｐｙてょｎ"
    EXPECT_EQ("\xEF\xBD\x90\xEF\xBD\x99\xE3\x81\xA6\xE3\x82\x87\xEF\xBD\x8E",
              composition);
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
    string composition;
    Segments segments;
    EXPECT_FALSE(RewriteWithLanguageAwareInput(rewriter.get(), "mozuk",
                                               &composition, &segments));

    // "もずｋ"
    EXPECT_EQ("\xE3\x82\x82\xE3\x81\x9A\xEF\xBD\x8B", composition);
    EXPECT_EQ(0, segments.conversion_segment(0).candidates_size());
  }

  {
    // "house" is composed to "ほうせ".  Since "house" is in the dictionary
    // dislike the above "mozuk" case, "house" should be suggested.
    string composition;
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

    // "ほうせ"
    EXPECT_EQ("\xE3\x81\xBB\xE3\x81\x86\xE3\x81\x9B", composition);
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
    string composition;
    Segments segments;
    EXPECT_TRUE(RewriteWithLanguageAwareInput(rewriter.get(), "query",
                                              &composition, &segments));

    // "くえｒｙ"
    EXPECT_EQ("\xE3\x81\x8F\xE3\x81\x88\xEF\xBD\x92\xEF\xBD\x99", composition);
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
    string composition;
    Segments segments;
    EXPECT_FALSE(RewriteWithLanguageAwareInput(rewriter.get(), "google",
                                               &composition, &segments));
    EXPECT_EQ("google", composition);
  }
}

TEST_F(LanguageAwareRewriterTest, LanguageAwareInputUsageStats) {
  scoped_ptr<LanguageAwareRewriter> rewriter(CreateLanguageAwareRewriter());

  EXPECT_STATS_NOT_EXIST("LanguageAwareSuggestionTriggered");
  EXPECT_STATS_NOT_EXIST("LanguageAwareSuggestionCommitted");

  const string kPyTeyoN =
      // "ｐｙてょｎ"
      "\xEF\xBD\x90\xEF\xBD\x99\xE3\x81\xA6\xE3\x82\x87\xEF\xBD\x8E";

  {
    // "python" is composed to "ｐｙてょｎ", but "python" should be suggested,
    // because alphabet characters are in the middle of the word.
    string composition;
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
    string composition;
    Segments segments;

    commands::Request client_request;
    client_request.set_language_aware_input(
        commands::Request::LANGUAGE_AWARE_SUGGESTION);

    composer::Table table;
    config::Config default_config;
    table.InitializeWithRequestAndConfig(client_request, default_config);

    composer::Composer composer(&table, &client_request);
    InsertASCIISequence("python", &composer);
    composer.GetStringForPreedit(&composition);
    EXPECT_EQ(kPyTeyoN, composition);

    // Perform the rewrite command.
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    segment->set_key(composition);
    ConversionRequest request(&composer, &client_request);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));

    EXPECT_COUNT_STATS("LanguageAwareSuggestionTriggered", 2);

    segment->set_segment_type(Segment::FIXED_VALUE);
    EXPECT_LT(0, segment->candidates_size());
    rewriter->Finish(request, &segments);
    EXPECT_COUNT_STATS("LanguageAwareSuggestionCommitted", 1);
  }
}

}  // namespace mozc

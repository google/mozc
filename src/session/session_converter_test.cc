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

// Tests for session converter.
//
// Note that we have a lot of tests which assume that the converter fills
// T13Ns. If you want to add test case related to T13Ns, please make sure
// you set T13Ns to the result for a mock converter.

#include "session/session_converter.h"

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/internal/candidate_list.h"
#include "session/internal/keymap.h"
#include "session/request_test_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/testing_util.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"

namespace mozc {
namespace session {

using ::mozc::commands::Context;
using ::mozc::commands::Request;
using ::mozc::commands::RequestForUnitTest;
using ::mozc::config::Config;

static constexpr char kChars_Aiueo[] = "あいうえお";
static constexpr char kChars_Mo[] = "も";
static constexpr char kChars_Mozuku[] = "もずく";
static constexpr char kChars_Mozukusu[] = "もずくす";
static constexpr char kChars_Momonga[] = "ももんが";

class SessionConverterTest : public ::testing::Test {
 protected:
  // Workaround for C2512 error (no default appropriate constructor) on MSVS.
  SessionConverterTest() {}
  ~SessionConverterTest() override {}

  void SetUp() override {
    convertermock_ = absl::make_unique<ConverterMock>();
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();

    config_ = absl::make_unique<Config>();
    config_->set_use_cascading_window(true);
    request_ = absl::make_unique<Request>();

    table_ = absl::make_unique<composer::Table>();
    table_->InitializeWithRequestAndConfig(*request_, *config_,
                                           mock_data_manager_);
    composer_ = absl::make_unique<composer::Composer>(
        table_.get(), request_.get(), config_.get());
  }

  void TearDown() override {
    table_.reset();
    composer_.reset();

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  static void GetSegments(const SessionConverter &converter, Segments *dest) {
    CHECK(dest);
    dest->CopyFrom(*converter.segments_);
  }

  static void SetSegments(const Segments &src, SessionConverter *converter) {
    CHECK(converter);
    converter->segments_->CopyFrom(src);
  }

  static const commands::Result &GetResult(const SessionConverter &converter) {
    return *converter.result_;
  }

  static const CandidateList &GetCandidateList(
      const SessionConverter &converter) {
    return *converter.candidate_list_;
  }

  static SessionConverterInterface::State GetState(
      const SessionConverter &converter) {
    return converter.state_;
  }

  static void SetState(SessionConverterInterface::State state,
                       SessionConverter *converter) {
    converter->state_ = state;
  }

  static size_t GetSegmentIndex(const SessionConverter &converter) {
    return converter.segment_index_;
  }

  static bool IsCandidateListVisible(const SessionConverter &converter) {
    return converter.candidate_list_visible_;
  }

  static const commands::Request &GetRequest(
      const SessionConverter &converter) {
    return *converter.request_;
  }

  static void GetPreedit(const SessionConverter &converter, size_t index,
                         size_t size, std::string *conversion) {
    converter.GetPreedit(index, size, conversion);
  }

  static void GetConversion(const SessionConverter &converter, size_t index,
                            size_t size, std::string *conversion) {
    converter.GetConversion(index, size, conversion);
  }

  static void AppendCandidateList(SessionConverter *converter) {
    converter->AppendCandidateList();
  }

  // set result for "あいうえお"
  static void SetAiueo(Segments *segments) {
    segments->Clear();
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments->add_segment();
    segment->set_key("あいうえお");
    candidate = segment->add_candidate();
    candidate->key = "あいうえお";
    candidate->value = candidate->key;
    candidate = segment->add_candidate();
    candidate->key = "あいうえお";
    candidate->value = "アイウエオ";
  }

  // set result for "かまぼこのいんぼう"
  static void SetKamaboko(Segments *segments) {
    Segment *segment;
    Segment::Candidate *candidate;

    segments->Clear();
    segment = segments->add_segment();

    segment->set_key("かまぼこの");
    candidate = segment->add_candidate();
    candidate->value = "かまぼこの";
    candidate = segment->add_candidate();
    candidate->value = "カマボコの";
    segment = segments->add_segment();
    segment->set_key("いんぼう");
    candidate = segment->add_candidate();
    candidate->value = "陰謀";
    candidate = segment->add_candidate();
    candidate->value = "印房";

    // Set dummy T13Ns
    std::vector<Segment::Candidate> *meta_candidates =
        segment->mutable_meta_candidates();
    meta_candidates->resize(transliteration::NUM_T13N_TYPES);
    for (size_t i = 0; i < transliteration::NUM_T13N_TYPES; ++i) {
      meta_candidates->at(i).Init();
      meta_candidates->at(i).value = segment->key();
      meta_candidates->at(i).content_value = segment->key();
      meta_candidates->at(i).content_key = segment->key();
    }
  }

  // set T13N candidates to segments using composer
  static void FillT13Ns(Segments *segments,
                        const composer::Composer *composer) {
    size_t composition_pos = 0;
    for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
      Segment *segment = segments->mutable_conversion_segment(i);
      CHECK(segment);
      const size_t composition_len = Util::CharsLen(segment->key());
      std::vector<std::string> t13ns;
      composer->GetSubTransliterations(composition_pos, composition_len,
                                       &t13ns);
      std::vector<Segment::Candidate> *meta_candidates =
          segment->mutable_meta_candidates();
      meta_candidates->resize(transliteration::NUM_T13N_TYPES);
      for (size_t j = 0; j < transliteration::NUM_T13N_TYPES; ++j) {
        meta_candidates->at(j).Init();
        meta_candidates->at(j).value = t13ns[j];
        meta_candidates->at(j).content_value = t13ns[j];
        meta_candidates->at(j).content_key = segment->key();
      }
      composition_pos += composition_len;
    }
  }

  // set result for "like"
  void InitConverterWithLike(Segments *segments) {
    composer_->InsertCharacterKeyAndPreedit("li", "ぃ");
    composer_->InsertCharacterKeyAndPreedit("ke", "け");

    Segment *segment;
    Segment::Candidate *candidate;

    segments->Clear();
    segment = segments->add_segment();

    segment->set_key("ぃ");
    candidate = segment->add_candidate();
    candidate->value = "ぃ";

    candidate = segment->add_candidate();
    candidate->value = "ィ";

    segment = segments->add_segment();
    segment->set_key("け");
    candidate = segment->add_candidate();
    candidate->value = "家";
    candidate = segment->add_candidate();
    candidate->value = "け";

    FillT13Ns(segments, composer_.get());
    convertermock_->SetStartConversionForRequest(segments, true);
  }

  static void InsertASCIISequence(const std::string text,
                                  composer::Composer *composer) {
    for (size_t i = 0; i < text.size(); ++i) {
      commands::KeyEvent key;
      key.set_key_code(text[i]);
      composer->InsertCharacterKeyEvent(key);
    }
  }

  static void ExpectSameSessionConverter(const SessionConverter &lhs,
                                         const SessionConverter &rhs) {
    EXPECT_EQ(lhs.IsActive(), rhs.IsActive());
    EXPECT_EQ(IsCandidateListVisible(lhs), IsCandidateListVisible(rhs));
    EXPECT_EQ(GetSegmentIndex(lhs), GetSegmentIndex(rhs));

    EXPECT_EQ(lhs.conversion_preferences().use_history,
              rhs.conversion_preferences().use_history);
    EXPECT_EQ(lhs.conversion_preferences().max_history_size,
              rhs.conversion_preferences().max_history_size);
    EXPECT_EQ(IsCandidateListVisible(lhs), IsCandidateListVisible(rhs));

    Segments segments_lhs, segments_rhs;
    GetSegments(lhs, &segments_lhs);
    GetSegments(rhs, &segments_rhs);
    EXPECT_EQ(segments_lhs.segments_size(), segments_rhs.segments_size());
    for (size_t i = 0; i < segments_lhs.segments_size(); ++i) {
      Segment segment_lhs, segment_rhs;
      segment_lhs.CopyFrom(segments_lhs.segment(i));
      segment_rhs.CopyFrom(segments_rhs.segment(i));
      EXPECT_EQ(segment_lhs.key(), segment_rhs.key()) << " i=" << i;
      EXPECT_EQ(segment_lhs.segment_type(), segment_rhs.segment_type())
          << " i=" << i;
      EXPECT_EQ(segment_lhs.candidates_size(), segment_rhs.candidates_size());
    }

    const CandidateList &candidate_list_lhs = GetCandidateList(lhs);
    const CandidateList &candidate_list_rhs = GetCandidateList(rhs);
    EXPECT_EQ(candidate_list_lhs.name(), candidate_list_rhs.name());
    EXPECT_EQ(candidate_list_lhs.page_size(), candidate_list_rhs.page_size());
    EXPECT_EQ(candidate_list_lhs.size(), candidate_list_rhs.size());
    EXPECT_EQ(candidate_list_lhs.last_index(), candidate_list_rhs.last_index());
    EXPECT_EQ(candidate_list_lhs.focused_id(), candidate_list_rhs.focused_id());
    EXPECT_EQ(candidate_list_lhs.focused_index(),
              candidate_list_rhs.focused_index());
    EXPECT_EQ(candidate_list_lhs.focused(), candidate_list_rhs.focused());

    for (int i = 0; i < candidate_list_lhs.size(); ++i) {
      const Candidate &candidate_lhs = candidate_list_lhs.candidate(i);
      const Candidate &candidate_rhs = candidate_list_rhs.candidate(i);
      EXPECT_EQ(candidate_lhs.id(), candidate_rhs.id());
      EXPECT_EQ(candidate_lhs.attributes(), candidate_rhs.attributes());
      EXPECT_EQ(candidate_lhs.IsSubcandidateList(),
                candidate_rhs.IsSubcandidateList());
      if (candidate_lhs.IsSubcandidateList()) {
        EXPECT_EQ(candidate_lhs.subcandidate_list().size(),
                  candidate_rhs.subcandidate_list().size());
      }
    }

    EXPECT_PROTO_EQ(GetResult(lhs), GetResult(rhs));
    EXPECT_PROTO_EQ(GetRequest(lhs), GetRequest(rhs));
  }

  static ::testing::AssertionResult ExpectSelectedCandidateIndices(
      const char *, const char *, const SessionConverter &converter,
      const std::vector<int> &expected) {
    const std::vector<int> &actual = converter.selected_candidate_indices_;

    if (expected.size() != actual.size()) {
      return ::testing::AssertionFailure()
             << "Indices size mismatch.\n"
             << "Expected: " << expected.size() << "\n"
             << "Actual:   " << actual.size();
    }

    for (size_t i = 0; i < expected.size(); ++i) {
      if (expected[i] != actual[i]) {
        return ::testing::AssertionFailure()
               << "Index mismatch.\n"
               << "Expected: " << expected[i] << "\n"
               << "Actual:   " << actual[i];
      }
    }

    return ::testing::AssertionSuccess();
  }

  static void SetCommandCandidate(Segments *segments, int segment_index,
                                  int candidate_index,
                                  Segment::Candidate::Command command) {
    segments->mutable_conversion_segment(segment_index)
        ->mutable_candidate(candidate_index)
        ->attributes |= Segment::Candidate::COMMAND_CANDIDATE;
    segments->mutable_conversion_segment(segment_index)
        ->mutable_candidate(candidate_index)
        ->command = command;
  }

  std::unique_ptr<ConverterMock> convertermock_;

  std::unique_ptr<composer::Composer> composer_;
  std::unique_ptr<composer::Table> table_;
  std::unique_ptr<Request> request_;
  std::unique_ptr<Config> config_;
  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;

 private:
  const testing::MockDataManager mock_data_manager_;
};

#define EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, indices) \
  EXPECT_PRED_FORMAT2(ExpectSelectedCandidateIndices, converter, indices);

TEST_F(SessionConverterTest, Convert) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionForRequest(&segments, true);
  std::vector<int> expected_indices;
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  composer_->InsertCharacterPreedit(kChars_Aiueo);
  EXPECT_TRUE(converter.Convert(*composer_));
  ASSERT_TRUE(converter.IsActive());
  expected_indices.push_back(0);
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  commands::Output output;
  converter.FillOutput(*composer_, &output);
  EXPECT_FALSE(output.has_result());
  EXPECT_TRUE(output.has_preedit());
  EXPECT_FALSE(output.has_candidates());

  const commands::Preedit &conversion = output.preedit();
  EXPECT_EQ(1, conversion.segment_size());
  EXPECT_EQ(commands::Preedit::Segment::HIGHLIGHT,
            conversion.segment(0).annotation());
  EXPECT_EQ(kChars_Aiueo, conversion.segment(0).value());
  EXPECT_EQ(kChars_Aiueo, conversion.segment(0).key());

  // Converter should be active before submittion
  EXPECT_TRUE(converter.IsActive());
  EXPECT_FALSE(IsCandidateListVisible(converter));

  converter.Commit(*composer_, Context::default_instance());
  composer_->Reset();
  output.Clear();
  converter.FillOutput(*composer_, &output);
  EXPECT_TRUE(output.has_result());
  EXPECT_FALSE(output.has_preedit());
  EXPECT_FALSE(output.has_candidates());
  expected_indices.clear();
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  const commands::Result &result = output.result();
  EXPECT_EQ(kChars_Aiueo, result.value());
  EXPECT_EQ(kChars_Aiueo, result.key());

  // Converter should be inactive after submittion
  EXPECT_FALSE(converter.IsActive());
  EXPECT_FALSE(IsCandidateListVisible(converter));

  EXPECT_COUNT_STATS("Commit", 1);
  EXPECT_COUNT_STATS("CommitFromConversion", 1);
  EXPECT_COUNT_STATS("ConversionCandidates0", 1);
}

TEST_F(SessionConverterTest, ConvertWithSpellingCorrection) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(&segments, composer_.get());
  segments.mutable_conversion_segment(0)->mutable_candidate(0)->attributes |=
      Segment::Candidate::SPELLING_CORRECTION;
  convertermock_->SetStartConversionForRequest(&segments, true);

  composer_->InsertCharacterPreedit(kChars_Aiueo);
  EXPECT_TRUE(converter.Convert(*composer_));
  ASSERT_TRUE(converter.IsActive());
  EXPECT_TRUE(IsCandidateListVisible(converter));
}

TEST_F(SessionConverterTest, ConvertToTransliteration) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  SetAiueo(&segments);

  composer_->InsertCharacterKeyAndPreedit("aiueo", kChars_Aiueo);
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionForRequest(&segments, true);

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HALF_ASCII));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  {  // Check the conversion #1
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("aiueo", conversion.segment(0).value());
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HALF_ASCII));
  {  // Check the conversion #2
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("AIUEO", conversion.segment(0).value());
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #3
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("ＡＩＵＥＯ", conversion.segment(0).value());
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  converter.Commit(*composer_, Context::default_instance());

  EXPECT_COUNT_STATS("Commit", 1);
  EXPECT_COUNT_STATS("CommitFromConversion", 1);
  EXPECT_COUNT_STATS("ConversionCandidates0", 1);
}

TEST_F(SessionConverterTest, ConvertToTransliterationWithMultipleSegments) {
  Segments segments;
  InitConverterWithLike(&segments);
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());

  // Convert
  EXPECT_TRUE(converter.Convert(*composer_));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  expected_indices.push_back(0);
  {  // Check the conversion #1
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(2, conversion.segment_size());
    EXPECT_EQ("ぃ", conversion.segment(0).value());
    EXPECT_EQ("家", conversion.segment(1).value());
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  // Convert to half-width alphanumeric.
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HALF_ASCII));
  {  // Check the conversion #2
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(2, conversion.segment_size());
    EXPECT_EQ("li", conversion.segment(0).value());
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }
}

TEST_F(SessionConverterTest, ConvertToTransliterationWithoutCascadigWindow) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  {
    Segment *segment;
    Segment::Candidate *candidate;
    segment = segments.add_segment();
    segment->set_key("dvd");
    candidate = segment->add_candidate();
    candidate->value = "dvd";
    candidate = segment->add_candidate();
    candidate->value = "DVD";
  }
  {  // Set OperationPreferences
    converter.set_use_cascading_window(false);
    converter.set_selection_shortcut(config::Config::NO_SHORTCUT);
  }

  composer_->InsertCharacterKeyAndPreedit("dvd", "ｄｖｄ");
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionForRequest(&segments, true);
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  {  // Check the conversion #1
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("ｄｖｄ", conversion.segment(0).value());
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #2
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("ＤＶＤ", conversion.segment(0).value());
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #3
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("Ｄｖｄ", conversion.segment(0).value());
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }
}

TEST_F(SessionConverterTest, MultiSegmentsConversion) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  SetKamaboko(&segments);
  const std::string kKamabokono = "かまぼこの";
  const std::string kInbou = "いんぼう";

  // Test for conversion
  composer_->InsertCharacterPreedit(kKamabokono + kInbou);
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionForRequest(&segments, true);
  EXPECT_TRUE(converter.Convert(*composer_));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  expected_indices.push_back(0);
  {
    EXPECT_EQ(0, GetSegmentIndex(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(2, conversion.segment_size());
    EXPECT_EQ(commands::Preedit::Segment::HIGHLIGHT,
              conversion.segment(0).annotation());
    EXPECT_EQ(kKamabokono, conversion.segment(0).key());
    EXPECT_EQ(kKamabokono, conversion.segment(0).value());

    EXPECT_EQ(commands::Preedit::Segment::UNDERLINE,
              conversion.segment(1).annotation());
    EXPECT_EQ(kInbou, conversion.segment(1).key());
    EXPECT_EQ("陰謀", conversion.segment(1).value());
  }

  // Test for candidates [CandidateNext]
  EXPECT_FALSE(IsCandidateListVisible(converter));
  converter.CandidateNext(*composer_);
  expected_indices[0] += 1;
  {
    EXPECT_TRUE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  // Test for candidates [CandidatePrev]
  converter.CandidatePrev();
  expected_indices[0] -= 1;
  {
    EXPECT_TRUE(IsCandidateListVisible(converter));
    EXPECT_EQ(0, GetSegmentIndex(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(3, candidates.size());  // two candidates + one t13n sub list.
    EXPECT_EQ(0, candidates.position());
    EXPECT_EQ(kKamabokono, candidates.candidate(0).value());
    EXPECT_EQ("カマボコの", candidates.candidate(1).value());
    EXPECT_EQ("そのほかの文字種", candidates.candidate(2).value());
  }

  // Test for segment motion. [SegmentFocusRight]
  converter.SegmentFocusRight();
  {
    EXPECT_EQ(1, GetSegmentIndex(converter));
    EXPECT_FALSE(IsCandidateListVisible(converter));
    converter.SetCandidateListVisible(true);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(0, candidates.focused_index());
    EXPECT_EQ(3, candidates.size());  // two candidates + one t13n sub list.
    EXPECT_EQ(5, candidates.position());
    EXPECT_EQ("陰謀", candidates.candidate(0).value());
    EXPECT_EQ("印房", candidates.candidate(1).value());
    EXPECT_EQ("そのほかの文字種", candidates.candidate(2).value());
  }

  // Test for segment motion. [SegmentFocusLeft]
  converter.SegmentFocusLeft();
  {
    EXPECT_EQ(0, GetSegmentIndex(converter));
    EXPECT_FALSE(IsCandidateListVisible(converter));
    converter.SetCandidateListVisible(true);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(0, candidates.focused_index());
    EXPECT_EQ(3, candidates.size());  // two candidates + one t13n sub list.
    EXPECT_EQ(0, candidates.position());
    EXPECT_EQ(kKamabokono, candidates.candidate(0).value());
    EXPECT_EQ("カマボコの", candidates.candidate(1).value());
    EXPECT_EQ("そのほかの文字種", candidates.candidate(2).value());
  }

  // Test for segment motion. [SegmentFocusLeft] at the head of segments.
  // http://b/2990134
  // Focus changing at the tail of segments to right,
  // and at the head of segments to left, should work.
  converter.SegmentFocusLeft();
  {
    EXPECT_EQ(1, GetSegmentIndex(converter));
    EXPECT_FALSE(IsCandidateListVisible(converter));
    converter.SetCandidateListVisible(true);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(0, candidates.focused_index());
    EXPECT_EQ(3, candidates.size());  // two candidates + one t13n sub list.
    EXPECT_EQ(5, candidates.position());
    EXPECT_EQ("陰謀", candidates.candidate(0).value());
    EXPECT_EQ("印房", candidates.candidate(1).value());
    EXPECT_EQ("そのほかの文字種", candidates.candidate(2).value());
  }

  // Test for segment motion. [SegmentFocusRight] at the tail of segments.
  // http://b/2990134
  // Focus changing at the tail of segments to right,
  // and at the head of segments to left, should work.
  converter.SegmentFocusRight();
  {
    EXPECT_FALSE(IsCandidateListVisible(converter));
    converter.SetCandidateListVisible(true);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    commands::Output output;
    EXPECT_EQ(0, GetSegmentIndex(converter));
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(0, candidates.focused_index());
    EXPECT_EQ(3, candidates.size());  // two candidates + one t13n sub list.
    EXPECT_EQ(0, candidates.position());
    EXPECT_EQ(kKamabokono, candidates.candidate(0).value());
    EXPECT_EQ("カマボコの", candidates.candidate(1).value());
    EXPECT_EQ("そのほかの文字種", candidates.candidate(2).value());
  }

  // Test for candidate motion. [CandidateNext]
  converter.SegmentFocusRight();  // Focus to the last segment.
  EXPECT_EQ(1, GetSegmentIndex(converter));
  converter.CandidateNext(*composer_);
  expected_indices[1] += 1;
  {
    EXPECT_TRUE(IsCandidateListVisible(converter));
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(1, candidates.focused_index());
    EXPECT_EQ(3, candidates.size());  // two candidates + one t13n sub list.
    EXPECT_EQ(5, candidates.position());
    EXPECT_EQ("陰謀", candidates.candidate(0).value());
    EXPECT_EQ("印房", candidates.candidate(1).value());
    EXPECT_EQ("そのほかの文字種", candidates.candidate(2).value());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(kKamabokono, conversion.segment(0).value());
    EXPECT_EQ("印房", conversion.segment(1).value());
  }

  // Test for segment motion again [SegmentFocusLeftEdge] [SegmentFocusLast]
  // The positions of "陰謀" and "印房" should be swapped.
  {
    Segments fixed_segments;
    SetKamaboko(&fixed_segments);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    ASSERT_EQ("陰謀", fixed_segments.segment(1).candidate(0).value);
    ASSERT_EQ("印房", fixed_segments.segment(1).candidate(1).value);
    // swap the values.
    fixed_segments.mutable_segment(1)->mutable_candidate(0)->value.swap(
        fixed_segments.mutable_segment(1)->mutable_candidate(1)->value);
    ASSERT_EQ("印房", fixed_segments.segment(1).candidate(0).value);
    ASSERT_EQ("陰謀", fixed_segments.segment(1).candidate(1).value);
    convertermock_->SetCommitSegmentValue(&fixed_segments, true);
  }
  converter.SegmentFocusLeftEdge();
  {
    EXPECT_EQ(0, GetSegmentIndex(converter));
    EXPECT_FALSE(IsCandidateListVisible(converter));
    converter.SegmentFocusLast();
    EXPECT_EQ(1, GetSegmentIndex(converter));
    EXPECT_FALSE(IsCandidateListVisible(converter));
    converter.SetCandidateListVisible(true);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(0, candidates.focused_index());
    EXPECT_EQ(3, candidates.size());  // two candidates + one t13n sub list.
    EXPECT_EQ(5, candidates.position());
    EXPECT_EQ("印房", candidates.candidate(0).value());

    EXPECT_EQ("陰謀", candidates.candidate(1).value());

    EXPECT_EQ("そのほかの文字種", candidates.candidate(2).value());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(kKamabokono, conversion.segment(0).value());
    EXPECT_EQ("印房", conversion.segment(1).value());
  }

  converter.Commit(*composer_, Context::default_instance());
  expected_indices.clear();
  {
    composer_->Reset();
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ("かまぼこの印房", result.value());
    EXPECT_EQ("かまぼこのいんぼう", result.key());
    EXPECT_FALSE(converter.IsActive());
  }
}

TEST_F(SessionConverterTest, Transliterations) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  composer_->InsertCharacterKeyAndPreedit("h", "く");
  composer_->InsertCharacterKeyAndPreedit("J", "ま");

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    segment->set_key("くま");
    segment->add_candidate()->value = "クマー";
  }
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionForRequest(&segments, true);
  EXPECT_TRUE(converter.Convert(*composer_));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // Move to the t13n list.
  converter.CandidateNext(*composer_);
  expected_indices[0] = -1;
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  commands::Output output;
  converter.FillOutput(*composer_, &output);
  EXPECT_FALSE(output.has_result());
  EXPECT_TRUE(output.has_preedit());
  EXPECT_TRUE(output.has_candidates());

  const commands::Candidates &candidates = output.candidates();
  EXPECT_EQ(2, candidates.size());  // one candidate + one t13n sub list.
  EXPECT_EQ(1, candidates.focused_index());
  EXPECT_EQ("そのほかの文字種", candidates.candidate(1).value());

  std::vector<std::string> t13ns;
  composer_->GetTransliterations(&t13ns);

  EXPECT_TRUE(candidates.has_subcandidates());
  EXPECT_EQ(t13ns.size(), candidates.subcandidates().size());
  EXPECT_EQ(9, candidates.subcandidates().candidate_size());

  for (size_t i = 0; i < candidates.subcandidates().candidate_size(); ++i) {
    EXPECT_EQ(t13ns[i], candidates.subcandidates().candidate(i).value());
  }
}

TEST_F(SessionConverterTest, T13NWithResegmentation) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    CHECK(segment);
    segment->set_key("かまぼこの");
    candidate = segment->add_candidate();
    CHECK(candidate);
    candidate->value = "かまぼこの";

    segment = segments.add_segment();
    CHECK(segment);
    segment->set_key("いんぼう");
    candidate = segment->add_candidate();
    CHECK(candidate);
    candidate->value = "いんぼう";

    InsertASCIISequence("kamabokonoinbou", composer_.get());
    FillT13Ns(&segments, composer_.get());
    convertermock_->SetStartConversionForRequest(&segments, true);
  }
  EXPECT_TRUE(converter.Convert(*composer_));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  expected_indices.push_back(0);
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // Test for segment motion. [SegmentFocusRight]
  converter.SegmentFocusRight();
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  // Shrink segment
  {
    Segments segments;
    Segment *segment;
    Segment::Candidate *candidate;

    segments.Clear();
    segment = segments.add_segment();

    segment->set_key("かまぼこの");
    candidate = segment->add_candidate();
    candidate->value = "かまぼこの";
    candidate = segment->add_candidate();
    candidate->value = "カマボコの";

    segment = segments.add_segment();
    segment->set_key("いんぼ");
    candidate = segment->add_candidate();
    candidate->value = "インボ";

    segment = segments.add_segment();
    segment->set_key("う");
    candidate = segment->add_candidate();
    candidate->value = "ウ";

    FillT13Ns(&segments, composer_.get());
    convertermock_->SetResizeSegment1(&segments, true);
  }
  converter.SegmentWidthShrink(*composer_);
  expected_indices.push_back(0);
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // Convert to half katakana. Expected index should be 0.
  converter.ConvertToTransliteration(*composer_,
                                     transliteration::HALF_KATAKANA);
  expected_indices[0] = 0;
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    const commands::Preedit &preedit = output.preedit();
    EXPECT_EQ(3, preedit.segment_size());
    EXPECT_EQ("ｲﾝﾎﾞ", preedit.segment(1).value());
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }
}

TEST_F(SessionConverterTest, ConvertToHalfWidth) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  std::vector<int> expected_indices;
  composer_->InsertCharacterKeyAndPreedit("a", "あ");
  composer_->InsertCharacterKeyAndPreedit("b", "ｂ");
  composer_->InsertCharacterKeyAndPreedit("c", "ｃ");

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    segment->set_key("あｂｃ");
    segment->add_candidate()->value = "あべし";
  }
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionForRequest(&segments, true);
  EXPECT_TRUE(converter.ConvertToHalfWidth(*composer_));
  expected_indices.push_back(0);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("ｱbc", conversion.segment(0).value());
  }

  // Composition will be transliterated to "ａｂｃ".
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("ａｂｃ", conversion.segment(0).value());
  }

  EXPECT_TRUE(converter.ConvertToHalfWidth(*composer_));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("abc", conversion.segment(0).value());
  }

  EXPECT_TRUE(converter.ConvertToHalfWidth(*composer_));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("ABC", conversion.segment(0).value());
  }
}

TEST_F(SessionConverterTest, ConvertToHalfWidth_2) {
  // http://b/2517514
  // ConvertToHalfWidth converts punctuations differently w/ or w/o kana.
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  composer_->InsertCharacterKeyAndPreedit("q", "ｑ");
  composer_->InsertCharacterKeyAndPreedit(",", "、");
  composer_->InsertCharacterKeyAndPreedit(".", "。");

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    segment->set_key("ｑ、。");
    segment->add_candidate()->value = "q,.";
    segment->add_candidate()->value = "q､｡";
  }
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionForRequest(&segments, true);
  EXPECT_TRUE(converter.ConvertToHalfWidth(*composer_));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("q､｡", conversion.segment(0).value());
  }
}

TEST_F(SessionConverterTest, SwitchKanaType) {
  {  // From composition mode.
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());
    composer_->InsertCharacterKeyAndPreedit("a", "あ");
    composer_->InsertCharacterKeyAndPreedit("b", "ｂ");
    composer_->InsertCharacterKeyAndPreedit("c", "ｃ");

    Segments segments;
    {  // Initialize segments.
      Segment *segment = segments.add_segment();
      segment->set_key("あｂｃ");
      segment->add_candidate()->value = "あべし";
    }
    FillT13Ns(&segments, composer_.get());
    convertermock_->SetStartConversionForRequest(&segments, true);
    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    std::vector<int> expected_indices;
    expected_indices.push_back(0);
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    {  // Make sure the output
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      EXPECT_EQ("アｂｃ", conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
    {
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      EXPECT_EQ("ｱbc", conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
    {
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      EXPECT_EQ("あｂｃ", conversion.segment(0).value());
    }
  }

  {  // From conversion mode
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());
    composer_->EditErase();
    composer_->InsertCharacterKeyAndPreedit("ka", "か");
    composer_->InsertCharacterKeyAndPreedit("n", "ん");
    composer_->InsertCharacterKeyAndPreedit("ji", "じ");

    Segments segments;
    {  // Initialize segments.
      Segment *segment = segments.add_segment();
      segment->set_key("かんじ");
      segment->add_candidate()->value = "漢字";
    }
    FillT13Ns(&segments, composer_.get());
    convertermock_->SetStartConversionForRequest(&segments, true);
    EXPECT_TRUE(converter.Convert(*composer_));
    std::vector<int> expected_indices;
    expected_indices.push_back(0);
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    {  // Make sure the output
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      EXPECT_EQ("漢字", conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
    {
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      EXPECT_EQ("かんじ", conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
    {
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      EXPECT_EQ("カンジ", conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
    {
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      EXPECT_EQ("ｶﾝｼﾞ", conversion.segment(0).value());
    }

    EXPECT_TRUE(converter.SwitchKanaType(*composer_));
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
    {
      commands::Output output;
      converter.FillOutput(*composer_, &output);
      EXPECT_FALSE(output.has_result());
      EXPECT_TRUE(output.has_preedit());
      EXPECT_FALSE(output.has_candidates());

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(1, conversion.segment_size());
      EXPECT_EQ("かんじ", conversion.segment(0).value());
    }
  }
}

TEST_F(SessionConverterTest, CommitFirstSegment) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  SetKamaboko(&segments);
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionForRequest(&segments, true);

  const std::string kKamabokono = "かまぼこの";
  const std::string kInbou = "いんぼう";

  composer_->InsertCharacterPreedit(kKamabokono + kInbou);
  EXPECT_TRUE(converter.Convert(*composer_));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  expected_indices.push_back(0);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the conversion.
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(kKamabokono, conversion.segment(0).value());
    EXPECT_EQ("陰謀", conversion.segment(1).value());
  }

  converter.CandidateNext(*composer_);
  expected_indices[0] += 1;
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the conversion.
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ("カマボコの", conversion.segment(0).value());
    EXPECT_EQ("陰謀", conversion.segment(1).value());
  }

  {  // Initialization of SetCommitSegments.
    Segments segments_after_submit;
    Segment *segment = segments_after_submit.add_segment();
    segment->set_key("いんぼう");
    segment->add_candidate()->value = "陰謀";
    segment->add_candidate()->value = "印房";
    convertermock_->SetCommitSegments(&segments_after_submit, true);
  }
  size_t size;
  converter.CommitFirstSegment(*composer_, Context::default_instance(), &size);
  expected_indices.erase(expected_indices.begin(),
                         expected_indices.begin() + 1);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_EQ(Util::CharsLen(kKamabokono), size);
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  EXPECT_COUNT_STATS("Commit", 1);
  EXPECT_COUNT_STATS("CommitFromConversion", 1);
  EXPECT_STATS_NOT_EXIST("ConversionCandidates0");
  EXPECT_COUNT_STATS("ConversionCandidates1", 1);
}

TEST_F(SessionConverterTest, CommitHeadToFocusedSegments) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  const std::string kIberiko = "いべりこ";
  const std::string kNekowo = "ねこを";
  const std::string kItadaita = "いただいた";
  {  // Three segments as the result of conversion.
    Segments segments;
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments.add_segment();
    segment->set_key(kIberiko);
    candidate = segment->add_candidate();
    candidate->value = "イベリコ";

    segment = segments.add_segment();
    segment->set_key(kNekowo);
    candidate = segment->add_candidate();
    candidate->value = "猫を";

    segment = segments.add_segment();
    segment->set_key(kItadaita);
    candidate = segment->add_candidate();
    candidate->value = "頂いた";
    convertermock_->SetStartConversionForRequest(&segments, true);
  }

  composer_->InsertCharacterPreedit(kIberiko + kNekowo + kItadaita);
  EXPECT_TRUE(converter.Convert(*composer_));
  // Here [イベリコ]|猫を|頂いた

  converter.SegmentFocusRight();
  // Here イベリコ|[猫を]|頂いた

  {  // Initialization of SetCommitSegments.
    Segments segments;
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments.add_segment();
    segment->set_key(kItadaita);
    candidate = segment->add_candidate();
    candidate->value = "頂いた";
    convertermock_->SetStartConversionForRequest(&segments, true);

    convertermock_->SetCommitSegments(&segments, true);
  }
  size_t size;
  converter.CommitHeadToFocusedSegments(*composer_, Context::default_instance(),
                                        &size);
  // Here 頂いた
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_EQ(Util::CharsLen(kIberiko + kNekowo), size);
  EXPECT_TRUE(converter.IsActive());
}

TEST_F(SessionConverterTest, CommitHeadToFocusedSegments_atLastSegment) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  SetKamaboko(&segments);
  convertermock_->SetStartConversionForRequest(&segments, true);

  const std::string kKamabokono = "かまぼこの";
  const std::string kInbou = "いんぼう";

  composer_->InsertCharacterPreedit(kKamabokono + kInbou);
  EXPECT_TRUE(converter.Convert(*composer_));
  // Here [かまぼこの]|陰謀

  converter.SegmentFocusRight();
  // Here かまぼこの|[陰謀]

  {  // Initialization of SetCommitSegments.
    Segments segments_after_submit;
    convertermock_->SetCommitSegments(&segments_after_submit, true);
  }
  size_t size;
  // All the segments should be committed.
  converter.CommitHeadToFocusedSegments(*composer_, Context::default_instance(),
                                        &size);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_EQ(0, size);
  EXPECT_FALSE(converter.IsActive());
}

TEST_F(SessionConverterTest, CommitPreedit) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  std::vector<int> expected_indices;
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  composer_->InsertCharacterPreedit(kChars_Aiueo);
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  converter.CommitPreedit(*composer_, Context::default_instance());
  composer_->Reset();
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the result
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(kChars_Aiueo, result.value());
    EXPECT_EQ(kChars_Aiueo, result.key());
  }
  EXPECT_FALSE(converter.IsActive());

  EXPECT_COUNT_STATS("Commit", 1);
  EXPECT_COUNT_STATS("CommitFromComposition", 1);
}

TEST_F(SessionConverterTest, CommitSuggestionByIndex) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  {  // Initialize mock segments for suggestion
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozukusu;
    candidate->key = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->key = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  convertermock_->SetStartSuggestionForRequest(&segments, true);
  EXPECT_TRUE(converter.Suggest(*composer_));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &preedit = output.preedit();
    EXPECT_EQ(1, preedit.segment_size());
    EXPECT_EQ(kChars_Mo, preedit.segment(0).value());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(2, candidates.size());
    EXPECT_EQ(kChars_Mozukusu, candidates.candidate(0).value());
    EXPECT_FALSE(candidates.has_focused_index());
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  // FinishConversion is expected to return empty Segments.
  convertermock_->SetFinishConversion(absl::make_unique<Segments>().get(),
                                      true);

  size_t committed_key_size = 0;
  converter.CommitSuggestionByIndex(1, *composer_, Context::default_instance(),
                                    &committed_key_size);
  expected_indices.clear();
  composer_->Reset();
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_FALSE(converter.IsActive());
  EXPECT_EQ(SessionConverter::kConsumedAllCharacters, committed_key_size);

  {  // Check the result
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(kChars_Momonga, result.value());
    EXPECT_EQ(kChars_Momonga, result.key());
    EXPECT_EQ(SessionConverterInterface::COMPOSITION, GetState(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  EXPECT_COUNT_STATS("Commit", 1);
  // Suggestion is counted as Prediction
  EXPECT_COUNT_STATS("CommitFromPrediction", 1);
  EXPECT_COUNT_STATS("PredictionCandidates1", 1);
}

TEST_F(SessionConverterTest, CommitSuggestionById) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  {  // Initialize mock segments for suggestion
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozukusu;
    candidate->key = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->key = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  convertermock_->SetStartSuggestionForRequest(&segments, true);
  EXPECT_TRUE(converter.Suggest(*composer_));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // FinishConversion is expected to return empty Segments.
  convertermock_->SetFinishConversion(absl::make_unique<Segments>().get(),
                                      true);

  constexpr int kCandidateIndex = 1;
  size_t committed_key_size = 0;
  convertermock_->SetCommitSegmentValue(&segments, true);
  converter.CommitSuggestionById(kCandidateIndex, *composer_,
                                 Context::default_instance(),
                                 &committed_key_size);
  expected_indices.clear();
  composer_->Reset();
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_FALSE(converter.IsActive());
  EXPECT_EQ(SessionConverter::kConsumedAllCharacters, committed_key_size);
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the result
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(kChars_Momonga, result.value());
    EXPECT_EQ(kChars_Momonga, result.key());
    EXPECT_EQ(SessionConverterInterface::COMPOSITION, GetState(converter));
  }
  // Check the converter's internal state
  Segments committed_segments;
  size_t segment_index;
  int candidate_index;
  convertermock_->GetCommitSegmentValue(&committed_segments, &segment_index,
                                        &candidate_index);
  EXPECT_EQ(0, segment_index);
  EXPECT_EQ(kCandidateIndex, candidate_index);

  EXPECT_COUNT_STATS("Commit", 1);
  // Suggestion is counted as Prediction.
  EXPECT_COUNT_STATS("CommitFromPrediction", 1);
  EXPECT_COUNT_STATS("PredictionCandidates" + std::to_string(kCandidateIndex),
                     1);
}

TEST_F(SessionConverterTest, PartialSuggestion) {
  RequestForUnitTest::FillMobileRequest(request_.get());
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments1, segments2;
  Segments suggestion_segments;
  const std::string kChars_Kokode = "ここで";
  const std::string kChars_Hakimonowo = "はきものを";

  {  // Initialize mock segments for partial suggestion
    segments1.set_request_type(Segments::PARTIAL_SUGGESTION);
    Segment *segment = segments1.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Kokode);
    candidate = segment->add_candidate();
    candidate->value = "此処では";
    candidate->key = kChars_Kokode;
    candidate->content_key = kChars_Kokode;
    candidate->attributes = Segment::Candidate::PARTIALLY_KEY_CONSUMED;
    candidate->consumed_key_size = Util::CharsLen(kChars_Kokode);
  }

  // Suggestion that matches to the same key by its prefix.
  // Should not be used by partial suggestion.
  {
    suggestion_segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = suggestion_segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Kokode);
    candidate = segment->add_candidate();
    candidate->value = "ここでは着物を";
    candidate->key = "ここではきものを";
    candidate->content_key = candidate->key;
    candidate = segment->add_candidate();
  }

  {  // Initialize mock segments for suggestion
    segments2.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments2.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Hakimonowo);
    candidate = segment->add_candidate();
    candidate->value = "此処では";
    candidate->key = kChars_Hakimonowo;
    candidate->content_key = kChars_Hakimonowo;
  }

  // "ここではきものを|"    ("|" is cursor position)
  composer_->InsertCharacterPreedit(kChars_Kokode + kChars_Hakimonowo);
  composer_->MoveCursorToEnd();
  // Suggestion for "ここではきものを". Not partial suggestion.
  convertermock_->SetStartSuggestionForRequest(&suggestion_segments, true);
  convertermock_->SetStartPartialSuggestion(&suggestion_segments, false);
  EXPECT_TRUE(converter.Suggest(*composer_));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // "|ここではきものを"    ("|" is cursor position)
  composer_->MoveCursorTo(0);

  // Suggestion for "ここではきものを". Not partial suggestion.
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // "ここで|はきものを"    ("|" is cursor position)
  composer_->MoveCursorTo(3);

  // Partial Suggestion for "ここで"
  convertermock_->SetStartSuggestionForRequest(&segments1, false);
  convertermock_->SetStartPartialSuggestion(&segments1, false);
  convertermock_->SetStartPartialSuggestionForRequest(&segments1, true);
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // commit partial suggestion
  size_t committed_key_size = 0;
  convertermock_->SetStartSuggestionForRequest(&segments2, true);
  convertermock_->SetStartPartialSuggestion(&segments2, false);
  converter.CommitSuggestionById(0, *composer_, Context::default_instance(),
                                 &committed_key_size);
  EXPECT_EQ(Util::CharsLen(kChars_Kokode), committed_key_size);
  // Indices should be {0} since there is another segment.
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the result
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());

    const commands::Result &result = output.result();
    EXPECT_EQ("此処では", result.value());
    EXPECT_EQ(kChars_Kokode, result.key());
    EXPECT_EQ(SessionConverterInterface::SUGGESTION, GetState(converter));
  }
  // Check the converter's internal state
  Segments committed_segments;
  size_t segment_index;
  int candidate_index;
  std::string current_segment_key;
  std::string new_segment_key;
  convertermock_->GetCommitPartialSuggestionSegmentValue(
      &committed_segments, &segment_index, &candidate_index,
      &current_segment_key, &new_segment_key);
  EXPECT_EQ(0, segment_index);
  EXPECT_EQ(0, candidate_index);
  EXPECT_EQ(kChars_Kokode, current_segment_key);
  EXPECT_EQ(kChars_Hakimonowo, new_segment_key);

  EXPECT_COUNT_STATS("Commit", 1);
  // Suggestion is counted as Prediction.
  EXPECT_COUNT_STATS("CommitFromPrediction", 1);
  EXPECT_COUNT_STATS("PredictionCandidates0", 1);
}

TEST_F(SessionConverterTest, SuggestAndPredict) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  {  // Initialize mock segments for suggestion
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  convertermock_->SetStartSuggestionForRequest(&segments, true);
  EXPECT_TRUE(converter.Suggest(*composer_));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_TRUE(output.candidates().has_footer());
#if defined(CHANNEL_DEV) && defined(GOOGLE_JAPANESE_INPUT_BUILD)
    EXPECT_FALSE(output.candidates().footer().has_label());
    EXPECT_TRUE(output.candidates().footer().has_sub_label());
#else   // CHANNEL_DEV && GOOGLE_JAPANESE_INPUT_BUILD
    EXPECT_TRUE(output.candidates().footer().has_label());
    EXPECT_FALSE(output.candidates().footer().has_sub_label());
#endif  // CHANNEL_DEV && GOOGLE_JAPANESE_INPUT_BUILD
    EXPECT_FALSE(output.candidates().footer().index_visible());
    EXPECT_FALSE(output.candidates().footer().logo_visible());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(2, candidates.size());
    EXPECT_EQ(kChars_Mozukusu, candidates.candidate(0).value());
    EXPECT_FALSE(candidates.has_focused_index());
  }

  segments.Clear();
  {  // Initialize mock segments for prediction
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozuku;
    candidate->content_key = kChars_Mozuku;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
    candidate = segment->add_candidate();
    candidate->value = "モンドリアン";
    candidate->content_key = "もんどりあん";
  }

  // Prediction
  convertermock_->SetStartPredictionForRequest(&segments, true);
  EXPECT_TRUE(converter.Predict(*composer_));
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // If there are suggestion results, the Prediction is not triggered.
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_FALSE(output.candidates().footer().has_label());
    EXPECT_TRUE(output.candidates().footer().index_visible());
    EXPECT_TRUE(output.candidates().footer().logo_visible());

    // Check the conversion
    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ(kChars_Mozukusu, conversion.segment(0).value());

    // Check the candidate list
    const commands::Candidates &candidates = output.candidates();
    // Candidates should be the same as suggestion
    EXPECT_EQ(2, candidates.size());
    EXPECT_EQ(kChars_Mozukusu, candidates.candidate(0).value());
    EXPECT_EQ(kChars_Momonga, candidates.candidate(1).value());
    EXPECT_TRUE(candidates.has_focused_index());
    EXPECT_EQ(0, candidates.focused_index());
  }

  // Prediction is called
  converter.CandidateNext(*composer_);
  converter.CandidateNext(*composer_);
  expected_indices[0] += 2;
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    // Candidates should be merged with the previous suggestions.
    EXPECT_EQ(4, candidates.size());
    EXPECT_EQ(kChars_Mozukusu, candidates.candidate(0).value());
    EXPECT_EQ(kChars_Momonga, candidates.candidate(1).value());
    EXPECT_EQ(kChars_Mozuku, candidates.candidate(2).value());
    EXPECT_EQ("モンドリアン", candidates.candidate(3).value());
    EXPECT_TRUE(candidates.has_focused_index());
  }

  // Select to "モンドリアン".
  converter.CandidateNext(*composer_);
  expected_indices[0] += 1;
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  converter.Commit(*composer_, Context::default_instance());
  composer_->Reset();
  expected_indices.clear();
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the submitted value
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ("モンドリアン", result.value());
    EXPECT_EQ("もんどりあん", result.key());
  }

  segments.Clear();
  {  // Initialize mock segments for prediction
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozuku;
    candidate->content_key = kChars_Mozuku;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
    candidate = segment->add_candidate();
    candidate->value = "モンドリアン";
    candidate->content_key = "もんどりあん";
  }

  // Prediction without suggestion.
  convertermock_->SetStartPredictionForRequest(&segments, true);
  expected_indices.push_back(0);
  EXPECT_TRUE(converter.Predict(*composer_));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    // Check the conversion
    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ(kChars_Mozuku, conversion.segment(0).value());

    // Check the candidate list
    const commands::Candidates &candidates = output.candidates();
    // Candidates should NOT be merged with the previous suggestions.
    EXPECT_EQ(3, candidates.size());
    EXPECT_EQ(kChars_Mozuku, candidates.candidate(0).value());
    EXPECT_EQ(kChars_Momonga, candidates.candidate(1).value());
    EXPECT_EQ("モンドリアン", candidates.candidate(2).value());
    EXPECT_TRUE(candidates.has_focused_index());
  }
}

TEST_F(SessionConverterTest, SuppressSuggestionOnPasswordField) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  {  // Initialize mock segments for suggestion
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  convertermock_->SetStartSuggestionForRequest(&segments, true);
  // No candidates should be visible because we are on password field.

  ConversionPreferences conversion_preferences =
      converter.conversion_preferences();
  conversion_preferences.request_suggestion = false;
  EXPECT_FALSE(
      converter.SuggestWithPreferences(*composer_, conversion_preferences));
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_FALSE(converter.IsActive());
}

TEST_F(SessionConverterTest, SuppressSuggestion) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  {  // Initialize mock segments for suggestion
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->SetInputFieldType(Context::PASSWORD);
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  convertermock_->SetStartSuggestionForRequest(&segments, true);
  // No candidates should be visible because we are on password field.
  EXPECT_FALSE(converter.Suggest(*composer_));
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_FALSE(converter.IsActive());
}

TEST_F(SessionConverterTest, ExpandPartialSuggestion) {
  RequestForUnitTest::FillMobileRequest(request_.get());
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());

  const char *kSuggestionValues[] = {
      "S0",
      "S1",
      "S2",
  };
  const char *kPredictionValues[] = {
      "P0",
      "P1",
      "P2",
      // Duplicate entry. Any dupulication should not exist
      // in the candidate list.
      "S1",
      "P3",
  };
  constexpr char kPredictionKey[] = "left";
  constexpr char kSuffixKey[] = "right";
  constexpr int kDupulicationIndex = 3;

  Segments segments;
  {  // Initialize mock segments for suggestion
    segments.set_request_type(Segments::PARTIAL_SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kPredictionKey);
    for (size_t i = 0; i < arraysize(kSuggestionValues); ++i) {
      candidate = segment->add_candidate();
      candidate->value = kSuggestionValues[i];
      candidate->content_key = kPredictionKey;
    }
  }
  composer_->InsertCharacterPreedit(std::string(kPredictionKey) +
                                    std::string(kSuffixKey));
  composer_->MoveCursorTo(Util::CharsLen(std::string(kPredictionKey)));

  // Partial Suggestion
  convertermock_->SetStartPartialSuggestionForRequest(&segments, true);
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());

  segments.Clear();
  {
    // Initialize mock segments for partial prediction for expanding suggestion.
    segments.set_request_type(Segments::PARTIAL_PREDICTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kPredictionKey);
    for (size_t i = 0; i < arraysize(kPredictionValues); ++i) {
      candidate = segment->add_candidate();
      candidate->value = kPredictionValues[i];
      candidate->content_key = kPredictionKey;
    }
  }
  // Expand suggestion candidate (cursor == HEAD)
  composer_->MoveCursorTo(0);
  convertermock_->SetStartPartialPrediction(&segments, false);
  convertermock_->SetStartPredictionForRequest(&segments, true);
  EXPECT_TRUE(converter.ExpandSuggestion(*composer_));
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());

  // Expand suggestion candidate (cursor == TAIL)
  composer_->MoveCursorTo(composer_->GetLength());
  convertermock_->SetStartPredictionForRequest(&segments, false);
  convertermock_->SetStartPartialPrediction(&segments, false);
  convertermock_->SetStartPartialPredictionForRequest(&segments, true);
  EXPECT_TRUE(converter.ExpandSuggestion(*composer_));
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(commands::SUGGESTION, candidates.category());
    EXPECT_EQ(commands::SUGGESTION, output.all_candidate_words().category());
    // -1 is for duplicate entry.
    EXPECT_EQ(arraysize(kSuggestionValues) + arraysize(kPredictionValues) - 1,
              candidates.size());
    size_t i;
    for (i = 0; i < arraysize(kSuggestionValues); ++i) {
      EXPECT_EQ(kSuggestionValues[i], candidates.candidate(i).value());
    }

    // -1 is for duplicate entry.
    for (; i < candidates.size(); ++i) {
      size_t index_in_prediction = i - arraysize(kSuggestionValues);
      if (index_in_prediction >= kDupulicationIndex) {
        ++index_in_prediction;
      }
      EXPECT_EQ(kPredictionValues[index_in_prediction],
                candidates.candidate(i).value());
    }
  }
}

TEST_F(SessionConverterTest, ExpandSuggestion) {
  RequestForUnitTest::FillMobileRequest(request_.get());
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());

  const char *kSuggestionValues[] = {
      "S0",
      "S1",
      "S2",
  };
  const char *kPredictionValues[] = {
      "P0",
      "P1",
      "P2",
      // Duplicate entry. Any dupulication should not exist
      // in the candidate list.
      "S1",
      "P3",
  };
  constexpr char kKey[] = "key";
  constexpr int kDupulicationIndex = 3;

  Segments segments;
  {  // Initialize mock segments for suggestion
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kKey);
    for (size_t i = 0; i < arraysize(kSuggestionValues); ++i) {
      candidate = segment->add_candidate();
      candidate->value = kSuggestionValues[i];
      candidate->content_key = kKey;
    }
  }
  composer_->InsertCharacterPreedit(kKey);

  // Suggestion
  convertermock_->SetStartSuggestionForRequest(&segments, true);
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(commands::SUGGESTION, candidates.category());
    EXPECT_EQ(commands::SUGGESTION, output.all_candidate_words().category());
    EXPECT_EQ(arraysize(kSuggestionValues), candidates.size());
    for (size_t i = 0; i < arraysize(kSuggestionValues); ++i) {
      EXPECT_EQ(kSuggestionValues[i], candidates.candidate(i).value());
    }
  }

  segments.Clear();
  {  // Initialize mock segments for prediction (== expanding suggestion)
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kKey);
    for (size_t i = 0; i < arraysize(kPredictionValues); ++i) {
      candidate = segment->add_candidate();
      candidate->value = kPredictionValues[i];
      candidate->content_key = kKey;
    }
  }
  // Expand suggestion candidate
  convertermock_->SetStartPredictionForRequest(&segments, true);
  EXPECT_TRUE(converter.ExpandSuggestion(*composer_));
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(commands::SUGGESTION, candidates.category());
    EXPECT_EQ(commands::SUGGESTION, output.all_candidate_words().category());
    // -1 is for duplicate entry.
    EXPECT_EQ(arraysize(kSuggestionValues) + arraysize(kPredictionValues) - 1,
              candidates.size());
    size_t i;
    for (i = 0; i < arraysize(kSuggestionValues); ++i) {
      EXPECT_EQ(kSuggestionValues[i], candidates.candidate(i).value());
    }

    // -1 is for duplicate entry.
    for (; i < candidates.size(); ++i) {
      size_t index_in_prediction = i - arraysize(kSuggestionValues);
      if (index_in_prediction >= kDupulicationIndex) {
        ++index_in_prediction;
      }
      EXPECT_EQ(kPredictionValues[index_in_prediction],
                candidates.candidate(i).value());
    }
  }
}

TEST_F(SessionConverterTest, AppendCandidateList) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  SetState(SessionConverterInterface::CONVERSION, &converter);
  converter.set_use_cascading_window(true);
  Segments segments;

  {
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());

    SetSegments(segments, &converter);
    AppendCandidateList(&converter);
    const CandidateList &candidate_list = GetCandidateList(converter);
    // 3 == hiragana cand, katakana cand and sub candidate list.
    EXPECT_EQ(3, candidate_list.size());
    EXPECT_TRUE(candidate_list.focused());
    size_t sub_cand_list_count = 0;
    for (size_t i = 0; i < candidate_list.size(); ++i) {
      if (candidate_list.candidate(i).IsSubcandidateList()) {
        ++sub_cand_list_count;
      }
    }
    // Sub candidate list for T13N.
    EXPECT_EQ(1, sub_cand_list_count);
  }
  {
    Segment *segment = segments.mutable_conversion_segment(0);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "あいうえお_2";
    // New meta candidates.
    // They should be ignored.
    std::vector<Segment::Candidate> *meta_candidates =
        segment->mutable_meta_candidates();
    meta_candidates->clear();
    meta_candidates->resize(1);
    meta_candidates->at(0).Init();
    meta_candidates->at(0).value = "t13nValue";
    meta_candidates->at(0).content_value = "t13nValue";
    meta_candidates->at(0).content_key = segment->key();

    SetSegments(segments, &converter);
    AppendCandidateList(&converter);
    const CandidateList &candidate_list = GetCandidateList(converter);
    // 4 == hiragana cand, katakana cand, hiragana cand2
    // and sub candidate list.
    EXPECT_EQ(4, candidate_list.size());
    EXPECT_TRUE(candidate_list.focused());
    size_t sub_cand_list_count = 0;
    std::set<int> id_set;
    for (size_t i = 0; i < candidate_list.size(); ++i) {
      if (candidate_list.candidate(i).IsSubcandidateList()) {
        ++sub_cand_list_count;
      } else {
        // No duplicate ids are expected.
        int id = candidate_list.candidate(i).id();
        // EXPECT_EQ(iterator, iterator) might cause compile error in specific
        // environment.
        EXPECT_TRUE(id_set.end() == id_set.find(id));
        id_set.insert(id);
      }
    }
    // Sub candidate list shouldn't be duplicated.
    EXPECT_EQ(1, sub_cand_list_count);
  }
}

TEST_F(SessionConverterTest, AppendCandidateListForRequestTypes) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  SetState(SessionConverterInterface::SUGGESTION, &converter);
  Segments segments;

  {
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    segments.set_request_type(Segments::SUGGESTION);
    SetSegments(segments, &converter);
    AppendCandidateList(&converter);
    const CandidateList &candidate_list = GetCandidateList(converter);
    EXPECT_FALSE(candidate_list.focused());
  }

  segments.Clear();
  {
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    segments.set_request_type(Segments::PARTIAL_SUGGESTION);
    SetSegments(segments, &converter);
    AppendCandidateList(&converter);
    const CandidateList &candidate_list = GetCandidateList(converter);
    EXPECT_FALSE(candidate_list.focused());
  }

  segments.Clear();
  {
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    segments.set_request_type(Segments::PARTIAL_PREDICTION);
    SetSegments(segments, &converter);
    AppendCandidateList(&converter);
    const CandidateList &candidate_list = GetCandidateList(converter);
    EXPECT_FALSE(candidate_list.focused());
  }
}

TEST_F(SessionConverterTest, ReloadConfig) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionForRequest(&segments, true);

  composer_->InsertCharacterPreedit("aiueo");
  EXPECT_TRUE(converter.Convert(*composer_));
  converter.SetCandidateListVisible(true);

  {  // Set OperationPreferences
    converter.set_use_cascading_window(false);
    converter.set_selection_shortcut(config::Config::SHORTCUT_123456789);
    EXPECT_TRUE(IsCandidateListVisible(converter));
  }
  {  // Check the config update
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ("1", candidates.candidate(0).annotation().shortcut());
    EXPECT_EQ("2", candidates.candidate(1).annotation().shortcut());
  }

  {  // Set OperationPreferences #2
    converter.set_use_cascading_window(false);
    converter.set_selection_shortcut(config::Config::NO_SHORTCUT);
  }
  {  // Check the config update
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_TRUE(candidates.candidate(0).annotation().shortcut().empty());
    EXPECT_TRUE(candidates.candidate(1).annotation().shortcut().empty());
  }
}

TEST_F(SessionConverterTest, OutputAllCandidateWords) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  SetKamaboko(&segments);
  const std::string kKamabokono = "かまぼこの";
  const std::string kInbou = "いんぼう";
  composer_->InsertCharacterPreedit(kKamabokono + kInbou);

  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionForRequest(&segments, true);

  commands::Output output;

  EXPECT_TRUE(converter.Convert(*composer_));
  {
    ASSERT_TRUE(converter.IsActive());
    EXPECT_FALSE(IsCandidateListVisible(converter));

    output.Clear();
    converter.PopOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());
    EXPECT_TRUE(output.has_all_candidate_words());

    EXPECT_EQ(0, output.all_candidate_words().focused_index());
    EXPECT_EQ(commands::CONVERSION, output.all_candidate_words().category());
    // [ "かまぼこの", "カマボコの", "カマボコノ" (t13n), "かまぼこの" (t13n),
    //   "ｶﾏﾎﾞｺﾉ" (t13n) ]
    EXPECT_EQ(5, output.all_candidate_words().candidates_size());
  }

  converter.CandidateNext(*composer_);
  {
    ASSERT_TRUE(converter.IsActive());
    EXPECT_TRUE(IsCandidateListVisible(converter));

    output.Clear();
    converter.PopOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_TRUE(output.has_all_candidate_words());

    EXPECT_EQ(1, output.all_candidate_words().focused_index());
    EXPECT_EQ(commands::CONVERSION, output.all_candidate_words().category());
    // [ "かまぼこの", "カマボコの", "カマボコノ" (t13n), "かまぼこの" (t13n),
    //   "ｶﾏﾎﾞｺﾉ" (t13n) ]
    EXPECT_EQ(5, output.all_candidate_words().candidates_size());
  }

  converter.SegmentFocusRight();
  {
    ASSERT_TRUE(converter.IsActive());
    EXPECT_FALSE(IsCandidateListVisible(converter));

    output.Clear();
    converter.PopOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());
    EXPECT_TRUE(output.has_all_candidate_words());

    EXPECT_EQ(0, output.all_candidate_words().focused_index());
    EXPECT_EQ(commands::CONVERSION, output.all_candidate_words().category());
    // [ "陰謀", "印房", "インボウ" (t13n), "いんぼう" (t13n), "ｲﾝﾎﾞｳ" (t13n) ]
    EXPECT_EQ(5, output.all_candidate_words().candidates_size());
  }
}

TEST_F(SessionConverterTest, GetPreeditAndGetConversion) {
  Segments segments;

  Segment *segment;
  Segment::Candidate *candidate;

  segment = segments.add_segment();
  segment->set_segment_type(Segment::HISTORY);
  segment->set_key("[key:history1]");
  candidate = segment->add_candidate();
  candidate->content_key = "[content_key:history1-1]";
  candidate = segment->add_candidate();
  candidate->content_key = "[content_key:history1-2]";

  segment = segments.add_segment();
  segment->set_segment_type(Segment::FREE);
  segment->set_key("[key:conversion1]");
  candidate = segment->add_candidate();
  candidate->key = "[key:conversion1-1]";
  candidate->content_key = "[content_key:conversion1-1]";
  candidate->value = "[value:conversion1-1]";
  candidate = segment->add_candidate();
  candidate->key = "[key:conversion1-2]";
  candidate->content_key = "[content_key:conversion1-2]";
  candidate->value = "[value:conversion1-2]";
  {
    // PREDICTION
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());
    convertermock_->SetStartPredictionForRequest(&segments, true);
    converter.Predict(*composer_);
    converter.CandidateNext(*composer_);
    std::string preedit;
    GetPreedit(converter, 0, 1, &preedit);
    EXPECT_EQ("[content_key:conversion1-2]", preedit);
    std::string conversion;
    GetConversion(converter, 0, 1, &conversion);
    EXPECT_EQ("[value:conversion1-2]", conversion);
  }
  {
    // SUGGESTION
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());
    convertermock_->SetStartSuggestionForRequest(&segments, true);
    converter.Suggest(*composer_);
    std::string preedit;
    GetPreedit(converter, 0, 1, &preedit);
    EXPECT_EQ("[content_key:conversion1-1]", preedit);
    std::string conversion;
    GetConversion(converter, 0, 1, &conversion);
    EXPECT_EQ("[value:conversion1-1]", conversion);
  }
  segment = segments.add_segment();
  segment->set_segment_type(Segment::FREE);
  segment->set_key("[key:conversion2]");
  candidate = segment->add_candidate();
  candidate->key = "[key:conversion2-1]";
  candidate->content_key = "[content_key:conversion2-1]";
  candidate->value = "[value:conversion2-1]";
  candidate = segment->add_candidate();
  candidate->key = "[key:conversion2-2]";
  candidate->content_key = "[content_key:conversion2-2]";
  candidate->value = "[value:conversion2-2]";
  {
    // CONVERSION
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());
    convertermock_->SetStartConversionForRequest(&segments, true);
    converter.Convert(*composer_);
    converter.CandidateNext(*composer_);
    std::string preedit;
    GetPreedit(converter, 0, 2, &preedit);
    EXPECT_EQ("[key:conversion1][key:conversion2]", preedit);
    std::string conversion;
    GetConversion(converter, 0, 2, &conversion);
    EXPECT_EQ("[value:conversion1-2][value:conversion2-1]", conversion);
  }
}

TEST_F(SessionConverterTest, GetAndSetSegments) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;

  // Set history segments.
  const std::string kHistoryInput[] = {"車で", "行く"};
  for (size_t i = 0; i < arraysize(kHistoryInput); ++i) {
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = kHistoryInput[i];
  }
  convertermock_->SetFinishConversion(&segments, true);
  converter.CommitPreedit(*composer_, Context::default_instance());

  Segments src;
  GetSegments(converter, &src);
  ASSERT_EQ(2, src.history_segments_size());
  EXPECT_EQ("車で", src.history_segment(0).candidate(0).value);
  EXPECT_EQ("行く", src.history_segment(1).candidate(0).value);

  src.mutable_history_segment(0)->mutable_candidate(0)->value = "歩いて";
  Segment *segment = src.add_segment();
  segment->set_segment_type(Segment::FREE);
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "?";

  SetSegments(src, &converter);

  Segments dest;
  GetSegments(converter, &dest);

  ASSERT_EQ(2, dest.history_segments_size());
  ASSERT_EQ(1, dest.conversion_segments_size());
  EXPECT_EQ(src.history_segment(0).candidate(0).value,
            dest.history_segment(0).candidate(0).value);
  EXPECT_EQ(src.history_segment(1).candidate(0).value,
            dest.history_segment(1).candidate(0).value);
  EXPECT_EQ(src.history_segment(2).candidate(0).value,
            dest.history_segment(2).candidate(0).value);
}

TEST_F(SessionConverterTest, Clone) {
  SessionConverter src(convertermock_.get(), request_.get(), config_.get());

  const std::string kKamabokono = "かまぼこの";
  const std::string kInbou = "いんぼう";
  const std::string kInbouKanji = "陰謀";

  {  // create source converter
    Segments segments;
    SetKamaboko(&segments);

    convertermock_->SetStartConversionForRequest(&segments, true);

    src.set_use_cascading_window(false);
    src.set_selection_shortcut(config::Config::SHORTCUT_123456789);
  }

  {  // validation
    // Copy and validate
    std::unique_ptr<SessionConverter> dest(src.Clone());
    ASSERT_TRUE(dest.get() != nullptr);
    ExpectSameSessionConverter(src, *dest);

    // Convert source
    EXPECT_TRUE(src.Convert(*composer_));
    EXPECT_TRUE(src.IsActive());

    // Convert destination and validate
    EXPECT_TRUE(dest->Convert(*composer_));
    ExpectSameSessionConverter(src, *dest);

    // Copy converted and validate
    dest.reset(src.Clone());
    ASSERT_TRUE(dest.get() != nullptr);
    ExpectSameSessionConverter(src, *dest);
  }
}

// Suggest() in the suggestion state was not accepted.  (http://b/1948334)
TEST_F(SessionConverterTest, Issue1948334) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  {  // Initialize mock segments for the first suggestion
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  convertermock_->SetStartSuggestionForRequest(&segments, true);
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(converter.IsActive());

  segments.Clear();
  {  // Initialize mock segments for the second suggestion
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key("もず");
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
  }
  composer_->InsertCharacterPreedit("もず");

  // Suggestion
  convertermock_->SetStartSuggestionForRequest(&segments, true);
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    // Candidates should be merged with the previous suggestions.
    EXPECT_EQ(1, candidates.size());
    EXPECT_EQ(kChars_Mozukusu, candidates.candidate(0).value());
    EXPECT_FALSE(candidates.has_focused_index());
  }
}

TEST_F(SessionConverterTest, Issue1960362) {
  // Testcase against http://b/1960362, a candidate list was not
  // updated when ConvertToTransliteration changed the size of segments.

  table_->AddRule("zyu", "ZYU", "");
  table_->AddRule("jyu", "ZYU", "");
  table_->AddRule("tt", "XTU", "t");
  table_->AddRule("ta", "TA", "");

  composer_->InsertCharacter("j");
  composer_->InsertCharacter("y");
  composer_->InsertCharacter("u");
  composer_->InsertCharacter("t");

  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());

  Segments segments;
  {
    segments.set_request_type(Segments::CONVERSION);
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments.add_segment();
    segment->set_key("ZYU");
    candidate = segment->add_candidate();
    candidate->value = "[ZYU]";
    candidate->content_key = "[ZYU]";

    segment = segments.add_segment();
    segment->set_key("t");
    candidate = segment->add_candidate();
    candidate->value = "[t]";
    candidate->content_key = "[t]";
  }

  Segments resized_segments;
  {
    resized_segments.set_request_type(Segments::CONVERSION);
    Segment *segment = resized_segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key("ZYUt");
    candidate = segment->add_candidate();
    candidate->value = "[ZYUt]";
    candidate->content_key = "[ZYUt]";
  }

  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionForRequest(&segments, true);
  FillT13Ns(&resized_segments, composer_.get());
  convertermock_->SetResizeSegment1(&resized_segments, true);
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HALF_ASCII));
  EXPECT_FALSE(IsCandidateListVisible(converter));

  commands::Output output;
  converter.FillOutput(*composer_, &output);
  EXPECT_FALSE(output.has_result());
  EXPECT_TRUE(output.has_preedit());
  EXPECT_FALSE(output.has_candidates());

  const commands::Preedit &conversion = output.preedit();
  EXPECT_EQ("jyut", conversion.segment(0).value());
}

TEST_F(SessionConverterTest, Issue1978201) {
  // This is a unittest against http://b/1978201
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  composer_->InsertCharacterPreedit(kChars_Mo);

  {  // Initialize mock segments for prediction
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozuku;
    candidate->content_key = kChars_Mozuku;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }

  // Prediction
  convertermock_->SetStartPredictionForRequest(&segments, true);
  EXPECT_TRUE(converter.Predict(*composer_));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the conversion
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ(kChars_Mozuku, conversion.segment(0).value());
  }

  // Meaningless segment manipulations.
  converter.SegmentWidthShrink(*composer_);
  converter.SegmentFocusLeft();
  converter.SegmentFocusLast();

  {  // Check the conversion again
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ(kChars_Mozuku, conversion.segment(0).value());
  }
}

TEST_F(SessionConverterTest, Issue1981020) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  // "〜〜〜〜" U+301C * 4
  const std::string wave_dash_301c = "〜〜〜〜";
  composer_->InsertCharacterPreedit(wave_dash_301c);
  Segments segments;
  convertermock_->SetFinishConversion(&segments, true);
  converter.CommitPreedit(*composer_, Context::default_instance());
  convertermock_->GetFinishConversion(&segments);

#ifdef OS_WIN
  // "～～～～" U+FF5E * 4
  const std::string fullwidth_tilde_ff5e = "～～～～";
  EXPECT_EQ(fullwidth_tilde_ff5e,
            segments.conversion_segment(0).candidate(0).value);
  EXPECT_EQ(fullwidth_tilde_ff5e,
            segments.conversion_segment(0).candidate(0).content_value);
#else   // OS_WIN
  EXPECT_EQ(wave_dash_301c, segments.conversion_segment(0).candidate(0).value);
  EXPECT_EQ(wave_dash_301c,
            segments.conversion_segment(0).candidate(0).content_value);
#endif  // OS_WIN
}

TEST_F(SessionConverterTest, Issue2029557) {
  // Unittest against http://b/2029557
  // a<tab><F6> raised a DCHECK error.
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  // Composition (as "a")
  composer_->InsertCharacterPreedit("a");

  // Prediction (as <tab>)
  Segments segments;
  SetAiueo(&segments);
  convertermock_->SetStartPredictionForRequest(&segments, true);
  EXPECT_TRUE(converter.Predict(*composer_));
  EXPECT_TRUE(converter.IsActive());

  // Transliteration (as <F6>)
  segments.Clear();
  Segment *segment = segments.add_segment();
  segment->set_key("a");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "a";

  FillT13Ns(&segments, composer_.get());
  convertermock_->SetStartConversionForRequest(&segments, true);
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HIRAGANA));
  EXPECT_TRUE(converter.IsActive());
}

TEST_F(SessionConverterTest, Issue2031986) {
  // Unittest against http://b/2031986
  // aaaaa<Shift+Enter> raised a CRT error.
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());

  {  // Initialize a suggest result triggered by "aaaa".
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("aaaa");
    Segment::Candidate *candidate;
    candidate = segment->add_candidate();
    candidate->value = "AAAA";
    candidate = segment->add_candidate();
    candidate->value = "Aaaa";
    convertermock_->SetStartSuggestionForRequest(&segments, true);
  }
  // Get suggestion
  composer_->InsertCharacterPreedit("aaaa");
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(converter.IsActive());

  {  // Initialize no suggest result triggered by "aaaaa".
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("aaaaa");
    convertermock_->SetStartSuggestionForRequest(&segments, false);
  }
  // Hide suggestion
  composer_->InsertCharacterPreedit("a");
  EXPECT_FALSE(converter.Suggest(*composer_));
  EXPECT_FALSE(converter.IsActive());
}

TEST_F(SessionConverterTest, Issue2040116) {
  // Unittest against http://b/2040116
  //
  // It happens when the first Predict returns results but the next
  // MaybeExpandPrediction does not return any results.  That's a
  // trick by GoogleSuggest.
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  composer_->InsertCharacterPreedit("G");

  {
    // Initialize no predict result.
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    segment->set_key("G");
    convertermock_->SetStartPredictionForRequest(&segments, false);
  }
  // Get prediction
  EXPECT_FALSE(converter.Predict(*composer_));
  EXPECT_FALSE(converter.IsActive());

  {
    // Initialize a suggest result triggered by "G".
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    segment->set_key("G");
    Segment::Candidate *candidate;
    candidate = segment->add_candidate();
    candidate->value = "GoogleSuggest";
    convertermock_->SetStartPredictionForRequest(&segments, true);
  }
  // Get prediction again
  EXPECT_TRUE(converter.Predict(*composer_));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the conversion.
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("GoogleSuggest", conversion.segment(0).value());
  }

  {
    // Initialize no predict result triggered by "G".  It's possible
    // by Google Suggest.
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    segment->set_key("G");
    convertermock_->SetStartPredictionForRequest(&segments, false);
  }
  // Hide prediction
  converter.CandidateNext(*composer_);
  EXPECT_TRUE(converter.IsActive());

  {  // Check the conversion.
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(1, conversion.segment_size());
    EXPECT_EQ("GoogleSuggest", conversion.segment(0).value());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(1, candidates.candidate_size());
  }
}

TEST_F(SessionConverterTest, GetReadingText) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());

  const char *kKanjiAiueo = "阿伊宇江於";
  // Set up Segments for reverse conversion.
  Segments reverse_segments;
  Segment *segment;
  segment = reverse_segments.add_segment();
  segment->set_key(kKanjiAiueo);
  Segment::Candidate *candidate;
  candidate = segment->add_candidate();
  // For reverse conversion, key is the original kanji string.
  candidate->key = kKanjiAiueo;
  candidate->value = kChars_Aiueo;
  convertermock_->SetStartReverseConversion(&reverse_segments, true);
  // Set up Segments for forward conversion.
  Segments segments;
  segment = segments.add_segment();
  segment->set_key(kChars_Aiueo);
  candidate = segment->add_candidate();
  candidate->key = kChars_Aiueo;
  candidate->value = kKanjiAiueo;
  convertermock_->SetStartConversionForRequest(&segments, true);

  std::string reading;
  EXPECT_TRUE(converter.GetReadingText(kKanjiAiueo, &reading));
  EXPECT_EQ(kChars_Aiueo, reading);
}

TEST_F(SessionConverterTest, ZeroQuerySuggestion) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());

  // Set up a mock suggestion result.
  Segments segments;
  segments.set_request_type(Segments::SUGGESTION);
  Segment *segment;
  segment = segments.add_segment();
  segment->set_key("");
  segment->add_candidate()->value = "search";
  segment->add_candidate()->value = "input";
  convertermock_->SetStartSuggestionForRequest(&segments, true);

  EXPECT_TRUE(composer_->Empty());
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(2, candidates.size());
    EXPECT_EQ("search", candidates.candidate(0).value());
    EXPECT_EQ("input", candidates.candidate(1).value());
  }
}

// since History segments are almost hidden from
namespace {

class ConverterMockForReset : public ConverterMock {
 public:
  ConverterMockForReset() : reset_conversion_called_(false) {}

  bool ResetConversion(Segments *segments) const override {
    reset_conversion_called_ = true;
    return true;
  }

  bool reset_conversion_called() const { return reset_conversion_called_; }

  void Reset() { reset_conversion_called_ = false; }

 private:
  mutable bool reset_conversion_called_;
};

class ConverterMockForRevert : public ConverterMock {
 public:
  ConverterMockForRevert() : revert_conversion_called_(false) {}

  bool RevertConversion(Segments *segments) const override {
    revert_conversion_called_ = true;
    return true;
  }

  bool revert_conversion_called() const { return revert_conversion_called_; }

  void Reset() { revert_conversion_called_ = false; }

 private:
  mutable bool revert_conversion_called_;
};

class ConverterMockForReconstructHistory : public ConverterMock {
 public:
  ConverterMockForReconstructHistory() : reconstruct_history_called_(false) {}

  bool ReconstructHistory(Segments *segments,
                          const std::string &preceding_text) const override {
    reconstruct_history_called_ = true;
    preceding_text_ = preceding_text;
    ConverterMock::ReconstructHistory(segments, preceding_text);
    return true;
  }

  bool reconstruct_history_called() const {
    return reconstruct_history_called_;
  }

  const std::string preceding_text() const { return preceding_text_; }

  void Reset() {
    reconstruct_history_called_ = false;
    preceding_text_.clear();
  }

 private:
  mutable bool reconstruct_history_called_;
  mutable std::string preceding_text_;
};

}  // namespace

TEST(SessionConverterResetTest, Reset) {
  ConverterMockForReset convertermock;
  Request request;
  Config config;
  SessionConverter converter(&convertermock, &request, &config);
  EXPECT_FALSE(convertermock.reset_conversion_called());
  converter.Reset();
  EXPECT_TRUE(convertermock.reset_conversion_called());
}

TEST(SessionConverterRevertTest, Revert) {
  ConverterMockForRevert convertermock;
  Request request;
  Config config;
  SessionConverter converter(&convertermock, &request, &config);
  EXPECT_FALSE(convertermock.revert_conversion_called());
  converter.Revert();
  EXPECT_TRUE(convertermock.revert_conversion_called());
}

TEST_F(SessionConverterTest, CommitHead) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  composer_->InsertCharacterPreedit(kChars_Aiueo);

  size_t committed_size;
  converter.CommitHead(1, *composer_, &committed_size);
  EXPECT_EQ(1, committed_size);
  composer_->DeleteAt(0);

  commands::Output output;
  converter.FillOutput(*composer_, &output);
  EXPECT_TRUE(output.has_result());
  EXPECT_FALSE(output.has_candidates());

  const commands::Result &result = output.result();
  EXPECT_EQ("あ", result.value());
  EXPECT_EQ("あ", result.key());
  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("いうえお", preedit);

  converter.CommitHead(3, *composer_, &committed_size);
  EXPECT_EQ(3, committed_size);
  composer_->DeleteAt(0);
  composer_->DeleteAt(0);
  composer_->DeleteAt(0);
  converter.FillOutput(*composer_, &output);
  EXPECT_TRUE(output.has_result());
  EXPECT_FALSE(output.has_candidates());

  const commands::Result &result2 = output.result();
  EXPECT_EQ("いうえ", result2.value());
  EXPECT_EQ("いうえ", result2.key());
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("お", preedit);

  EXPECT_STATS_NOT_EXIST("Commit");
  EXPECT_STATS_NOT_EXIST("CommitFromComposition");
}

TEST_F(SessionConverterTest, CommandCandidate) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(&segments, composer_.get());
  // set COMMAND_CANDIDATE.
  SetCommandCandidate(&segments, 0, 0, Segment::Candidate::DEFAULT_COMMAND);
  convertermock_->SetStartConversionForRequest(&segments, true);

  composer_->InsertCharacterPreedit(kChars_Aiueo);
  EXPECT_TRUE(converter.Convert(*composer_));

  converter.Commit(*composer_, Context::default_instance());
  commands::Output output;
  converter.FillOutput(*composer_, &output);
  EXPECT_FALSE(output.has_result());
}

TEST_F(SessionConverterTest, CommandCandidateWithCommitCommands) {
  const std::string kKamabokono = "かまぼこの";
  const std::string kInbou = "いんぼう";
  composer_->InsertCharacterPreedit(kKamabokono + kInbou);

  {
    // The first candidate is a command candidate, so
    // CommitFirstSegment resets all conversion.
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());
    Segments segments;
    SetKamaboko(&segments);
    SetCommandCandidate(&segments, 0, 0, Segment::Candidate::DEFAULT_COMMAND);
    convertermock_->SetStartConversionForRequest(&segments, true);
    converter.Convert(*composer_);

    size_t committed_size = 0;
    converter.CommitFirstSegment(*composer_, Context::default_instance(),
                                 &committed_size);
    EXPECT_EQ(0, committed_size);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(converter.IsActive());
    EXPECT_FALSE(output.has_result());
  }

  {
    // The second candidate is a command candidate, so
    // CommitFirstSegment commits all conversion.
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());
    Segments segments;
    SetKamaboko(&segments);
    SetCommandCandidate(&segments, 1, 0, Segment::Candidate::DEFAULT_COMMAND);
    convertermock_->SetStartConversionForRequest(&segments, true);
    converter.Convert(*composer_);

    size_t committed_size = 0;
    converter.CommitFirstSegment(*composer_, Context::default_instance(),
                                 &committed_size);
    EXPECT_EQ(Util::CharsLen(kKamabokono), committed_size);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(converter.IsActive());
    EXPECT_TRUE(output.has_result());
  }

  {
    // The selected suggestion with Id is a command candidate.
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());
    Segments segments;
    SetAiueo(&segments);
    SetCommandCandidate(&segments, 0, 0, Segment::Candidate::DEFAULT_COMMAND);
    convertermock_->SetStartSuggestionForRequest(&segments, true);
    converter.Suggest(*composer_);

    size_t committed_size = 0;
    EXPECT_FALSE(converter.CommitSuggestionById(
        0, *composer_, Context::default_instance(), &committed_size));
    EXPECT_EQ(0, committed_size);
  }

  {
    // The selected suggestion with Index is a command candidate.
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());
    Segments segments;
    SetAiueo(&segments);
    SetCommandCandidate(&segments, 0, 1, Segment::Candidate::DEFAULT_COMMAND);
    convertermock_->SetStartSuggestionForRequest(&segments, true);
    converter.Suggest(*composer_);

    size_t committed_size = 0;
    EXPECT_FALSE(converter.CommitSuggestionByIndex(
        1, *composer_, Context::default_instance(), &committed_size));
    EXPECT_EQ(0, committed_size);
  }
}

TEST_F(SessionConverterTest, ExecuteCommandCandidate) {
  // Enable Incognito mode
  {
    config_->set_incognito_mode(false);
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());
    Segments segments;
    SetAiueo(&segments);
    SetCommandCandidate(&segments, 0, 0,
                        Segment::Candidate::ENABLE_INCOGNITO_MODE);
    convertermock_->SetStartConversionForRequest(&segments, true);

    composer_->InsertCharacterPreedit(kChars_Aiueo);
    EXPECT_TRUE(converter.Convert(*composer_));

    converter.Commit(*composer_, Context::default_instance());
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());

    // The config in the |output| has the updated value, but |config_| keeps
    // the previous value.
    EXPECT_TRUE(output.has_config());
    EXPECT_TRUE(output.config().incognito_mode());
    EXPECT_FALSE(config_->incognito_mode());
  }

  // Disable Incognito mode
  {
    config_->set_incognito_mode(false);
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());
    Segments segments;
    SetAiueo(&segments);
    SetCommandCandidate(&segments, 0, 0,
                        Segment::Candidate::DISABLE_INCOGNITO_MODE);
    convertermock_->SetStartConversionForRequest(&segments, true);

    composer_->InsertCharacterPreedit(kChars_Aiueo);
    EXPECT_TRUE(converter.Convert(*composer_));

    converter.Commit(*composer_, Context::default_instance());
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());

    // The config in the |output| has the updated value, but |config_| keeps
    // the previous value.
    EXPECT_TRUE(output.has_config());
    EXPECT_FALSE(output.config().incognito_mode());
    EXPECT_FALSE(config_->incognito_mode());
  }

  // Enable Presentation mode
  {
    config_->set_presentation_mode(false);
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());

    Segments segments;
    SetAiueo(&segments);
    SetCommandCandidate(&segments, 0, 0,
                        Segment::Candidate::ENABLE_PRESENTATION_MODE);
    convertermock_->SetStartConversionForRequest(&segments, true);

    composer_->InsertCharacterPreedit(kChars_Aiueo);
    EXPECT_TRUE(converter.Convert(*composer_));

    converter.Commit(*composer_, Context::default_instance());
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());

    // The config in the |output| has the updated value, but |config_| keeps
    // the previous value.
    EXPECT_TRUE(output.has_config());
    EXPECT_TRUE(output.config().presentation_mode());
    EXPECT_FALSE(config_->presentation_mode());
  }

  // Disable Presentation mode
  {
    config_->set_incognito_mode(true);
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());

    Segments segments;
    SetAiueo(&segments);
    SetCommandCandidate(&segments, 0, 0,
                        Segment::Candidate::DISABLE_PRESENTATION_MODE);
    convertermock_->SetStartConversionForRequest(&segments, true);

    composer_->InsertCharacterPreedit(kChars_Aiueo);
    EXPECT_TRUE(converter.Convert(*composer_));

    converter.Commit(*composer_, Context::default_instance());
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());

    // The config in the |output| has the updated value, but |config_| keeps
    // the previous value.
    EXPECT_TRUE(output.has_config());
    EXPECT_FALSE(output.config().presentation_mode());
    EXPECT_FALSE(config_->presentation_mode());
  }
}

TEST_F(SessionConverterTest, PropageteConfigToRenderer) {
  // Disable information_list_config()
  {
    SessionConverter converter(convertermock_.get(), request_.get(),
                               config_.get());
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    convertermock_->SetStartConversionForRequest(&segments, true);

    commands::Output output;
    composer_->InsertCharacterPreedit(kChars_Aiueo);
    converter.Convert(*composer_);

    EXPECT_FALSE(IsCandidateListVisible(converter));
    output.Clear();
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_config());

    converter.CandidateNext(*composer_);
    EXPECT_TRUE(IsCandidateListVisible(converter));
    output.Clear();
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_config());
  }
}

TEST_F(SessionConverterTest, ConversionFail) {
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());

  // Conversion fails.
  {
    // segments doesn't have any candidates.
    Segments segments;
    segments.add_segment()->set_key(kChars_Aiueo);
    convertermock_->SetStartConversionForRequest(&segments, false);
    composer_->InsertCharacterPreedit(kChars_Aiueo);

    // Falls back to composition state.
    EXPECT_FALSE(converter.Convert(*composer_));
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_TRUE(converter.CheckState(SessionConverterInterface::COMPOSITION));

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());
    EXPECT_FALSE(IsCandidateListVisible(converter));
  }

  composer_->Reset();

  // Suggestion succeeds and conversion fails.
  {
    Segments segments;
    SetAiueo(&segments);
    convertermock_->SetStartSuggestionForRequest(&segments, true);
    composer_->InsertCharacterPreedit(kChars_Aiueo);

    EXPECT_TRUE(converter.Suggest(*composer_));
    EXPECT_TRUE(IsCandidateListVisible(converter));
    EXPECT_TRUE(converter.CheckState(SessionConverterInterface::SUGGESTION));

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    segments.Clear();
    output.Clear();

    // segments doesn't have any candidates.
    segments.add_segment()->set_key(kChars_Aiueo);
    convertermock_->SetStartConversionForRequest(&segments, false);

    // Falls back to composition state.
    EXPECT_FALSE(converter.Convert(*composer_));
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_TRUE(converter.CheckState(SessionConverterInterface::COMPOSITION));

    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());
  }
}

TEST_F(SessionConverterTest, ResetByClientRevision) {
  const int32_t kRevision = 0x1234;

  ConverterMockForReset convertermock;
  SessionConverter converter(&convertermock, request_.get(), config_.get());
  Context context;

  // Initialize the session converter with given context age.
  context.set_revision(kRevision);
  converter.OnStartComposition(context);
  converter.Revert();

  convertermock.Reset();
  EXPECT_FALSE(convertermock.reset_conversion_called());

  // OnStartComposition with different context age causes Reset()
  context.set_revision(kRevision + 1);
  converter.OnStartComposition(context);
  EXPECT_TRUE(convertermock.reset_conversion_called());
}

TEST_F(SessionConverterTest, ResetByPrecedingText) {
  ConverterMockForReset convertermock;
  SessionConverter converter(&convertermock, request_.get(), config_.get());

  // no preceding_text -> Reset should not be called.
  {
    convertermock.Reset();
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    for (size_t i = 0; i < segments.segments_size(); ++i) {
      Segment *segment = segments.mutable_segment(i);
      segment->set_segment_type(Segment::HISTORY);
    }
    SetSegments(segments, &converter);
    converter.OnStartComposition(Context::default_instance());
    EXPECT_FALSE(convertermock.reset_conversion_called());
    converter.Revert();
  }

  // preceding_text == history_segments -> Reset should not be called.
  {
    convertermock.Reset();
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    for (size_t i = 0; i < segments.segments_size(); ++i) {
      Segment *segment = segments.mutable_segment(i);
      segment->set_segment_type(Segment::HISTORY);
    }
    SetSegments(segments, &converter);
    Context context;
    context.set_preceding_text(kChars_Aiueo);
    converter.OnStartComposition(context);
    EXPECT_FALSE(convertermock.reset_conversion_called());
    converter.Revert();
  }

  // preceding_text == "" && history_segments != "" -> Reset should be called.
  {
    convertermock.Reset();
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    for (size_t i = 0; i < segments.segments_size(); ++i) {
      Segment *segment = segments.mutable_segment(i);
      segment->set_segment_type(Segment::HISTORY);
    }
    SetSegments(segments, &converter);
    Context context;
    context.set_preceding_text("");
    converter.OnStartComposition(context);
    EXPECT_TRUE(convertermock.reset_conversion_called());
    converter.Revert();
  }

  // preceding_text != "" && preceding_text.EndsWith(history_segments).
  //    -> Reset should not be called.
  {
    convertermock.Reset();
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    for (size_t i = 0; i < segments.segments_size(); ++i) {
      Segment *segment = segments.mutable_segment(i);
      segment->set_segment_type(Segment::HISTORY);
    }
    SetSegments(segments, &converter);
    Context context;
    context.set_preceding_text(kChars_Aiueo);
    converter.OnStartComposition(context);
    EXPECT_FALSE(convertermock.reset_conversion_called());
    converter.Revert();
  }

  // preceding_text != "" && history_segments.EndsWith(preceding_text).
  //    -> Reset should not be called.
  {
    convertermock.Reset();
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    for (size_t i = 0; i < segments.segments_size(); ++i) {
      Segment *segment = segments.mutable_segment(i);
      segment->set_segment_type(Segment::HISTORY);
    }
    SetSegments(segments, &converter);
    Context context;
    context.set_preceding_text(kChars_Aiueo);
    converter.OnStartComposition(context);
    EXPECT_FALSE(convertermock.reset_conversion_called());
    converter.Revert();
  }
}

TEST_F(SessionConverterTest, ReconstructHistoryByPrecedingText) {
  ConverterMockForReconstructHistory convertermock;

  const uint16_t kId = 1234;
  constexpr char kKey[] = "1";
  constexpr char kValue[] = "1";

  // Set up mock
  Segments mock_result;
  {
    Segment *segment = mock_result.add_segment();
    segment->set_key(kKey);
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->push_back_candidate();
    candidate->rid = kId;
    candidate->lid = kId;
    candidate->content_key = kKey;
    candidate->key = kKey;
    candidate->content_value = kValue;
    candidate->value = kValue;
    candidate->attributes = Segment::Candidate::NO_LEARNING;
  }
  convertermock.SetReconstructHistory(&mock_result, true);

  // With revision
  {
    SessionConverter converter(&convertermock, request_.get(), config_.get());

    Context context;
    context.set_revision(0);
    context.set_preceding_text(kKey);
    EXPECT_FALSE(convertermock.reconstruct_history_called());
    converter.OnStartComposition(context);
    EXPECT_TRUE(convertermock.reconstruct_history_called());
    EXPECT_EQ(kKey, convertermock.preceding_text());

    // History segments should be reconstructed.
    Segments segments;
    GetSegments(converter, &segments);
    EXPECT_EQ(1, segments.segments_size());
    const Segment &segment = segments.segment(0);
    EXPECT_EQ(Segment::HISTORY, segment.segment_type());
    EXPECT_EQ(kKey, segment.key());
    EXPECT_EQ(1, segment.candidates_size());
    const Segment::Candidate &candidate = segment.candidate(0);
    EXPECT_EQ(Segment::Candidate::NO_LEARNING, candidate.attributes);
    EXPECT_EQ(kKey, candidate.content_key);
    EXPECT_EQ(kKey, candidate.key);
    EXPECT_EQ(kValue, candidate.content_value);
    EXPECT_EQ(kValue, candidate.value);
    EXPECT_EQ(kId, candidate.lid);
    EXPECT_EQ(kId, candidate.rid);

    convertermock.Reset();
    convertermock.SetReconstructHistory(&mock_result, true);

    // Increment revesion
    context.set_revision(1);
    context.set_preceding_text(kKey);
    EXPECT_FALSE(convertermock.reconstruct_history_called());
    converter.OnStartComposition(context);
    EXPECT_FALSE(convertermock.reconstruct_history_called())
        << "ReconstructHistory should not be called as long as the "
        << "history segments are consistent with the preceding text "
        << "even when the revision number is changed.";
  }

  convertermock.Reset();
  convertermock.SetReconstructHistory(&mock_result, true);

  // Without revision
  {
    SessionConverter converter(&convertermock, request_.get(), config_.get());

    Context context;
    context.set_preceding_text(kKey);
    EXPECT_FALSE(convertermock.reconstruct_history_called());
    converter.OnStartComposition(context);
    EXPECT_TRUE(convertermock.reconstruct_history_called());
    EXPECT_EQ(kKey, convertermock.preceding_text());

    // History segments should be reconstructed.
    Segments segments;
    GetSegments(converter, &segments);
    EXPECT_EQ(1, segments.segments_size());
    const Segment &segment = segments.segment(0);
    EXPECT_EQ(Segment::HISTORY, segment.segment_type());
    EXPECT_EQ(kKey, segment.key());
    EXPECT_EQ(1, segment.candidates_size());
    const Segment::Candidate &candidate = segment.candidate(0);
    EXPECT_EQ(Segment::Candidate::NO_LEARNING, candidate.attributes);
    EXPECT_EQ(kKey, candidate.content_key);
    EXPECT_EQ(kKey, candidate.key);
    EXPECT_EQ(kValue, candidate.content_value);
    EXPECT_EQ(kValue, candidate.value);
    EXPECT_EQ(kId, candidate.lid);
    EXPECT_EQ(kId, candidate.rid);

    convertermock.Reset();
    convertermock.SetReconstructHistory(&mock_result, true);

    context.set_preceding_text(kKey);
    EXPECT_FALSE(convertermock.reconstruct_history_called());
    converter.OnStartComposition(context);
    EXPECT_FALSE(convertermock.reconstruct_history_called())
        << "ReconstructHistory should not be called as long as the "
        << "history segments are consistent with the preceding text "
        << "even when the revision number is changed.";
  }
}

// Test whether Request::candidate_page_size is correctly propagated to
// CandidateList.page_size in SessionConverter.  The tests for the behavior
// of CandidateList.page_size is in session/internal/candidate_list_test.cc
TEST_F(SessionConverterTest, CandidatePageSize) {
  constexpr size_t kPageSize = 3;
  request_->set_candidate_page_size(kPageSize);
  SessionConverter converter(convertermock_.get(), request_.get(),
                             config_.get());
  EXPECT_EQ(kPageSize, GetCandidateList(converter).page_size());
}

}  // namespace session
}  // namespace mozc

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

#include "prediction/zero_query_decoder.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"
#include "absl/time/time.h"
#include "base/clock_mock.h"
#include "base/container/serialized_string_array.h"
#include "converter/attribute.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "prediction/zero_query_dict.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc::prediction {

class ZeroQueryDecoderTestPeer {
 public:
  explicit ZeroQueryDecoderTestPeer(const ZeroQueryDecoder& decoder)
      : decoder_(decoder) {}

  void GetZeroQueryCandidatesForKey(const ConversionRequest& request,
                                    absl::string_view key,
                                    const ZeroQueryDict& dict, uint16_t lid,
                                    uint16_t rid,
                                    std::vector<Result>* results) const {
    decoder_.GetZeroQueryCandidatesForKey(request, key, dict, lid, rid,
                                          results);
  }

  bool AggregateWeekdayZeroQuery(const ConversionRequest& request,
                                 std::vector<Result>* results) const {
    return decoder_.AggregateWeekdayZeroQuery(request, results);
  }

 private:
  const ZeroQueryDecoder& decoder_;
};

namespace {

using ::mozc::dictionary::PosMatcher;

class ZeroQueryDecoderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_OK_AND_ASSIGN(
        modules_,
        engine::Modules::Create(std::make_unique<testing::MockDataManager>()));
    decoder_ = std::make_unique<ZeroQueryDecoder>(*modules_);
  }

  static Result CreateHistoryResult(absl::string_view key,
                                    absl::string_view value) {
    Result result;
    result.key = std::string(key);
    result.value = std::string(value);
    return result;
  }

  // Essentially the same logic as converter::EncodeLengths in
  // converter/inner_segment.h.
  static uint32_t EncodeBoundary(uint8_t key_len, uint8_t value_len) {
    return static_cast<uint32_t>(key_len) |
           (static_cast<uint32_t>(value_len) << 8) |
           (static_cast<uint32_t>(key_len) << 16) |
           (static_cast<uint32_t>(value_len) << 24);
  }

  const PosMatcher& pos_matcher() const { return modules_->GetPosMatcher(); }

  std::unique_ptr<engine::Modules> modules_;
  std::unique_ptr<ZeroQueryDecoder> decoder_;
};

TEST_F(ZeroQueryDecoderTest, EmptyRequestReturnsEmpty) {
  ConversionRequest request;
  std::vector<Result> results = decoder_->Decode(request);
  EXPECT_TRUE(results.empty());
}

TEST_F(ZeroQueryDecoderTest, ZeroQuerySuggestionAfterNumbers) {
  commands::Request request_proto;
  request_proto.set_zero_query_suggestion(true);

  {
    constexpr absl::string_view kHistoryKey = "12";
    constexpr absl::string_view kHistoryValue = "12";
    constexpr absl::string_view kExpectedValue = "月";

    const Result history = CreateHistoryResult(kHistoryKey, kHistoryValue);
    ConversionRequest request = ConversionRequestBuilder()
                                    .SetHistoryResult(history)
                                    .SetRequest(request_proto)
                                    .Build();

    std::vector<Result> results = decoder_->Decode(request);
    EXPECT_FALSE(results.empty());

    auto target = results.end();
    for (auto it = results.begin(); it != results.end(); ++it) {
      if (it->value == kExpectedValue) {
        target = it;
        break;
      }
    }
    ASSERT_NE(results.end(), target);
    EXPECT_EQ(target->value, kExpectedValue);
    EXPECT_EQ(target->lid, pos_matcher().GetCounterSuffixWordId());
    EXPECT_EQ(target->rid, pos_matcher().GetCounterSuffixWordId());
  }

  {
    constexpr absl::string_view kHistoryKey = "66050713";  // A random number
    constexpr absl::string_view kHistoryValue = "66050713";
    constexpr absl::string_view kExpectedValue = "個";

    const Result history = CreateHistoryResult(kHistoryKey, kHistoryValue);
    ConversionRequest request = ConversionRequestBuilder()
                                    .SetHistoryResult(history)
                                    .SetRequest(request_proto)
                                    .Build();

    std::vector<Result> results = decoder_->Decode(request);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (const auto& result : results) {
      if (result.value == kExpectedValue) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
}

TEST_F(ZeroQueryDecoderTest, TriggerNumberZeroQuerySuggestion) {
  commands::Request request_proto;
  request_proto.set_zero_query_suggestion(true);

  const struct TestCase {
    const char* history_key;
    const char* history_value;
    const char* find_suffix_value;
    bool expected_result;
  } kTestCases[] = {
      {"12", "12", "月", true},      {"12", "１２", "月", true},
      {"12", "壱拾弐", "月", false}, {"12", "十二", "月", false},
      {"12", "一二", "月", false},   {"12", "Ⅻ", "月", false},
      {"あか", "12", "月", true},    // T13N
      {"あか", "１２", "月", true},  // T13N
      {"じゅう", "10", "時", true},  {"じゅう", "１０", "時", true},
      {"じゅう", "十", "時", false}, {"じゅう", "拾", "時", false},
  };

  for (const auto& test_case : kTestCases) {
    const Result history =
        CreateHistoryResult(test_case.history_key, test_case.history_value);
    ConversionRequest request = ConversionRequestBuilder()
                                    .SetHistoryResult(history)
                                    .SetRequest(request_proto)
                                    .Build();

    std::vector<Result> results = decoder_->Decode(request);

    bool found = false;
    for (const auto& result : results) {
      if (result.value == test_case.find_suffix_value) {
        found = true;
        break;
      }
    }
    EXPECT_EQ(found, test_case.expected_result)
        << "Failed for history: " << test_case.history_value
        << ", suffix: " << test_case.find_suffix_value;
  }
}

TEST_F(ZeroQueryDecoderTest, GeneralZeroQuery) {
  commands::Request request_proto;
  request_proto.set_zero_query_suggestion(true);

  commands::Context context;
  context.set_preceding_text("あけまして");

  ConversionRequest request = ConversionRequestBuilder()
                                  .SetContext(context)
                                  .SetRequest(request_proto)
                                  .Build();

  std::vector<Result> results = decoder_->Decode(request);
  EXPECT_FALSE(results.empty());
}

namespace {
alignas(uint32_t) constexpr char kTestZeroQueryTokenArray[] =
    // The last two items must be 0x00, because they are now unused field.
    // {"あ", "❕", ZERO_QUERY_EMOJI, 0x00, 0x00}
    "\x04\x00\x00\x00"
    "\x02\x00\x00\x00"
    "\x03\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"ああ", "( •̀ㅁ•́;)", ZERO_QUERY_EMOTICON, 0x00, 0x00}
    "\x05\x00\x00\x00"
    "\x01\x00\x00\x00"
    "\x02\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"あい", "❕", ZERO_QUERY_EMOJI, 0x00, 0x00}
    "\x06\x00\x00\x00"
    "\x02\x00\x00\x00"
    "\x03\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"あい", "❣", ZERO_QUERY_NONE, 0x00, 0x00}
    "\x06\x00\x00\x00"
    "\x03\x00\x00\x00"
    "\x00\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"猫", "😾", ZERO_QUERY_EMOJI, 0x00, 0x00}
    "\x07\x00\x00\x00"
    "\x08\x00\x00\x00"
    "\x03\x00"
    "\x00\x00"
    "\x00\x00\x00\x00";

const char* kTestZeroQueryStrings[] = {"",     "( •̀ㅁ•́;)", "❕", "❣", "あ",
                                       "ああ", "あい",     "猫", "😾"};
}  // namespace

TEST_F(ZeroQueryDecoderTest, GetZeroQueryCandidates) {
  const ZeroQueryDecoderTestPeer peer(*decoder_);

  // Create test zero query data.
  std::unique_ptr<uint32_t[]> string_data_buffer;
  ZeroQueryDict zero_query_dict;
  {
    // kTestZeroQueryTokenArray contains a trailing '\0', so create a
    // absl::string_view that excludes it by subtracting 1.
    const absl::string_view token_array_data(
        kTestZeroQueryTokenArray, std::size(kTestZeroQueryTokenArray) - 1);
    std::vector<absl::string_view> strs;
    for (const char* str : kTestZeroQueryStrings) {
      strs.push_back(str);
    }
    const absl::string_view string_array_data =
        SerializedStringArray::SerializeToBuffer(strs, &string_data_buffer);
    zero_query_dict.Init(token_array_data, string_array_data);
  }

  struct TestCase {
    std::string key;
    bool expected_result;
    // candidate value and ZeroQueryType.
    std::vector<std::string> expected_candidates;
    std::vector<ZeroQueryType> expected_types;

    std::string DebugString() const {
      const std::string candidates = absl::StrJoin(expected_candidates, ", ");
      std::string types;
      for (size_t i = 0; i < expected_types.size(); ++i) {
        if (i != 0) {
          types.append(", ");
        }
        absl::StrAppendFormat(&types, "%d", types[i]);
      }
      return absl::StrFormat(
          "key: %s\n"
          "expected_result: %d\n"
          "expected_candidates: %s\n"
          "expected_types: %s",
          key, expected_result, candidates, types);
    }
  } kTestCases[] = {
      {"あい", true, {"❕", "❣"}, {ZERO_QUERY_EMOJI, ZERO_QUERY_NONE}},
      {"猫", true, {"😾"}, {ZERO_QUERY_EMOJI}},
      {"あ", false, {}, {}},  // Do not look up for one-char non-Kanji key
      {"あい", true, {"❕", "❣"}, {ZERO_QUERY_EMOJI, ZERO_QUERY_NONE}},
      {"あいう", false, {}, {}},
      {"", false, {}, {}},
      {"ああ", true, {"( •̀ㅁ•́;)"}, {ZERO_QUERY_EMOTICON}}};

  for (const auto& test_case : kTestCases) {
    ASSERT_EQ(test_case.expected_candidates.size(),
              test_case.expected_types.size());

    const ConversionRequest request;
    std::vector<Result> results;
    constexpr uint16_t kId = 0;  // EOS
    peer.GetZeroQueryCandidatesForKey(request, test_case.key, zero_query_dict,
                                      kId, kId, &results);
    EXPECT_EQ(results.size(), test_case.expected_candidates.size());
    for (size_t i = 0; i < test_case.expected_candidates.size(); ++i) {
      EXPECT_EQ(results[i].value, test_case.expected_candidates[i]);
    }
  }
}

TEST_F(ZeroQueryDecoderTest, WeekdayZeroQuerySuggestion) {
  // Set clock to Wednesday, September 16, 2026 12:00:00 UTC.
  const ScopedClockMock clock(absl::FromCivil(
      absl::CivilSecond(2026, 9, 16, 12, 0, 0), absl::UTCTimeZone()));

  commands::Request request_proto;
  request_proto.set_zero_query_suggestion(true);

  // 1. Default (disabled): should not emit day-of-week candidates.
  {
    const Result history = CreateHistoryResult("9/16", "9/16");
    const ConversionRequest request = ConversionRequestBuilder()
                                          .SetHistoryResult(history)
                                          .SetRequest(request_proto)
                                          .Build();
    const std::vector<Result> results = decoder_->Decode(request);
    for (const Result& r : results) {
      EXPECT_NE(r.value, " (水)");
    }
  }

  // 2. Enabled via DecoderExperimentParams.
  request_proto.mutable_decoder_experiment_params()
      ->set_enable_weekday_zero_query_prediction(true);

  const struct TestCase {
    absl::string_view input;
    bool use_preceding_text;
    absl::string_view expected_value;
    absl::string_view expected_key;
  } kTestCases[] = {
      {"9/16", false, " (水)", "すい"},
      {"9/16", true, " (水)", "すい"},
      {"9/17", false, " (木)", "もく"},
      {"09/16", true, " (水)", "すい"},
      {"2026/09/16", false, " (水)", "すい"},
      {"9月16日", false, " (水)", "すい"},
      {"2026年9月16日", true, " (水)", "すい"},
      {"予定は９／１６", true, " (水)", "すい"},
      {"明日は９月１７日", true, " (木)", "もく"},
  };

  for (const auto& tc : kTestCases) {
    ConversionRequestBuilder builder;
    builder.SetRequest(request_proto);
    commands::Context context;
    Result history;
    if (tc.use_preceding_text) {
      context.set_preceding_text(tc.input);
      builder.SetContext(context);
    } else {
      history = CreateHistoryResult(tc.input, tc.input);
      builder.SetHistoryResult(history);
    }
    const ConversionRequest request = builder.Build();
    const std::vector<Result> results = decoder_->Decode(request);

    ASSERT_GE(results.size(), 1) << "Input: " << tc.input;
    EXPECT_EQ(results[0].value, tc.expected_value) << "Input: " << tc.input;
    EXPECT_EQ(results[0].key, tc.expected_key) << "Input: " << tc.input;
    EXPECT_EQ(results[0].lid, 0) << "Input: " << tc.input;
    EXPECT_EQ(results[0].rid, 0) << "Input: " << tc.input;
    EXPECT_EQ(results[0].wcost, -500) << "Input: " << tc.input;
    EXPECT_TRUE(results[0].attributes & converter::Attribute::DISABLE_RESCORING)
        << "Input: " << tc.input;
    EXPECT_TRUE(results[0].attributes & converter::Attribute::NO_LEARNING)
        << "Input: " << tc.input;
    for (size_t i = 1; i < results.size(); ++i) {
      EXPECT_FALSE(results[i].value.starts_with(" （"))
          << "Redundant full-width weekday candidate: " << results[i].value;
      EXPECT_FALSE(results[i].value.ends_with("曜日"))
          << "Redundant full-name weekday candidate: " << results[i].value;
      EXPECT_NE(results[i].value, "(");
      EXPECT_NE(results[i].value, " (");
      EXPECT_NE(results[i].value, "（");
      EXPECT_NE(results[i].value, " （");
    }
  }
}

TEST_F(ZeroQueryDecoderTest,
       WeekdayZeroQueryNotRepeatedAfterCommittingWeekday) {
  const ScopedClockMock clock(absl::FromCivil(
      absl::CivilSecond(2026, 9, 16, 12, 0, 0), absl::UTCTimeZone()));

  commands::Request request_proto;
  request_proto.set_zero_query_suggestion(true);
  request_proto.mutable_decoder_experiment_params()
      ->set_enable_weekday_zero_query_prediction(true);

  // Case 1: `preceding_text` is still "9/16" (not yet updated by client during
  // Session::CommitCandidate), while `history_result` contains " (水)".
  {
    commands::Context context;
    context.set_preceding_text("9/16");
    const Result history = CreateHistoryResult("すい", " (水)");
    const ConversionRequest request = ConversionRequestBuilder()
                                          .SetContext(context)
                                          .SetHistoryResult(history)
                                          .SetRequest(request_proto)
                                          .Build();
    const std::vector<Result> results = decoder_->Decode(request);
    for (const Result& r : results) {
      EXPECT_NE(r.value, " (水)");
    }
  }

  // Case 2: `preceding_text` is still "9/16", while `history_result` contains
  // concatenated segments "9/16 (水)".
  {
    commands::Context context;
    context.set_preceding_text("9/16");
    const Result history = CreateHistoryResult("9/16すい", "9/16 (水)");
    const ConversionRequest request = ConversionRequestBuilder()
                                          .SetContext(context)
                                          .SetHistoryResult(history)
                                          .SetRequest(request_proto)
                                          .Build();
    const std::vector<Result> results = decoder_->Decode(request);
    for (const Result& r : results) {
      EXPECT_NE(r.value, " (水)");
    }
  }

  // Case 3: `preceding_text` has been updated to "9/16 (水)" with empty
  // `history_result`.
  {
    commands::Context context;
    context.set_preceding_text("9/16 (水)");
    const ConversionRequest request = ConversionRequestBuilder()
                                          .SetContext(context)
                                          .SetRequest(request_proto)
                                          .Build();
    const std::vector<Result> results = decoder_->Decode(request);
    for (const Result& r : results) {
      EXPECT_NE(r.value, " (水)");
    }
  }
}

TEST_F(ZeroQueryDecoderTest,
       WeekdayZeroQueryWithSplitCommitAndMultiSegmentHistory) {
  const ScopedClockMock clock(absl::FromCivil(
      absl::CivilSecond(2026, 9, 16, 12, 0, 0), absl::UTCTimeZone()));

  commands::Request request_proto;
  request_proto.set_zero_query_suggestion(true);
  request_proto.mutable_decoder_experiment_params()
      ->set_enable_weekday_zero_query_prediction(true);

  // Case 1: `preceding_text` is "9/16" and `history_result` is "16" (e.g.,
  // after HistoryReconstructor extracts the trailing number token, or when
  // `preceding_text` already includes the last committed segment).
  {
    commands::Context context;
    context.set_preceding_text("9/16");
    const Result history = CreateHistoryResult("16", "16");
    const ConversionRequest request = ConversionRequestBuilder()
                                          .SetContext(context)
                                          .SetHistoryResult(history)
                                          .SetRequest(request_proto)
                                          .Build();
    const std::vector<Result> results = decoder_->Decode(request);
    ASSERT_GE(results.size(), 1);
    EXPECT_EQ(results[0].value, " (水)");
    EXPECT_EQ(results[0].key, "すい");
  }

  // Case 2: Multi-segment history where Segment 0 = "9" and Segment 1 = "16",
  // and "/" was committed directly to the editor (`preceding_text` = "9/").
  {
    commands::Context context;
    context.set_preceding_text("9/");
    Result history = CreateHistoryResult("916", "916");
    history.inner_segment_boundary.push_back(EncodeBoundary(1, 1));
    history.inner_segment_boundary.push_back(EncodeBoundary(2, 2));
    const ConversionRequest request = ConversionRequestBuilder()
                                          .SetContext(context)
                                          .SetHistoryResult(history)
                                          .SetRequest(request_proto)
                                          .Build();
    const std::vector<Result> results = decoder_->Decode(request);
    ASSERT_GE(results.size(), 1);
    EXPECT_EQ(results[0].value, " (水)");
    EXPECT_EQ(results[0].key, "すい");
  }

  // Case 3: Multi-segment history where Segment 0 = "9月" and Segment 1 =
  // "16日" (`last_history_value` = "16日", `full_history_value` = "9月16日").
  {
    Result history = CreateHistoryResult("9がつ16にち", "9月16日");
    history.inner_segment_boundary.push_back(EncodeBoundary(7, 4));
    history.inner_segment_boundary.push_back(EncodeBoundary(8, 5));
    const ConversionRequest request = ConversionRequestBuilder()
                                          .SetHistoryResult(history)
                                          .SetRequest(request_proto)
                                          .Build();
    const std::vector<Result> results = decoder_->Decode(request);
    ASSERT_GE(results.size(), 1);
    EXPECT_EQ(results[0].value, " (水)");
    EXPECT_EQ(results[0].key, "すい");
  }
}

TEST_F(ZeroQueryDecoderTest,
       WeekdayZeroQueryFiltersRedundantOpenParenthesesAndWeekdays) {
  const ScopedClockMock clock(absl::FromCivil(
      absl::CivilSecond(2026, 9, 16, 12, 0, 0), absl::UTCTimeZone()));

  commands::Request request_proto;
  request_proto.set_zero_query_suggestion(true);
  request_proto.mutable_decoder_experiment_params()
      ->set_enable_weekday_zero_query_prediction(true);

  const Result history = CreateHistoryResult("9/16", "9/16");
  const ConversionRequest request = ConversionRequestBuilder()
                                        .SetHistoryResult(history)
                                        .SetRequest(request_proto)
                                        .Build();

  std::vector<Result> results;
  auto add_candidate = [&](absl::string_view val) {
    Result r;
    r.value = std::string(val);
    results.push_back(std::move(r));
  };
  // Redundant candidates that might be predicted by SupplementalModel.
  add_candidate("(");
  add_candidate(" (");
  add_candidate("（");
  add_candidate(" （");
  add_candidate("()");
  add_candidate("（）");
  add_candidate(" (金)");
  add_candidate("（水）");
  add_candidate("水曜日");
  // Valid non-redundant candidate.
  add_candidate("の");

  ZeroQueryDecoderTestPeer peer(*decoder_);
  EXPECT_TRUE(peer.AggregateWeekdayZeroQuery(request, &results));

  ASSERT_EQ(results.size(), 2);
  EXPECT_EQ(results[0].value, "の");
  EXPECT_EQ(results[1].value, " (水)");
}

TEST_F(ZeroQueryDecoderTest, WeekdayZeroQuerySlidingWindowAndValidation) {
  commands::Request request_proto;
  request_proto.set_zero_query_suggestion(true);
  request_proto.mutable_decoder_experiment_params()
      ->set_enable_weekday_zero_query_prediction(true);

  // 1. Sliding window forward rollover: typed in late December 2026 -> Jan 2027
  {
    const ScopedClockMock clock(absl::FromCivil(
        absl::CivilSecond(2026, 12, 20, 12, 0, 0), absl::UTCTimeZone()));
    commands::Context context;
    // Jan 10, 2027 is Sunday ("日"). (Jan 10, 2026 was Saturday).
    context.set_preceding_text("1/10");
    const ConversionRequest request = ConversionRequestBuilder()
                                          .SetContext(context)
                                          .SetRequest(request_proto)
                                          .Build();
    const std::vector<Result> results = decoder_->Decode(request);
    ASSERT_GE(results.size(), 1);
    EXPECT_EQ(results[0].value, " (日)");
    EXPECT_EQ(results[0].key, "にち");
  }

  // 2. Sliding window backward rollover: typed in early January 2026 -> Dec
  // 2025
  {
    const ScopedClockMock clock(absl::FromCivil(
        absl::CivilSecond(2026, 1, 5, 12, 0, 0), absl::UTCTimeZone()));
    commands::Context context;
    // Dec 28, 2025 is Sunday ("日"). (Dec 28, 2026 is Monday).
    context.set_preceding_text("12/28");
    const ConversionRequest request = ConversionRequestBuilder()
                                          .SetContext(context)
                                          .SetRequest(request_proto)
                                          .Build();
    const std::vector<Result> results = decoder_->Decode(request);
    ASSERT_GE(results.size(), 1);
    EXPECT_EQ(results[0].value, " (日)");
    EXPECT_EQ(results[0].key, "にち");
  }

  // 3. Leap year validation: 2028/2/29 is valid (Tuesday), 2026/2/29 is invalid
  {
    const ScopedClockMock clock(absl::FromCivil(
        absl::CivilSecond(2026, 9, 16, 12, 0, 0), absl::UTCTimeZone()));
    {
      commands::Context context;
      context.set_preceding_text("2028/2/29");
      const ConversionRequest request = ConversionRequestBuilder()
                                            .SetContext(context)
                                            .SetRequest(request_proto)
                                            .Build();
      const std::vector<Result> results = decoder_->Decode(request);
      ASSERT_GE(results.size(), 1);
      EXPECT_EQ(results[0].value, " (火)");
      EXPECT_EQ(results[0].key, "か");
    }

    const absl::string_view kInvalidInputs[] = {
        "2/30", "2026/2/29", "13/1", "0/10", "192.168.1/24", "1/2/3",
    };
    for (const absl::string_view invalid_input : kInvalidInputs) {
      commands::Context context;
      context.set_preceding_text(invalid_input);
      const ConversionRequest request = ConversionRequestBuilder()
                                            .SetContext(context)
                                            .SetRequest(request_proto)
                                            .Build();
      const std::vector<Result> results = decoder_->Decode(request);
      for (const Result& r : results) {
        EXPECT_FALSE(r.value.starts_with(" ("))
            << "Unexpected date candidate for invalid input: " << invalid_input
            << " -> " << r.value;
      }
    }
  }
}

}  // namespace
}  // namespace mozc::prediction

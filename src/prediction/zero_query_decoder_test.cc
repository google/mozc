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

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
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

}  // namespace
}  // namespace mozc::prediction

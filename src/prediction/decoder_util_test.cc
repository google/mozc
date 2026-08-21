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

#include "prediction/decoder_util.h"

#include <cstddef>
#include <string>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/strings/string_view.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "prediction/result.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"

namespace mozc::prediction {
namespace {

using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::Token;

TEST(DecoderUtilTest, GetCandidateCutoffThreshold) {
  EXPECT_EQ(GetCandidateCutoffThreshold(ConversionRequest::PREDICTION),
            kPredictionMaxResultsSize);
  EXPECT_EQ(GetCandidateCutoffThreshold(ConversionRequest::PARTIAL_PREDICTION),
            kPredictionMaxResultsSize);
  EXPECT_EQ(GetCandidateCutoffThreshold(ConversionRequest::SUGGESTION),
            kSuggestionMaxResultsSize);
  EXPECT_EQ(GetCandidateCutoffThreshold(ConversionRequest::PARTIAL_SUGGESTION),
            kSuggestionMaxResultsSize);
}

TEST(DecoderUtilTest, ResultsSizeAdjuster) {
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetRequestType(ConversionRequest::SUGGESTION)
          .Build();
  std::vector<Result> results;
  results.resize(10);

  {
    const ResultsSizeAdjuster adjuster(request, &results);
    EXPECT_EQ(adjuster.cutoff_threshold(), kSuggestionMaxResultsSize);
    for (size_t i = 0; i < 300; ++i) {
      results.emplace_back();
    }
    // Added 300 results (>= 256 cutoff threshold).
  }
  // Should be rolled back to 10.
  EXPECT_EQ(results.size(), 10);

  {
    const ResultsSizeAdjuster adjuster(request, &results);
    for (size_t i = 0; i < 50; ++i) {
      results.emplace_back();
    }
    EXPECT_EQ(adjuster.GetAddedResults().size(), 50);
  }
  // Not exceeding cutoff threshold, size is kept.
  EXPECT_EQ(results.size(), 60);
}

TEST(DecoderUtilTest, IsNoisyNumberToken) {
  Token token;
  // Non-number key
  token.key = "とうきょう";
  token.value = "東京";
  EXPECT_FALSE(IsNoisyNumberToken("とうきょう", 15, token));

  // Key is "1"
  token.key = "10がつ";
  token.value = "10月";
  EXPECT_TRUE(IsNoisyNumberToken("1", 1, token));

  token.key = "1じ";
  token.value = "12時";
  EXPECT_TRUE(IsNoisyNumberToken("1", 1, token));

  token.key = "101ぴきわんちゃん";
  token.value = "101匹わんちゃん";
  EXPECT_TRUE(IsNoisyNumberToken("101", 3, token));

  token.key = "1";
  token.value = "1";
  EXPECT_FALSE(IsNoisyNumberToken("1", 1, token));

  token.key = "1がつ";
  token.value = "1月";
  EXPECT_FALSE(IsNoisyNumberToken("1", 1, token));
}

TEST(DecoderUtilTest, PredictiveLookupCallback) {
  std::vector<Result> results;
  constexpr int kZipCodeId = 100;
  constexpr int kUnknownId = 200;
  absl::btree_set<std::string> subsequent_chars = {"ん", "き"};

  PredictiveLookupCallback callback(UNIGRAM, 10, 3, subsequent_chars,
                                    kZipCodeId, kUnknownId, &results);

  // Test OnKey
  EXPECT_EQ(callback.OnKey("あん"),
            DictionaryInterface::Callback::TRAVERSE_CONTINUE);
  EXPECT_EQ(callback.OnKey("あさ"),
            DictionaryInterface::Callback::TRAVERSE_NEXT_KEY);

  // Test OnActualKey
  EXPECT_EQ(callback.OnActualKey("あん", "あん", 2),
            DictionaryInterface::Callback::TRAVERSE_CONTINUE);

  // Test OnToken
  Token token;
  token.key = "あん";
  token.value = "案";
  token.cost = 1000;
  token.lid = 1;
  token.rid = 1;

  EXPECT_EQ(callback.OnToken("あん", "あん", token),
            DictionaryInterface::Callback::TRAVERSE_CONTINUE);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].key, "あん");
  EXPECT_EQ(results[0].value, "案");
  EXPECT_GT(results[0].wcost, 1000);  // Spatial cost penalty added
}

}  // namespace
}  // namespace mozc::prediction

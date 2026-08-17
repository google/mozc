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

#include "prediction/dictionary_decoder.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/attribute.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc::prediction {
namespace {

using ::mozc::converter::Attribute;
using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::MockDictionary;
using ::mozc::dictionary::PosMatcher;
using ::mozc::dictionary::Token;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::StrEq;

struct InvokeCallbackWithKeyValues {
  using Callback = DictionaryInterface::Callback;

  template <class T, class U>
  void operator()(T, U, Callback* callback) {
    for (const auto& [key, value] : kv_list) {
      if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE ||
          callback->OnActualKey(key, key, false) !=
              Callback::TRAVERSE_CONTINUE) {
        return;
      }
      const Token token(key, value, MockDictionary::kDefaultCost,
                        MockDictionary::kDefaultPosId,
                        MockDictionary::kDefaultPosId, token_attribute);
      if (callback->OnToken(key, key, token) != Callback::TRAVERSE_CONTINUE) {
        return;
      }
    }
  }

  std::vector<std::pair<absl::string_view, absl::string_view>> kv_list;
  Token::Attribute token_attribute = Token::NONE;
};

struct InvokeCallbackWithTokens {
  using Callback = DictionaryInterface::Callback;

  template <class T, class U>
  void operator()(T, U, Callback* callback) {
    for (const Token& token : tokens) {
      if (callback->OnKey(token.key) != Callback::TRAVERSE_CONTINUE ||
          callback->OnActualKey(token.key, token.key, false) !=
              Callback::TRAVERSE_CONTINUE) {
        return;
      }
      if (callback->OnToken(token.key, token.key, token) !=
          Callback::TRAVERSE_CONTINUE) {
        return;
      }
    }
  }

  std::vector<Token> tokens;
};

void InsertInputSequence(absl::string_view text, composer::Composer* composer) {
  for (const char32_t codepoint : Util::Utf8ToUtf32(text)) {
    commands::KeyEvent key;
    if (codepoint <= 0x7F) {
      key.set_key_code(codepoint);
    } else {
      key.set_key_code('?');
      *key.mutable_key_string() = Util::CodepointToUtf8(codepoint);
    }
    composer->InsertCharacterKeyEvent(key);
  }
}

class DictionaryDecoderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config::ConfigHandler::GetDefaultConfig(&config_);
    table_ = std::make_shared<composer::Table>();
    table_->LoadFromFile("system://romanji-hiragana.tsv");
    composer_ = std::make_unique<composer::Composer>(table_, request_, config_);
  }

  void InitWithMockDictionary(std::unique_ptr<MockDictionary> mock_dict) {
    auto data_manager = std::make_unique<testing::MockDataManager>();
    pos_matcher_ =
        std::make_unique<PosMatcher>(data_manager->GetPosMatcherData());
    ASSERT_OK_AND_ASSIGN(modules_, engine::ModulesPresetBuilder()
                                       .PresetDictionary(std::move(mock_dict))
                                       .Build(std::move(data_manager)));
    decoder_ = std::make_unique<DictionaryDecoder>(*modules_);
  }

  void InitWithDefaultMock() {
    auto mock_dict = std::make_unique<MockDictionary>();
    EXPECT_CALL(*mock_dict, LookupPredictive(StrEq("ぐーぐるあ"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"ぐーぐるあどせんす", "グーグルアドセンス"},
        }});
    EXPECT_CALL(*mock_dict, LookupPredictive(StrEq("ぐーぐる"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"ぐーぐる", "グーグル"},
            {"ぐーぐるあどせんす", "グーグルアドセンス"},
        }});
    EXPECT_CALL(*mock_dict, LookupPrefix(StrEq("ぐーぐる"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"ぐーぐる", "グーグル"},
        }});
    EXPECT_CALL(*mock_dict, LookupPrefix(StrEq("ぐーぐるあ"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"ぐーぐる", "グーグル"},
        }});
    InitWithMockDictionary(std::move(mock_dict));
  }

  ConversionRequest CreatePredictionRequest(absl::string_view key) {
    composer_->Reset();
    InsertInputSequence(key, composer_.get());
    return ConversionRequestBuilder()
        .SetComposer(*composer_)
        .SetKey(key)
        .SetRequestType(ConversionRequest::PREDICTION)
        .Build();
  }

  ConversionRequest CreateSuggestionRequest(absl::string_view key) {
    composer_->Reset();
    InsertInputSequence(key, composer_.get());
    return ConversionRequestBuilder()
        .SetComposer(*composer_)
        .SetKey(key)
        .SetRequestType(ConversionRequest::SUGGESTION)
        .Build();
  }

  std::unique_ptr<engine::Modules> modules_;
  std::unique_ptr<DictionaryDecoder> decoder_;
  std::unique_ptr<PosMatcher> pos_matcher_;
  config::Config config_;
  commands::Request request_;
  std::shared_ptr<composer::Table> table_;
  std::unique_ptr<composer::Composer> composer_;
};

TEST_F(DictionaryDecoderTest, EmptyRequestReturnsEmpty) {
  InitWithDefaultMock();
  ConversionRequest request;
  std::vector<Result> results;
  int min_key_len = 0;
  decoder_->AggregateUnigram(request, &results, &min_key_len);
  EXPECT_TRUE(results.empty());
}

TEST_F(DictionaryDecoderTest, AggregateUnigramCandidate) {
  InitWithDefaultMock();
  constexpr absl::string_view kKey = "ぐーぐるあ";
  const ConversionRequest convreq = CreateSuggestionRequest(kKey);
  std::vector<Result> results;
  int min_unigram_key_len = 0;
  decoder_->AggregateUnigram(convreq, &results, &min_unigram_key_len);
  EXPECT_FALSE(results.empty());

  for (const auto& result : results) {
    EXPECT_TRUE(result.attributes & UNIGRAM);
    EXPECT_TRUE(result.key.starts_with(kKey));
  }
}

TEST_F(DictionaryDecoderTest, AggregatePrefixCandidates) {
  InitWithDefaultMock();
  constexpr absl::string_view kKey = "ぐーぐるあ";
  const ConversionRequest convreq = CreatePredictionRequest(kKey);
  std::vector<Result> results;
  decoder_->AggregatePrefix(convreq, &results);
  EXPECT_FALSE(results.empty());
  for (const auto& r : results) {
    EXPECT_TRUE(r.attributes & PREFIX);
    EXPECT_TRUE(r.attributes & Attribute::PARTIALLY_KEY_CONSUMED);
    EXPECT_NE(r.consumed_key_size, 0);
  }
}

TEST_F(DictionaryDecoderTest, BigramTest) {
  auto mock_dict = std::make_unique<MockDictionary>();
  EXPECT_CALL(*mock_dict, LookupPrefix(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*mock_dict, LookupPrefix(StrEq("ぐーぐる"), _, _))
      .WillRepeatedly(InvokeCallbackWithKeyValues{{
          {"ぐーぐる", "グーグル"},
      }});
  EXPECT_CALL(*mock_dict, LookupPrefix(StrEq("あどせんす"), _, _))
      .WillRepeatedly(InvokeCallbackWithKeyValues{{
          {"あどせんす", "アドセンス"},
      }});
  EXPECT_CALL(*mock_dict, LookupPredictive(StrEq("ぐーぐる"), _, _))
      .WillRepeatedly(InvokeCallbackWithKeyValues{{
          {"ぐーぐるあどせんす", "グーグルアドセンス"},
      }});
  InitWithMockDictionary(std::move(mock_dict));

  Result history_result;
  history_result.key = "ぐーぐる";
  history_result.value = "グーグル";

  composer_->Reset();
  InsertInputSequence("あ", composer_.get());

  const ConversionRequest convreq =
      ConversionRequestBuilder()
          .SetComposer(*composer_)
          .SetHistoryResult(history_result)
          .SetKey("あ")
          .SetRequestType(ConversionRequest::SUGGESTION)
          .Build();

  std::vector<Result> results;
  decoder_->AggregateBigram(convreq, &results);
  EXPECT_FALSE(results.empty());
  for (const auto& result : results) {
    EXPECT_TRUE(result.attributes & BIGRAM);
  }
}

TEST_F(DictionaryDecoderTest, UserDictionaryPredictionFilter) {
  constexpr absl::string_view kHiraganaA = "あ";
  constexpr absl::string_view kHiraganaAA = "ああ";
  constexpr auto kCost = MockDictionary::kDefaultCost;
  constexpr auto kPosId = MockDictionary::kDefaultPosId;

  auto mock_dict = std::make_unique<MockDictionary>();

  const std::vector<Token> a_tokens = {
      {kHiraganaA, "a", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a0", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a1", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "aaa", kCost, kPosId, kPosId, Token::USER_DICTIONARY},
  };
  const std::vector<Token> aa_tokens = {
      {kHiraganaAA, "bbb", 0, 0, 0, Token::USER_DICTIONARY},
  };

  EXPECT_CALL(*mock_dict, LookupPredictive(StrEq(kHiraganaA), _, _))
      .WillRepeatedly(InvokeCallbackWithTokens{a_tokens});
  EXPECT_CALL(*mock_dict, LookupPredictive(StrEq(kHiraganaAA), _, _))
      .WillRepeatedly(InvokeCallbackWithTokens{aa_tokens});

  InitWithMockDictionary(std::move(mock_dict));

  auto is_user_dictionary_result = [](const Result& res) {
    return (res.attributes & Attribute::USER_DICTIONARY) != 0;
  };

  {
    // Test prediction from input あ.
    const ConversionRequest convreq = CreatePredictionRequest(kHiraganaA);
    std::vector<Result> results;
    decoder_->AggregateUnigramForMixedConversion(convreq, &results);

    // Check if "aaa" is not filtered.
    auto iter =
        std::find_if(results.begin(), results.end(), [&](const Result& res) {
          return res.key == kHiraganaA && res.value == "aaa" &&
                 is_user_dictionary_result(res);
        });
    EXPECT_NE(results.end(), iter);

    // "bbb" is looked up from input "あ" but it will be filtered because it is
    // from user dictionary with unknown POS ID.
    iter = std::find_if(results.begin(), results.end(), [&](const Result& res) {
      return res.key == kHiraganaAA && res.value == "bbb" &&
             is_user_dictionary_result(res);
    });
    EXPECT_EQ(iter, results.end());
  }

  {
    // Test prediction from input ああ.
    const ConversionRequest convreq = CreatePredictionRequest(kHiraganaAA);
    std::vector<Result> results;
    decoder_->AggregateUnigramForMixedConversion(convreq, &results);

    // Check if "aaa" is not found as its key is あ.
    auto iter =
        std::find_if(results.begin(), results.end(), [&](const Result& res) {
          return res.key == kHiraganaA && res.value == "aaa" &&
                 is_user_dictionary_result(res);
        });
    EXPECT_EQ(iter, results.end());

    // Unlike the above case for "あ", "bbb" is now found because input key is
    // exactly "ああ".
    iter = std::find_if(results.begin(), results.end(), [&](const Result& res) {
      return res.key == kHiraganaAA && res.value == "bbb" &&
             is_user_dictionary_result(res);
    });
    EXPECT_NE(results.end(), iter);
  }
}

}  // namespace
}  // namespace mozc::prediction

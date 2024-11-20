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

#include "dictionary/system/system_dictionary.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "config/config_handler.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/dictionary_test_util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/system/system_dictionary_builder.h"
#include "dictionary/text_dictionary_loader.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

ABSL_FLAG(int32_t, dictionary_test_size, 100000,
          "Dictionary size for this test.");
ABSL_FLAG(int32_t, dictionary_reverse_lookup_test_size, 1000,
          "Number of tokens to run reverse lookup test.");
ABSL_DECLARE_FLAG(int32_t, min_key_length_to_use_small_cost_encoding);

namespace mozc {
namespace dictionary {
namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::Return;

class SystemDictionaryTest : public testing::TestWithTempUserProfile {
 protected:
  SystemDictionaryTest()
      : pos_matcher_(mock_data_manager_.GetPosMatcherData()),
        text_dict_(pos_matcher_),
        temp_dir_(testing::MakeTempDirectoryOrDie()),
        dic_fn_(FileUtil::JoinPath(temp_dir_.path(), "mozc.dic")) {
    const std::string dic_path = mozc::testing::GetSourceFileOrDie(
        {MOZC_DICT_DIR_COMPONENTS, "dictionary_oss", "dictionary00.txt"});
    text_dict_.LoadWithLineLimit(dic_path, "",
                                 absl::GetFlag(FLAGS_dictionary_test_size));
  }

  void SetUp() override {
    // Don't use small cost encoding by default.
    original_flags_min_key_length_to_use_small_cost_encoding_ =
        absl::GetFlag(FLAGS_min_key_length_to_use_small_cost_encoding);
    absl::SetFlag(&FLAGS_min_key_length_to_use_small_cost_encoding,
                  std::numeric_limits<int32_t>::max());

    request_.Clear();
    config::ConfigHandler::GetDefaultConfig(&config_);
  }

  void TearDown() override {
    absl::SetFlag(&FLAGS_min_key_length_to_use_small_cost_encoding,
                  original_flags_min_key_length_to_use_small_cost_encoding_);

    // This config initialization will be removed once ConversionRequest can
    // take config as an injected argument.
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  static ConversionRequest ConvReq(const config::Config &config,
                                   const commands::Request &request) {
    return ConversionRequestBuilder()
        .SetConfig(config)
        .SetRequest(request)
        .Build();
  }

  void BuildAndWriteSystemDictionary(absl::Span<Token *const> source,
                                     size_t num_tokens,
                                     const std::string &filename);
  std::unique_ptr<SystemDictionary> BuildSystemDictionary(
      absl::Span<Token *const> source,
      size_t num_tokens = std::numeric_limits<size_t>::max());
  bool CompareTokensForLookup(const Token &a, const Token &b,
                              bool reverse) const;

  const testing::MockDataManager mock_data_manager_;
  dictionary::PosMatcher pos_matcher_;
  TextDictionaryLoader text_dict_;

  config::Config config_;
  commands::Request request_;
  TempDirectory temp_dir_;
  const std::string dic_fn_;
  int original_flags_min_key_length_to_use_small_cost_encoding_;
};

Token *GetTokenPointer(Token &token) { return &token; }
Token *GetTokenPointer(const std::unique_ptr<Token> &token) {
  return token.get();
}

// Get pointers to the Tokens contained in `token_container`. Since the returned
// vector contains mutable pointers to the elements of `token_container`, it
// cannot be passed by const reference.
template <typename C>
std::vector<Token *> MakeTokenPointers(C *token_container) {
  std::vector<Token *> ptrs;
  std::transform(std::begin(*token_container), std::end(*token_container),
                 std::back_inserter(ptrs),
                 [](auto &token) { return GetTokenPointer(token); });
  return ptrs;
}

void SystemDictionaryTest::BuildAndWriteSystemDictionary(
    absl::Span<Token *const> source, size_t num_tokens,
    const std::string &filename) {
  SystemDictionaryBuilder builder;
  std::vector<Token *> tokens;
  tokens.reserve(std::min(source.size(), num_tokens));
  // Picks up first tokens.
  for (auto it = source.begin();
       tokens.size() < num_tokens && it != source.end(); ++it) {
    tokens.push_back(*it);
  }
  builder.BuildFromTokens(tokens);
  builder.WriteToFile(filename);
}

std::unique_ptr<SystemDictionary> SystemDictionaryTest::BuildSystemDictionary(
    absl::Span<Token *const> source, size_t num_tokens) {
  BuildAndWriteSystemDictionary(source, num_tokens, dic_fn_);
  return SystemDictionary::Builder(dic_fn_).Build().value();
}

// Returns true if they seem to be same
bool SystemDictionaryTest::CompareTokensForLookup(const Token &a,
                                                  const Token &b,
                                                  bool reverse) const {
  const bool key_value_check = reverse ? (a.key == b.value && a.value == b.key)
                                       : (a.key == b.key && a.value == b.value);
  if (!key_value_check) {
    return false;
  }
  const bool comp_cost = a.cost == b.cost;
  if (!comp_cost) {
    return false;
  }
  const bool spelling_match = (a.attributes & Token::SPELLING_CORRECTION) ==
                              (b.attributes & Token::SPELLING_CORRECTION);
  if (!spelling_match) {
    return false;
  }
  const bool id_match = (a.lid == b.lid) && (a.rid == b.rid);
  if (!id_match) {
    return false;
  }
  return true;
}

TEST_F(SystemDictionaryTest, Callback) {
  std::vector<Token> tokens = {
      {"star", "star"}, {"start", "start"}, {"starting", "starting"}};

  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(MakeTokenPointers(&tokens));
  ASSERT_TRUE(system_dic.get() != nullptr);

  {
    MockCallback mock_callback;
    EXPECT_CALL(mock_callback, OnKey(_))
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
    EXPECT_CALL(mock_callback, OnActualKey(_, _, _))
        .Times(AtLeast(1))
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
    EXPECT_CALL(mock_callback, OnToken(_, _, _))
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));

    EXPECT_CALL(mock_callback, OnKey(Eq("start")))
        .Times(1)
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));

    EXPECT_CALL(mock_callback, OnActualKey(Eq("start"), Eq("start"), Eq(0)))
        .Times(1)
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));

    EXPECT_CALL(mock_callback, OnToken(Eq("start"), Eq("start"), _))
        .Times(1)
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));

    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupPredictive("start", convreq, &mock_callback);
  }

  {
    MockCallback mock_callback;
    EXPECT_CALL(mock_callback, OnKey(Eq("start")))
        .Times(1)
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
    EXPECT_CALL(mock_callback, OnActualKey(Eq("start"), Eq("start"), Eq(0)))
        .Times(1)
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
    EXPECT_CALL(mock_callback, OnToken(Eq("start"), Eq("start"), _))
        .Times(1)
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));

    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupExact("start", convreq, &mock_callback);
  }

  {
    MockCallback mock_callback;
    EXPECT_CALL(mock_callback, OnKey(_))
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
    EXPECT_CALL(mock_callback, OnActualKey(_, _, _))
        .Times(AtLeast(1))
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
    EXPECT_CALL(mock_callback, OnToken(_, _, _))
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));

    EXPECT_CALL(mock_callback, OnKey(Eq("start")))
        .Times(1)
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
    EXPECT_CALL(mock_callback, OnActualKey(Eq("start"), Eq("start"), Eq(0)))
        .Times(1)
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
    EXPECT_CALL(mock_callback, OnToken(Eq("start"), Eq("start"), _))
        .Times(1)
        .WillRepeatedly(
            Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));

    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupPrefix("start", convreq, &mock_callback);
  }
}

TEST_F(SystemDictionaryTest, HasValue) {
  std::vector<Token> tokens;
  for (int i = 0; i < 4; ++i) {
    tokens.emplace_back(absl::StrFormat("きー%d", i),
                        absl::StrFormat("バリュー%d", i));
  }

  const std::string kFull = "ｆｕｌｌ";
  const std::string kHiragana = "ひらがな";
  const std::string kKatakanaKey = "かたかな";
  const std::string kKatakanaValue = "カタカナ";

  tokens.emplace_back("Mozc", "Mozc");                // Alphabet
  tokens.emplace_back("upper", "UPPER");              // Alphabet upper case
  tokens.emplace_back("full", kFull);                 // Alphabet full width
  tokens.emplace_back(kHiragana, kHiragana);          // Hiragana
  tokens.emplace_back(kKatakanaKey, kKatakanaValue);  // Katakana

  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(MakeTokenPointers(&tokens));
  ASSERT_TRUE(system_dic.get() != nullptr);

  EXPECT_TRUE(system_dic->HasValue("バリュー0"));
  EXPECT_TRUE(system_dic->HasValue("バリュー1"));
  EXPECT_TRUE(system_dic->HasValue("バリュー2"));
  EXPECT_TRUE(system_dic->HasValue("バリュー3"));
  EXPECT_FALSE(system_dic->HasValue("バリュー4"));
  EXPECT_FALSE(system_dic->HasValue("バリュー5"));
  EXPECT_FALSE(system_dic->HasValue("バリュー6"));

  EXPECT_TRUE(system_dic->HasValue("Mozc"));
  EXPECT_FALSE(system_dic->HasValue("mozc"));

  EXPECT_TRUE(system_dic->HasValue("UPPER"));
  EXPECT_FALSE(system_dic->HasValue("upper"));

  EXPECT_TRUE(system_dic->HasValue(kFull));
  EXPECT_FALSE(system_dic->HasValue("full"));

  EXPECT_TRUE(system_dic->HasValue(kHiragana));
  EXPECT_FALSE(system_dic->HasValue("ヒラガナ\n"));

  EXPECT_TRUE(system_dic->HasValue(kKatakanaValue));
  EXPECT_FALSE(system_dic->HasValue(kKatakanaKey));
}

TEST_F(SystemDictionaryTest, NormalWord) {
  Token token = {"あ", "亜", 100, 50, 70, Token::NONE};
  std::unique_ptr<SystemDictionary> system_dic = BuildSystemDictionary(
      {&token}, absl::GetFlag(FLAGS_dictionary_test_size));
  ASSERT_TRUE(system_dic);

  CollectTokenCallback callback;

  // Look up by exact key.
  const ConversionRequest convreq = ConvReq(config_, request_);
  system_dic->LookupPrefix(token.key, convreq, &callback);
  ASSERT_EQ(1, callback.tokens().size());
  EXPECT_TOKEN_EQ(token, callback.tokens().front());

  // Look up by prefix.
  callback.Clear();
  system_dic->LookupPrefix("あいう", convreq, &callback);
  ASSERT_EQ(1, callback.tokens().size());
  EXPECT_TOKEN_EQ(token, callback.tokens().front());

  // Nothing should be looked up.
  callback.Clear();
  system_dic->LookupPrefix("かきく", convreq, &callback);
  EXPECT_TRUE(callback.tokens().empty());
}

TEST_F(SystemDictionaryTest, SameWord) {
  std::vector<Token> tokens = {
      {"あ", "亜", 100, 50, 70, Token::NONE},
      {"あ", "亜", 150, 100, 200, Token::NONE},
      {"あ", "あ", 100, 1000, 2000, Token::NONE},
      {"あ", "亜", 1000, 2000, 3000, Token::NONE},
  };

  std::vector<Token *> source_tokens = MakeTokenPointers(&tokens);
  std::unique_ptr<SystemDictionary> system_dic = BuildSystemDictionary(
      source_tokens, absl::GetFlag(FLAGS_dictionary_test_size));
  ASSERT_TRUE(system_dic);

  // All the tokens should be looked up.
  CollectTokenCallback callback;
  const ConversionRequest convreq = ConvReq(config_, request_);
  system_dic->LookupPrefix("あ", convreq, &callback);
  EXPECT_TOKENS_EQ_UNORDERED(source_tokens, callback.tokens());
}

TEST_F(SystemDictionaryTest, LookupAllWords) {
  absl::Span<const std::unique_ptr<Token>> source_tokens = text_dict_.tokens();
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(MakeTokenPointers(&source_tokens),
                            absl::GetFlag(FLAGS_dictionary_test_size));
  ASSERT_TRUE(system_dic);

  // All the tokens should be looked up.
  const ConversionRequest convreq = ConvReq(config_, request_);
  for (size_t i = 0; i < source_tokens.size(); ++i) {
    CheckTokenExistenceCallback callback(source_tokens[i].get());
    system_dic->LookupPrefix(source_tokens[i]->key, convreq, &callback);
    EXPECT_TRUE(callback.found())
        << "Token was not found: " << PrintToken(*source_tokens[i]);
  }
}

TEST_F(SystemDictionaryTest, SimpleLookupPrefix) {
  const std::string k0 = "は";
  const std::string k1 = "はひふへほ";
  Token t0 = {k0, "aa", 0, 0, 0, Token::NONE};
  Token t1 = {k1, "bb", 0, 0, 0, Token::NONE};

  std::vector<Token *> source_tokens = {&t0, &t1};
  text_dict_.CollectTokens(&source_tokens);
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(source_tokens, 100);
  ASSERT_TRUE(system_dic);

  // |t0| should be looked up from |k1|.
  CheckTokenExistenceCallback callback(&t0);
  const ConversionRequest convreq = ConvReq(config_, request_);
  system_dic->LookupPrefix(k1, convreq, &callback);
  EXPECT_TRUE(callback.found());
}

class LookupPrefixTestCallback : public SystemDictionary::Callback {
 public:
  ResultType OnKey(absl::string_view key) override {
    if (key == "かき") {
      return TRAVERSE_CULL;
    } else if (key == "さ") {
      return TRAVERSE_NEXT_KEY;
    } else if (key == "た") {
      return TRAVERSE_DONE;
    }
    return TRAVERSE_CONTINUE;
  }

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token &token) override {
    result_.insert(std::make_pair(token.key, token.value));
    return TRAVERSE_CONTINUE;
  }

  const std::set<std::pair<std::string, std::string>> &result() const {
    return result_;
  }

 private:
  std::set<std::pair<std::string, std::string>> result_;
};

TEST_F(SystemDictionaryTest, LookupPrefix) {
  // Set up a test dictionary.
  struct {
    const char *key;
    const char *value;
  } kKeyValues[] = {
      {"あ", "亜"},       {"あ", "安"},         {"あ", "在"},
      {"あい", "愛"},     {"あい", "藍"},       {"あいう", "藍雨"},
      {"か", "可"},       {"かき", "牡蠣"},     {"かき", "夏季"},
      {"かきく", "柿久"}, {"さ", "差"},         {"さ", "左"},
      {"さし", "刺"},     {"た", "田"},         {"た", "多"},
      {"たち", "多値"},   {"たちつ", "タチツ"}, {"は", "葉"},
      {"は", "歯"},       {"はひ", "ハヒ"},     {"ば", "場"},
      {"はび", "波美"},   {"ばび", "馬尾"},     {"ばびぶ", "バビブ"},
  };
  constexpr size_t kKeyValuesSize = std::size(kKeyValues);
  std::vector<Token> tokens;
  tokens.reserve(kKeyValuesSize);
  for (const auto &kv : kKeyValues) {
    tokens.emplace_back(kv.key, kv.value);
  }
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(MakeTokenPointers(&tokens), kKeyValuesSize);
  ASSERT_TRUE(system_dic);

  // Test for normal prefix lookup without key expansion.
  {
    LookupPrefixTestCallback callback;
    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupPrefix("あい",  // "あい"
                             convreq, &callback);
    const std::set<std::pair<std::string, std::string>> &result =
        callback.result();
    // "あ" -- "あい" should be found.
    for (size_t i = 0; i < 5; ++i) {
      const std::pair<std::string, std::string> entry(kKeyValues[i].key,
                                                      kKeyValues[i].value);
      EXPECT_TRUE(result.end() != result.find(entry));
    }
    // The others should not be found.
    for (size_t i = 5; i < std::size(kKeyValues); ++i) {
      const std::pair<std::string, std::string> entry(kKeyValues[i].key,
                                                      kKeyValues[i].value);
      EXPECT_TRUE(result.end() == result.find(entry));
    }
  }

  // Test for normal prefix lookup without key expansion, but with culling
  // feature.
  {
    LookupPrefixTestCallback callback;
    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupPrefix("かきく", convreq, &callback);
    const std::set<std::pair<std::string, std::string>> &result =
        callback.result();
    // Only "か" should be found as the callback doesn't traverse the subtree of
    // "かき" due to culling request from LookupPrefixTestCallback::OnKey().
    for (size_t i = 0; i < kKeyValuesSize; ++i) {
      const std::pair<std::string, std::string> entry(kKeyValues[i].key,
                                                      kKeyValues[i].value);
      EXPECT_EQ(entry.first == "か", result.find(entry) != result.end());
    }
  }

  // Test for TRAVERSE_NEXT_KEY.
  {
    LookupPrefixTestCallback callback;
    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupPrefix("さしす", convreq, &callback);
    const std::set<std::pair<std::string, std::string>> &result =
        callback.result();
    // Only "さし" should be found as tokens for "さ" is skipped (see
    // LookupPrefixTestCallback::OnKey()).
    for (size_t i = 0; i < kKeyValuesSize; ++i) {
      const std::pair<std::string, std::string> entry(kKeyValues[i].key,
                                                      kKeyValues[i].value);
      EXPECT_EQ(entry.first == "さし", result.find(entry) != result.end());
    }
  }

  // Test for TRAVERSE_DONE.
  {
    LookupPrefixTestCallback callback;
    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupPrefix("たちつ", convreq, &callback);
    const std::set<std::pair<std::string, std::string>> &result =
        callback.result();
    // Nothing should be found as the traversal is immediately done after seeing
    // "た"; see LookupPrefixTestCallback::OnKey().
    EXPECT_TRUE(result.empty());
  }

  // Test for prefix lookup with key expansion.
  {
    LookupPrefixTestCallback callback;
    // Use kana modifier insensitive lookup
    request_.set_kana_modifier_insensitive_conversion(true);
    config_.set_use_kana_modifier_insensitive_conversion(true);
    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupPrefix("はひ", convreq, &callback);
    const std::set<std::pair<std::string, std::string>> &result =
        callback.result();
    const char *kExpectedKeys[] = {
        "は", "ば", "はひ", "ばひ", "はび", "ばび",
    };
    const absl::btree_set<std::string> expected(
        kExpectedKeys, kExpectedKeys + std::size(kExpectedKeys));
    for (size_t i = 0; i < kKeyValuesSize; ++i) {
      const bool to_be_found =
          expected.find(kKeyValues[i].key) != expected.end();
      const std::pair<std::string, std::string> entry(kKeyValues[i].key,
                                                      kKeyValues[i].value);
      EXPECT_EQ(result.find(entry) != result.end(), to_be_found);
    }
  }
}

TEST_F(SystemDictionaryTest, LookupPredictive) {
  Token tokens[] = {
      {"まみむめもや", "value0", 0, 0, 0, Token::NONE},
      {"まみむめもやゆよ", "value1", 0, 0, 0, Token::NONE},
  };

  // Build a dictionary with the above two tokens plus those from test data.
  std::vector<Token *> source_tokens = MakeTokenPointers(&tokens);
  text_dict_.CollectTokens(&source_tokens);  // Load test data.
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(source_tokens, 10000);
  ASSERT_TRUE(system_dic);

  // All the tokens in |tokens| should be looked up by "まみむめも".
  constexpr char kMamimumemo[] = "まみむめも";
  CheckMultiTokensExistenceCallback callback({&tokens[0], &tokens[1]});
  const ConversionRequest convreq = ConvReq(config_, request_);
  system_dic->LookupPredictive(kMamimumemo, convreq, &callback);
  EXPECT_TRUE(callback.AreAllFound());
}

TEST_F(SystemDictionaryTest, LookupPredictiveKanaModifierInsensitiveLookup) {
  Token tokens[] = {
      {"がっこう", "学校", 0, 0, 0, Token::NONE},
      {"かっこう", "格好", 0, 0, 0, Token::NONE},
  };
  const std::vector<Token *> source_tokens = {&tokens[0], &tokens[1]};
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(source_tokens, 100);
  ASSERT_TRUE(system_dic);

  const std::string kKey = "かつこう";

  // Without Kana modifier insensitive lookup flag, nothing is looked up.
  CollectTokenCallback callback;
  request_.set_kana_modifier_insensitive_conversion(false);
  config_.set_use_kana_modifier_insensitive_conversion(false);
  const ConversionRequest convreq1 = ConvReq(config_, request_);
  system_dic->LookupPredictive(kKey, convreq1, &callback);
  EXPECT_TRUE(callback.tokens().empty());

  // With Kana modifier insensitive lookup flag, every token is looked up.
  callback.Clear();
  request_.set_kana_modifier_insensitive_conversion(true);
  config_.set_use_kana_modifier_insensitive_conversion(true);
  const ConversionRequest convreq2 = ConvReq(config_, request_);
  system_dic->LookupPredictive(kKey, convreq2, &callback);
  EXPECT_TOKENS_EQ_UNORDERED(source_tokens, callback.tokens());
}

TEST_F(SystemDictionaryTest, LookupPredictiveCutOffEmulatingBFS) {
  Token tokens[] = {
      {"あい", "ai", 0, 0, 0, Token::NONE},
      {"あいうえお", "aiueo", 0, 0, 0, Token::NONE},
  };
  // Build a dictionary with the above two tokens plus those from test data.
  std::vector<Token *> source_tokens = MakeTokenPointers(&tokens);
  text_dict_.CollectTokens(&source_tokens);  // Load test data.
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(source_tokens, 10000);
  ASSERT_TRUE(system_dic);

  // Since there are many entries starting with "あ" in test dictionary, it's
  // expected that "あいうえお" is not looked up because of longer key cut-off
  // mechanism.  However, "あい" is looked up as it's short.
  CheckMultiTokensExistenceCallback callback({&tokens[0], &tokens[1]});
  const ConversionRequest convreq = ConvReq(config_, request_);
  system_dic->LookupPredictive("あ", convreq, &callback);
  EXPECT_TRUE(callback.IsFound(&tokens[0]));
  EXPECT_FALSE(callback.IsFound(&tokens[1]));
}

TEST_F(SystemDictionaryTest, LookupExact) {
  const std::string k0 = "は";
  const std::string k1 = "はひふへほ";
  Token t0 = {k0, "aa", 0, 0, 0, Token::NONE};
  Token t1 = {k1, "bb", 0, 0, 0, Token::NONE};
  std::vector<Token *> source_tokens = {&t0, &t1};
  text_dict_.CollectTokens(&source_tokens);
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(source_tokens, 100);
  ASSERT_TRUE(system_dic);

  // |t0| should not be looked up from |k1|.
  CheckTokenExistenceCallback callback0(&t0);
  const ConversionRequest convreq = ConvReq(config_, request_);
  system_dic->LookupExact(k1, convreq, &callback0);
  EXPECT_FALSE(callback0.found());
  // But |t1| should be found.
  CheckTokenExistenceCallback callback1(&t1);
  system_dic->LookupExact(k1, convreq, &callback1);
  EXPECT_TRUE(callback1.found());

  // Nothing should be found from "hoge".
  CollectTokenCallback callback_hoge;
  system_dic->LookupExact("hoge", convreq, &callback_hoge);
  EXPECT_TRUE(callback_hoge.tokens().empty());
}

TEST_F(SystemDictionaryTest, LookupReverse) {
  Token tokens[] = {
      {"ど", "ド", 1, 2, 3, Token::NONE},
      {"どらえもん", "ドラえもん", 1, 2, 3, Token::NONE},
      {"といざらす®", "トイザらス®", 1, 2, 3, Token::NONE},
      // Both token[3] and token[4] will be encoded into 3 bytes.
      {"ああああああ", "ああああああ", 32000, 1, 1, Token::NONE},
      {"ああああああ", "ああああああ", 32000, 1, 2, Token::NONE},
      // token[5] will be encoded into 3 bytes.
      {"いいいいいい", "いいいいいい", 32000, 1, 1, Token::NONE},
      {"どらえもん", "ドラえもん", 1, 2, 3, Token::SPELLING_CORRECTION},
      {"こんさーと", "コンサート", 1, 1, 1, Token::NONE},
      {"ばーじょん", "バージョン", 1, 1, 1, Token::NONE},
  };
  std::vector<Token *> source_tokens = MakeTokenPointers(&tokens);
  text_dict_.CollectTokens(&source_tokens);
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(source_tokens, source_tokens.size());
  ASSERT_TRUE(system_dic);

  const size_t test_size =
      std::min<size_t>(absl::GetFlag(FLAGS_dictionary_reverse_lookup_test_size),
                       source_tokens.size());
  for (size_t source_index = 0; source_index < test_size; ++source_index) {
    const Token &source_token = *source_tokens[source_index];
    CollectTokenCallback callback;
    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupReverse(source_token.value, convreq, &callback);

    bool found = false;
    for (const Token &token : callback.tokens()) {
      // Make sure any of the key lengths of the lookup results
      // doesn't exceed the original key length.
      // It happened once
      // when called with "バージョン", returning "ヴァージョン".
      EXPECT_LE(token.key.size(), source_token.value.size())
          << token.key << ":" << token.value << "\t" << source_token.value;
      if (CompareTokensForLookup(source_token, token, true)) {
        found = true;
      }
    }

    if ((source_token.attributes & Token::SPELLING_CORRECTION) ==
        Token::SPELLING_CORRECTION) {
      EXPECT_FALSE(found) << "Spelling correction token was retrieved:"
                          << PrintToken(source_token);
      if (found) {
        return;
      }
    } else {
      EXPECT_TRUE(found) << "Failed to find " << source_token.key << ":"
                         << source_token.value;
      if (!found) {
        return;
      }
    }
  }

  {
    // test for non exact transliterated index string.
    // append "が"
    const std::string key = absl::StrCat(tokens[7].value, "が");
    CollectTokenCallback callback;
    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupReverse(key, convreq, &callback);
    bool found = false;
    for (const Token &token : callback.tokens()) {
      if (CompareTokensForLookup(tokens[7], token, true)) {
        found = true;
      }
    }
    EXPECT_TRUE(found) << "Missed token for non exact transliterated index "
                       << key;
  }
}

TEST_F(SystemDictionaryTest, LookupReverseIndex) {
  absl::Span<const std::unique_ptr<Token>> source_tokens = text_dict_.tokens();
  BuildAndWriteSystemDictionary(MakeTokenPointers(&source_tokens),
                                absl::GetFlag(FLAGS_dictionary_test_size),
                                dic_fn_);

  std::unique_ptr<SystemDictionary> system_dic_without_index =
      SystemDictionary::Builder(dic_fn_)
          .SetOptions(SystemDictionary::NONE)
          .Build()
          .value();
  ASSERT_TRUE(system_dic_without_index)
      << "Failed to open dictionary source:" << dic_fn_;
  std::unique_ptr<SystemDictionary> system_dic_with_index =
      SystemDictionary::Builder(dic_fn_)
          .SetOptions(SystemDictionary::ENABLE_REVERSE_LOOKUP_INDEX)
          .Build()
          .value();
  ASSERT_TRUE(system_dic_with_index)
      << "Failed to open dictionary source:" << dic_fn_;

  int size = absl::GetFlag(FLAGS_dictionary_reverse_lookup_test_size);
  for (auto it = source_tokens.begin(); size > 0 && it != source_tokens.end();
       ++it, --size) {
    const Token &t = **it;
    CollectTokenCallback callback1, callback2;
    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic_without_index->LookupReverse(t.value, convreq, &callback1);
    system_dic_with_index->LookupReverse(t.value, convreq, &callback2);

    absl::Span<const Token> tokens1 = callback1.tokens();
    absl::Span<const Token> tokens2 = callback2.tokens();
    ASSERT_EQ(tokens1.size(), tokens2.size());
    for (size_t i = 0; i < tokens1.size(); ++i) {
      EXPECT_TOKEN_EQ(tokens1[i], tokens2[i]);
    }
  }
}

TEST_F(SystemDictionaryTest, LookupReverseWithCache) {
  const std::string kDoraemon = "ドラえもん";

  Token source_token;
  source_token.key = "どらえもん";
  source_token.value = kDoraemon;
  source_token.cost = 1;
  source_token.lid = 2;
  source_token.rid = 3;
  std::vector<Token *> source_tokens = {&source_token};
  text_dict_.CollectTokens(&source_tokens);
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(source_tokens, source_tokens.size());
  ASSERT_TRUE(system_dic);
  system_dic->PopulateReverseLookupCache(kDoraemon);

  Token target_token = source_token;
  target_token.key.swap(target_token.value);

  CheckTokenExistenceCallback callback(&target_token);
  const ConversionRequest convreq = ConvReq(config_, request_);
  system_dic->LookupReverse(kDoraemon, convreq, &callback);
  EXPECT_TRUE(callback.found())
      << "Could not find " << PrintToken(source_token);
  system_dic->ClearReverseLookupCache();
}

TEST_F(SystemDictionaryTest, SpellingCorrectionTokens) {
  std::vector<Token> tokens = {
      {"あぼがど", "アボカド", 1, 0, 2, Token::SPELLING_CORRECTION},
      {"しゅみれーしょん", "シミュレーション", 1, 100, 3,
       Token::SPELLING_CORRECTION},
      {"あきはばら", "秋葉原", 1000, 1, 2, Token::NONE},
  };

  std::vector<Token *> source_tokens = MakeTokenPointers(&tokens);
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(source_tokens, source_tokens.size());
  ASSERT_TRUE(system_dic);

  for (size_t i = 0; i < source_tokens.size(); ++i) {
    CheckTokenExistenceCallback callback(source_tokens[i]);
    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupPrefix(source_tokens[i]->key, convreq, &callback);
    EXPECT_TRUE(callback.found())
        << "Token " << i << " was not found: " << PrintToken(*source_tokens[i]);
  }
}

TEST_F(SystemDictionaryTest, EnableNoModifierTargetWithLoudsTrie) {
  const std::string k0 = "かつ";
  const std::string k1 = "かっこ";
  const std::string k2 = "かつこう";
  const std::string k3 = "かっこう";
  const std::string k4 = "がっこう";

  Token tokens[5] = {
      {k0, "aa", 0, 0, 0, Token::NONE}, {k1, "bb", 0, 0, 0, Token::NONE},
      {k2, "cc", 0, 0, 0, Token::NONE}, {k3, "dd", 0, 0, 0, Token::NONE},
      {k4, "ee", 0, 0, 0, Token::NONE},
  };

  std::vector<Token *> source_tokens = MakeTokenPointers(&tokens);
  text_dict_.CollectTokens(&source_tokens);
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(source_tokens, 100);
  ASSERT_TRUE(system_dic);

  request_.set_kana_modifier_insensitive_conversion(true);
  config_.set_use_kana_modifier_insensitive_conversion(true);

  // Prefix search
  for (size_t i = 0; i < std::size(tokens); ++i) {
    CheckTokenExistenceCallback callback(&tokens[i]);
    // "かつこう" -> "かつ", "かっこ", "かつこう", "かっこう" and "がっこう"
    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupPrefix(k2, convreq, &callback);
    EXPECT_TRUE(callback.found())
        << "Token " << i << " was not found: " << PrintToken(tokens[i]);
  }

  // Predictive searches
  {
    // "かつ" -> "かつ", "かっこ", "かつこう", "かっこう" and "がっこう"
    std::vector<Token *> expected = MakeTokenPointers(&tokens);
    CheckMultiTokensExistenceCallback callback(expected);
    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupPredictive(k0, convreq, &callback);
    EXPECT_TRUE(callback.AreAllFound());
  }
  {
    // "かっこ" -> "かっこ", "かっこう" and "がっこう"
    std::vector<Token *> expected = {&tokens[1], &tokens[3], &tokens[4]};
    CheckMultiTokensExistenceCallback callback(expected);
    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupPredictive(k1, convreq, &callback);
    EXPECT_TRUE(callback.AreAllFound());
  }
}

TEST_F(SystemDictionaryTest, NoModifierForKanaEntries) {
  Token t0 = {"ていすてぃんぐ", "テイスティング", 0, 0, 0, Token::NONE};
  Token t1 = {"てすとです", "てすとです", 0, 0, 0, Token::NONE};

  std::vector<Token *> source_tokens = {&t0, &t1};
  text_dict_.CollectTokens(&source_tokens);
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(source_tokens, 100);
  ASSERT_TRUE(system_dic);

  // Lookup |t0| from "ていすていんぐ"
  const std::string k = "ていすていんぐ";
  request_.set_kana_modifier_insensitive_conversion(true);
  config_.set_use_kana_modifier_insensitive_conversion(true);
  CheckTokenExistenceCallback callback(&t0);
    const ConversionRequest convreq = ConvReq(config_, request_);
  system_dic->LookupPrefix(k, convreq, &callback);
  EXPECT_TRUE(callback.found()) << "Not found: " << PrintToken(t0);
}

TEST_F(SystemDictionaryTest, DoNotReturnNoModifierTargetWithLoudsTrie) {
  const std::string k0 = "かつ";
  const std::string k1 = "かっこ";
  const std::string k2 = "かつこう";
  const std::string k3 = "かっこう";
  const std::string k4 = "がっこう";

  Token tokens[5] = {
      {k0, "aa", 0, 0, 0, Token::NONE}, {k1, "bb", 0, 0, 0, Token::NONE},
      {k2, "cc", 0, 0, 0, Token::NONE}, {k3, "dd", 0, 0, 0, Token::NONE},
      {k4, "ee", 0, 0, 0, Token::NONE},
  };
  std::vector<Token *> source_tokens = MakeTokenPointers(&tokens);
  text_dict_.CollectTokens(&source_tokens);
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(source_tokens, 100);
  ASSERT_TRUE(system_dic);

  request_.set_kana_modifier_insensitive_conversion(false);
  config_.set_use_kana_modifier_insensitive_conversion(false);

  // Prefix search
  {
    // "かっこう" (k3) -> "かっこ" (k1) and "かっこう" (k3)
    // Make sure "がっこう" is not in the results when searched by "かっこう"
    std::vector<Token *> to_be_looked_up = {&tokens[1], &tokens[3]};
    std::vector<Token *> not_to_be_looked_up = {&tokens[0], &tokens[2],
                                                &tokens[4]};
    for (size_t i = 0; i < to_be_looked_up.size(); ++i) {
      CheckTokenExistenceCallback callback(to_be_looked_up[i]);
      const ConversionRequest convreq = ConvReq(config_, request_);
      system_dic->LookupPrefix(k3, convreq, &callback);
      EXPECT_TRUE(callback.found())
          << "Token is not found: " << PrintToken(*to_be_looked_up[i]);
    }
    for (size_t i = 0; i < not_to_be_looked_up.size(); ++i) {
      CheckTokenExistenceCallback callback(not_to_be_looked_up[i]);
      const ConversionRequest convreq = ConvReq(config_, request_);
      system_dic->LookupPrefix(k3, convreq, &callback);
      EXPECT_FALSE(callback.found()) << "Token should not be found: "
                                     << PrintToken(*not_to_be_looked_up[i]);
    }
  }

  // Predictive search
  {
    // "かっこ" -> "かっこ" and "かっこう"
    // Make sure "がっこう" is not in the results when searched by "かっこ"
    std::vector<Token *> to_be_looked_up = {&tokens[1], &tokens[3]};
    std::vector<Token *> not_to_be_looked_up = {&tokens[0], &tokens[2],
                                                &tokens[4]};
    for (size_t i = 0; i < to_be_looked_up.size(); ++i) {
      CheckTokenExistenceCallback callback(to_be_looked_up[i]);
      const ConversionRequest convreq = ConvReq(config_, request_);
      system_dic->LookupPredictive(k1, convreq, &callback);
      EXPECT_TRUE(callback.found())
          << "Token is not found: " << PrintToken(*to_be_looked_up[i]);
    }
    for (size_t i = 0; i < not_to_be_looked_up.size(); ++i) {
      CheckTokenExistenceCallback callback(not_to_be_looked_up[i]);
      const ConversionRequest convreq = ConvReq(config_, request_);
      system_dic->LookupPredictive(k3, convreq, &callback);
      EXPECT_FALSE(callback.found()) << "Token should not be found: "
                                     << PrintToken(*not_to_be_looked_up[i]);
    }
  }
}

TEST_F(SystemDictionaryTest, ShouldNotUseSmallCostEncodingForHeteronyms) {
  absl::SetFlag(&FLAGS_min_key_length_to_use_small_cost_encoding,
                original_flags_min_key_length_to_use_small_cost_encoding_);

  std::vector<Token> tokens = {
      {"しょうろんぽう", "ショウロンポウ", 5948, 100, 100, Token::NONE},
      {"しょうろんぽう", "小籠包", 7692, 100, 100, Token::NONE},
      {"しょーろんぽう", "ショーロンポウ", 6092, 200, 200, Token::NONE},
      {"しょーろんぽう", "小籠包", 9000, 100, 100, Token::NONE},
  };

  std::vector<Token *> source_tokens = MakeTokenPointers(&tokens);
  std::unique_ptr<SystemDictionary> system_dic =
      BuildSystemDictionary(source_tokens, source_tokens.size());
  ASSERT_TRUE(system_dic);

  for (size_t i = 0; i < source_tokens.size(); ++i) {
    CheckTokenExistenceCallback callback(source_tokens[i]);
    const ConversionRequest convreq = ConvReq(config_, request_);
    system_dic->LookupPrefix(source_tokens[i]->key, convreq, &callback);
    // The original token will not be found when the token is encoded with
    // small_cost_encoding
    EXPECT_TRUE(callback.found())
        << "Token " << i << " was not found: " << PrintToken(*source_tokens[i]);
  }
}

}  // namespace
}  // namespace dictionary
}  // namespace mozc

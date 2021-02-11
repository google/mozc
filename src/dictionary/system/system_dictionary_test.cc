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
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/file_util.h"
#include "base/flags.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/stl_util.h"
#include "base/system_util.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_test_util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/system_dictionary_builder.h"
#include "dictionary/text_dictionary_loader.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"
#include "absl/strings/string_view.h"

MOZC_FLAG(int32, dictionary_test_size, 100000,
          "Dictionary size for this test.");
MOZC_FLAG(int32, dictionary_reverse_lookup_test_size, 1000,
          "Number of tokens to run reverse lookup test.");
MOZC_DECLARE_FLAG(int32, min_key_length_to_use_small_cost_encoding);

namespace mozc {
namespace dictionary {

class SystemDictionaryTest : public ::testing::Test {
 protected:
  SystemDictionaryTest()
      : pos_matcher_(mock_data_manager_.GetPOSMatcherData()),
        text_dict_(new TextDictionaryLoader(pos_matcher_)),
        dic_fn_(
            FileUtil::JoinPath(mozc::GetFlag(FLAGS_test_tmpdir), "mozc.dic")) {
    const std::string dic_path = mozc::testing::GetSourceFileOrDie(
        {"data", "dictionary_oss", "dictionary00.txt"});
    text_dict_->LoadWithLineLimit(dic_path, "",
                                  mozc::GetFlag(FLAGS_dictionary_test_size));

    convreq_.set_request(&request_);
    convreq_.set_config(&config_);
  }

  void SetUp() override {
    // Don't use small cost encoding by default.
    original_flags_min_key_length_to_use_small_cost_encoding_ =
        mozc::GetFlag(FLAGS_min_key_length_to_use_small_cost_encoding);
    mozc::SetFlag(&FLAGS_min_key_length_to_use_small_cost_encoding,
                  std::numeric_limits<int32>::max());

    request_.Clear();
    config::ConfigHandler::GetDefaultConfig(&config_);
  }

  void TearDown() override {
    mozc::SetFlag(&FLAGS_min_key_length_to_use_small_cost_encoding,
                  original_flags_min_key_length_to_use_small_cost_encoding_);

    // This config initialization will be removed once ConversionRequest can
    // take config as an injected argument.
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  void BuildSystemDictionary(const std::vector<Token *> &tokens,
                             int num_tokens);
  Token *CreateToken(const std::string &key, const std::string &value) const;
  bool CompareTokensForLookup(const Token &a, const Token &b,
                              bool reverse) const;

  const testing::ScopedTmpUserProfileDirectory scoped_profile_dir_;
  const testing::MockDataManager mock_data_manager_;
  dictionary::POSMatcher pos_matcher_;
  std::unique_ptr<TextDictionaryLoader> text_dict_;

  ConversionRequest convreq_;
  config::Config config_;
  commands::Request request_;
  const std::string dic_fn_;
  int original_flags_min_key_length_to_use_small_cost_encoding_;
};

void SystemDictionaryTest::BuildSystemDictionary(
    const std::vector<Token *> &source, int num_tokens) {
  SystemDictionaryBuilder builder;
  std::vector<Token *> tokens;
  // Picks up first tokens.
  for (std::vector<Token *>::const_iterator it = source.begin();
       tokens.size() < num_tokens && it != source.end(); ++it) {
    tokens.push_back(*it);
  }
  builder.BuildFromTokens(tokens);
  builder.WriteToFile(dic_fn_);
}

Token *SystemDictionaryTest::CreateToken(const std::string &key,
                                         const std::string &value) const {
  Token *t = new Token;
  t->key = key;
  t->value = value;
  t->cost = 0;
  t->lid = 0;
  t->rid = 0;
  return t;
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

TEST_F(SystemDictionaryTest, HasValue) {
  std::vector<Token *> tokens;
  for (int i = 0; i < 4; ++i) {
    Token *token = new Token;
    token->key = Util::StringPrintf("きー%d", i);
    token->value = Util::StringPrintf("バリュー%d", i);
    tokens.push_back(token);
  }

  {  // Alphabet
    Token *token = new Token;
    token->key = "Mozc";
    token->value = "Mozc";
    tokens.push_back(token);
  }

  {  // Alphabet upper case
    Token *token = new Token;
    token->key = "upper";
    token->value = "UPPER";
    tokens.push_back(token);
  }

  const std::string kFull = "ｆｕｌｌ";
  const std::string kHiragana = "ひらがな";
  const std::string kKatakanaKey = "かたかな";
  const std::string kKatakanaValue = "カタカナ";

  {  // Alphabet full width
    Token *token = new Token;
    token->key = "full";
    token->value = kFull;
    tokens.push_back(token);
  }

  {  // Hiragana
    Token *token = new Token;
    token->key = kHiragana;
    token->value = kHiragana;
    tokens.push_back(token);
  }

  {  // Katakana
    Token *token = new Token;
    token->key = kKatakanaKey;
    token->value = kKatakanaValue;
    tokens.push_back(token);
  }

  BuildSystemDictionary(tokens, tokens.size());

  auto system_dic = SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic.get() != nullptr)
      << "Failed to open dictionary source:" << dic_fn_;

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

  STLDeleteElements(&tokens);
}

TEST_F(SystemDictionaryTest, NormalWord) {
  std::vector<Token *> source_tokens;
  std::unique_ptr<Token> t0(new Token);
  t0->key = "あ";
  t0->value = "亜";
  t0->cost = 100;
  t0->lid = 50;
  t0->rid = 70;
  source_tokens.push_back(t0.get());
  BuildSystemDictionary(source_tokens,
                        mozc::GetFlag(FLAGS_dictionary_test_size));

  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source:" << dic_fn_;

  CollectTokenCallback callback;

  // Look up by exact key.
  system_dic->LookupPrefix(t0->key, convreq_, &callback);
  ASSERT_EQ(1, callback.tokens().size());
  EXPECT_TOKEN_EQ(*t0, callback.tokens().front());

  // Look up by prefix.
  callback.Clear();
  system_dic->LookupPrefix("あいう", convreq_, &callback);
  ASSERT_EQ(1, callback.tokens().size());
  EXPECT_TOKEN_EQ(*t0, callback.tokens().front());

  // Nothing should be looked up.
  callback.Clear();
  system_dic->LookupPrefix("かきく", convreq_, &callback);
  EXPECT_TRUE(callback.tokens().empty());
}

TEST_F(SystemDictionaryTest, SameWord) {
  std::vector<Token> tokens(4);

  tokens[0].key = "あ";
  tokens[0].value = "亜";
  tokens[0].cost = 100;
  tokens[0].lid = 50;
  tokens[0].rid = 70;

  tokens[1].key = "あ";
  tokens[1].value = "亜";
  tokens[1].cost = 150;
  tokens[1].lid = 100;
  tokens[1].rid = 200;

  tokens[2].key = "あ";
  tokens[2].value = "あ";
  tokens[2].cost = 100;
  tokens[2].lid = 1000;
  tokens[2].rid = 2000;

  tokens[3].key = "あ";
  tokens[3].value = "亜";
  tokens[3].cost = 1000;
  tokens[3].lid = 2000;
  tokens[3].rid = 3000;

  std::vector<Token *> source_tokens;
  for (size_t i = 0; i < tokens.size(); ++i) {
    source_tokens.push_back(&tokens[i]);
  }
  BuildSystemDictionary(source_tokens,
                        mozc::GetFlag(FLAGS_dictionary_test_size));

  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source:" << dic_fn_;

  // All the tokens should be looked up.
  CollectTokenCallback callback;
  system_dic->LookupPrefix("あ", convreq_, &callback);
  EXPECT_TOKENS_EQ_UNORDERED(source_tokens, callback.tokens());
}

TEST_F(SystemDictionaryTest, LookupAllWords) {
  const std::vector<Token *> &source_tokens = text_dict_->tokens();
  BuildSystemDictionary(source_tokens,
                        mozc::GetFlag(FLAGS_dictionary_test_size));

  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source:" << dic_fn_;

  // All the tokens should be looked up.
  for (size_t i = 0; i < source_tokens.size(); ++i) {
    CheckTokenExistenceCallback callback(source_tokens[i]);
    system_dic->LookupPrefix(source_tokens[i]->key, convreq_, &callback);
    EXPECT_TRUE(callback.found())
        << "Token was not found: " << PrintToken(*source_tokens[i]);
  }
}

TEST_F(SystemDictionaryTest, SimpleLookupPrefix) {
  const std::string k0 = "は";
  const std::string k1 = "はひふへほ";
  std::unique_ptr<Token> t0(CreateToken(k0, "aa"));
  std::unique_ptr<Token> t1(CreateToken(k1, "bb"));

  std::vector<Token *> source_tokens;
  source_tokens.push_back(t0.get());
  source_tokens.push_back(t1.get());
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 100);

  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source:" << dic_fn_;

  // |t0| should be looked up from |k1|.
  CheckTokenExistenceCallback callback(t0.get());
  system_dic->LookupPrefix(k1, convreq_, &callback);
  EXPECT_TRUE(callback.found());
}

namespace {

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

}  // namespace

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
  const size_t kKeyValuesSize = arraysize(kKeyValues);
  std::unique_ptr<Token> tokens[kKeyValuesSize];
  std::vector<Token *> source_tokens(kKeyValuesSize);
  for (size_t i = 0; i < kKeyValuesSize; ++i) {
    tokens[i].reset(CreateToken(kKeyValues[i].key, kKeyValues[i].value));
    source_tokens[i] = tokens[i].get();
  }
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, kKeyValuesSize);
  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source:" << dic_fn_;

  // Test for normal prefix lookup without key expansion.
  {
    LookupPrefixTestCallback callback;
    system_dic->LookupPrefix("あい",  // "あい"
                             convreq_, &callback);
    const std::set<std::pair<std::string, std::string>> &result =
        callback.result();
    // "あ" -- "あい" should be found.
    for (size_t i = 0; i < 5; ++i) {
      const std::pair<std::string, std::string> entry(kKeyValues[i].key,
                                                      kKeyValues[i].value);
      EXPECT_TRUE(result.end() != result.find(entry));
    }
    // The others should not be found.
    for (size_t i = 5; i < arraysize(kKeyValues); ++i) {
      const std::pair<std::string, std::string> entry(kKeyValues[i].key,
                                                      kKeyValues[i].value);
      EXPECT_TRUE(result.end() == result.find(entry));
    }
  }

  // Test for normal prefix lookup without key expansion, but with culling
  // feature.
  {
    LookupPrefixTestCallback callback;
    system_dic->LookupPrefix("かきく", convreq_, &callback);
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
    system_dic->LookupPrefix("さしす", convreq_, &callback);
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
    system_dic->LookupPrefix("たちつ", convreq_, &callback);
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
    system_dic->LookupPrefix("はひ", convreq_, &callback);
    const std::set<std::pair<std::string, std::string>> &result =
        callback.result();
    const char *kExpectedKeys[] = {
        "は", "ば", "はひ", "ばひ", "はび", "ばび",
    };
    const std::set<std::string> expected(
        kExpectedKeys, kExpectedKeys + arraysize(kExpectedKeys));
    for (size_t i = 0; i < kKeyValuesSize; ++i) {
      const bool to_be_found =
          expected.find(kKeyValues[i].key) != expected.end();
      const std::pair<std::string, std::string> entry(kKeyValues[i].key,
                                                      kKeyValues[i].value);
      EXPECT_EQ(to_be_found, result.find(entry) != result.end());
    }
  }
}

TEST_F(SystemDictionaryTest, LookupPredictive) {
  std::vector<Token *> tokens;
  ScopedElementsDeleter<std::vector<Token *>> deleter(&tokens);

  tokens.push_back(CreateToken("まみむめもや", "value0"));
  tokens.push_back(CreateToken("まみむめもやゆよ", "value1"));
  // Build a dictionary with the above two tokens plus those from test data.
  {
    std::vector<Token *> source_tokens = tokens;
    text_dict_->CollectTokens(&source_tokens);  // Load test data.
    BuildSystemDictionary(source_tokens, 10000);
  }
  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source: " << dic_fn_;

  // All the tokens in |tokens| should be looked up by "まみむめも".
  const char kMamimumemo[] = "まみむめも";
  CheckMultiTokensExistenceCallback callback(tokens);
  system_dic->LookupPredictive(kMamimumemo, convreq_, &callback);
  EXPECT_TRUE(callback.AreAllFound());
}

TEST_F(SystemDictionaryTest, LookupPredictive_KanaModifierInsensitiveLookup) {
  std::vector<Token *> tokens;
  ScopedElementsDeleter<std::vector<Token *>> deleter(&tokens);

  tokens.push_back(CreateToken("がっこう", "学校"));
  tokens.push_back(CreateToken("かっこう", "格好"));

  BuildSystemDictionary(tokens, 100);
  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic.get() != nullptr)
      << "Failed to open dictionary source: " << dic_fn_;

  const std::string kKey = "かつこう";

  // Without Kana modifier insensitive lookup flag, nothing is looked up.
  CollectTokenCallback callback;
  request_.set_kana_modifier_insensitive_conversion(false);
  config_.set_use_kana_modifier_insensitive_conversion(false);
  system_dic->LookupPredictive(kKey, convreq_, &callback);
  EXPECT_TRUE(callback.tokens().empty());

  // With Kana modifier insensitive lookup flag, every token is looked up.
  callback.Clear();
  request_.set_kana_modifier_insensitive_conversion(true);
  config_.set_use_kana_modifier_insensitive_conversion(true);
  system_dic->LookupPredictive(kKey, convreq_, &callback);
  EXPECT_TOKENS_EQ_UNORDERED(tokens, callback.tokens());
}

TEST_F(SystemDictionaryTest, LookupPredictive_CutOffEmulatingBFS) {
  std::vector<Token *> tokens;
  ScopedElementsDeleter<std::vector<Token *>> deleter(&tokens);

  tokens.push_back(CreateToken("あい", "ai"));
  tokens.push_back(CreateToken("あいうえお", "aiueo"));
  // Build a dictionary with the above two tokens plus those from test data.
  {
    std::vector<Token *> source_tokens = tokens;
    text_dict_->CollectTokens(&source_tokens);  // Load test data.
    BuildSystemDictionary(source_tokens, 10000);
  }
  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source: " << dic_fn_;

  // Since there are many entries starting with "あ" in test dictionary, it's
  // expected that "あいうえお" is not looked up because of longer key cut-off
  // mechanism.  However, "あい" is looked up as it's short.
  CheckMultiTokensExistenceCallback callback(tokens);
  system_dic->LookupPredictive("あ", convreq_, &callback);
  EXPECT_TRUE(callback.IsFound(tokens[0]));
  EXPECT_FALSE(callback.IsFound(tokens[1]));
}

TEST_F(SystemDictionaryTest, LookupExact) {
  std::vector<Token *> source_tokens;

  const std::string k0 = "は";
  const std::string k1 = "はひふへほ";

  std::unique_ptr<Token> t0(CreateToken(k0, "aa"));
  std::unique_ptr<Token> t1(CreateToken(k1, "bb"));
  source_tokens.push_back(t0.get());
  source_tokens.push_back(t1.get());
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 100);

  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source:" << dic_fn_;

  // |t0| should not be looked up from |k1|.
  CheckTokenExistenceCallback callback0(t0.get());
  system_dic->LookupExact(k1, convreq_, &callback0);
  EXPECT_FALSE(callback0.found());
  // But |t1| should be found.
  CheckTokenExistenceCallback callback1(t1.get());
  system_dic->LookupExact(k1, convreq_, &callback1);
  EXPECT_TRUE(callback1.found());

  // Nothing should be found from "hoge".
  CollectTokenCallback callback_hoge;
  system_dic->LookupExact("hoge", convreq_, &callback_hoge);
  EXPECT_TRUE(callback_hoge.tokens().empty());
}

TEST_F(SystemDictionaryTest, LookupReverse) {
  std::unique_ptr<Token> t0(new Token);
  t0->key = "ど";
  t0->value = "ド";
  t0->cost = 1;
  t0->lid = 2;
  t0->rid = 3;
  std::unique_ptr<Token> t1(new Token);
  t1->key = "どらえもん";
  t1->value = "ドラえもん";
  t1->cost = 1;
  t1->lid = 2;
  t1->rid = 3;
  std::unique_ptr<Token> t2(new Token);
  t2->key = "といざらす®";
  t2->value = "トイザらス®";
  t2->cost = 1;
  t2->lid = 2;
  t2->rid = 3;
  std::unique_ptr<Token> t3(new Token);
  // Both t3 and t4 will be encoded into 3 bytes.
  t3->key = "ああああああ";
  t3->value = t3->key;
  t3->cost = 32000;
  t3->lid = 1;
  t3->rid = 1;
  std::unique_ptr<Token> t4(new Token);
  *t4 = *t3;
  t4->lid = 1;
  t4->rid = 2;
  std::unique_ptr<Token> t5(new Token);
  // t5 will be encoded into 3 bytes.
  t5->key = "いいいいいい";
  t5->value = t5->key;
  t5->cost = 32000;
  t5->lid = 1;
  t5->rid = 1;
  // spelling correction token should not be retrieved by reverse lookup.
  std::unique_ptr<Token> t6(new Token);
  t6->key = "どらえもん";
  t6->value = "ドラえもん";
  t6->cost = 1;
  t6->lid = 2;
  t6->rid = 3;
  t6->attributes = Token::SPELLING_CORRECTION;
  std::unique_ptr<Token> t7(new Token);
  t7->key = "こんさーと";
  t7->value = "コンサート";
  t7->cost = 1;
  t7->lid = 1;
  t7->rid = 1;
  // "バージョン" should not return a result with the key "ヴァージョン".
  std::unique_ptr<Token> t8(new Token);
  t8->key = "ばーじょん";
  t8->value = "バージョン";
  t8->cost = 1;
  t8->lid = 1;
  t8->rid = 1;

  std::vector<Token *> source_tokens;
  source_tokens.push_back(t0.get());
  source_tokens.push_back(t1.get());
  source_tokens.push_back(t2.get());
  source_tokens.push_back(t3.get());
  source_tokens.push_back(t4.get());
  source_tokens.push_back(t5.get());
  source_tokens.push_back(t6.get());
  source_tokens.push_back(t7.get());
  source_tokens.push_back(t8.get());

  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, source_tokens.size());

  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source:" << dic_fn_;
  const size_t test_size =
      std::min(static_cast<size_t>(
                   mozc::GetFlag(FLAGS_dictionary_reverse_lookup_test_size)),
               source_tokens.size());
  for (size_t source_index = 0; source_index < test_size; ++source_index) {
    const Token &source_token = *source_tokens[source_index];
    CollectTokenCallback callback;
    system_dic->LookupReverse(source_token.value, convreq_, &callback);
    const std::vector<Token> &tokens = callback.tokens();

    bool found = false;
    for (size_t i = 0; i < tokens.size(); ++i) {
      const Token &token = tokens[i];
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
    const std::string key = t7->value + "が";
    CollectTokenCallback callback;
    system_dic->LookupReverse(key, convreq_, &callback);
    const std::vector<Token> &tokens = callback.tokens();
    bool found = false;
    for (size_t i = 0; i < tokens.size(); ++i) {
      if (CompareTokensForLookup(*t7, tokens[i], true)) {
        found = true;
      }
    }
    EXPECT_TRUE(found) << "Missed token for non exact transliterated index "
                       << key;
  }
}

TEST_F(SystemDictionaryTest, LookupReverseIndex) {
  const std::vector<Token *> &source_tokens = text_dict_->tokens();
  BuildSystemDictionary(source_tokens,
                        mozc::GetFlag(FLAGS_dictionary_test_size));

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

  std::vector<Token *>::const_iterator it;
  int size = mozc::GetFlag(FLAGS_dictionary_reverse_lookup_test_size);
  for (it = source_tokens.begin(); size > 0 && it != source_tokens.end();
       ++it, --size) {
    const Token &t = **it;
    CollectTokenCallback callback1, callback2;
    system_dic_without_index->LookupReverse(t.value, convreq_, &callback1);
    system_dic_with_index->LookupReverse(t.value, convreq_, &callback2);

    const std::vector<Token> &tokens1 = callback1.tokens();
    const std::vector<Token> &tokens2 = callback2.tokens();
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
  std::vector<Token *> source_tokens;
  source_tokens.push_back(&source_token);
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, source_tokens.size());

  Token target_token = source_token;
  target_token.key.swap(target_token.value);

  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source:" << dic_fn_;
  system_dic->PopulateReverseLookupCache(kDoraemon);
  CheckTokenExistenceCallback callback(&target_token);
  system_dic->LookupReverse(kDoraemon, convreq_, &callback);
  EXPECT_TRUE(callback.found())
      << "Could not find " << PrintToken(source_token);
  system_dic->ClearReverseLookupCache();
}

TEST_F(SystemDictionaryTest, SpellingCorrectionTokens) {
  std::vector<Token> tokens(3);

  tokens[0].key = "あぼがど";
  tokens[0].value = "アボカド";
  tokens[0].cost = 1;
  tokens[0].lid = 0;
  tokens[0].rid = 2;
  tokens[0].attributes = Token::SPELLING_CORRECTION;

  tokens[1].key = "しゅみれーしょん";
  tokens[1].value = "シミュレーション";
  tokens[1].cost = 1;
  tokens[1].lid = 100;
  tokens[1].rid = 3;
  tokens[1].attributes = Token::SPELLING_CORRECTION;

  tokens[2].key = "あきはばら";
  tokens[2].value = "秋葉原";
  tokens[2].cost = 1000;
  tokens[2].lid = 1;
  tokens[2].rid = 2;

  std::vector<Token *> source_tokens;
  for (size_t i = 0; i < tokens.size(); ++i) {
    source_tokens.push_back(&tokens[i]);
  }
  BuildSystemDictionary(source_tokens, source_tokens.size());

  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source:" << dic_fn_;

  for (size_t i = 0; i < source_tokens.size(); ++i) {
    CheckTokenExistenceCallback callback(source_tokens[i]);
    system_dic->LookupPrefix(source_tokens[i]->key, convreq_, &callback);
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

  std::unique_ptr<Token> tokens[5];
  tokens[0].reset(CreateToken(k0, "aa"));
  tokens[1].reset(CreateToken(k1, "bb"));
  tokens[2].reset(CreateToken(k2, "cc"));
  tokens[3].reset(CreateToken(k3, "dd"));
  tokens[4].reset(CreateToken(k4, "ee"));

  std::vector<Token *> source_tokens;
  for (size_t i = 0; i < arraysize(tokens); ++i) {
    source_tokens.push_back(tokens[i].get());
  }
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 100);

  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source:" << dic_fn_;

  request_.set_kana_modifier_insensitive_conversion(true);
  config_.set_use_kana_modifier_insensitive_conversion(true);

  // Prefix search
  for (size_t i = 0; i < arraysize(tokens); ++i) {
    CheckTokenExistenceCallback callback(tokens[i].get());
    // "かつこう" -> "かつ", "かっこ", "かつこう", "かっこう" and "がっこう"
    system_dic->LookupPrefix(k2, convreq_, &callback);
    EXPECT_TRUE(callback.found())
        << "Token " << i << " was not found: " << PrintToken(*tokens[i]);
  }

  // Predictive searches
  {
    // "かつ" -> "かつ", "かっこ", "かつこう", "かっこう" and "がっこう"
    std::vector<Token *> expected;
    for (size_t i = 0; i < arraysize(tokens); ++i) {
      expected.push_back(tokens[i].get());
    }
    CheckMultiTokensExistenceCallback callback(expected);
    system_dic->LookupPredictive(k0, convreq_, &callback);
    EXPECT_TRUE(callback.AreAllFound());
  }
  {
    // "かっこ" -> "かっこ", "かっこう" and "がっこう"
    std::vector<Token *> expected;
    expected.push_back(tokens[1].get());
    expected.push_back(tokens[3].get());
    expected.push_back(tokens[4].get());
    CheckMultiTokensExistenceCallback callback(expected);
    system_dic->LookupPredictive(k1, convreq_, &callback);
    EXPECT_TRUE(callback.AreAllFound());
  }
}

TEST_F(SystemDictionaryTest, NoModifierForKanaEntries) {
  std::unique_ptr<Token> t0(CreateToken("ていすてぃんぐ", "テイスティング"));
  std::unique_ptr<Token> t1(CreateToken("てすとです", "てすとです"));

  std::vector<Token *> source_tokens;
  source_tokens.push_back(t0.get());
  source_tokens.push_back(t1.get());

  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 100);

  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source:" << dic_fn_;

  // Lookup |t0| from "ていすていんぐ"
  const std::string k = "ていすていんぐ";
  request_.set_kana_modifier_insensitive_conversion(true);
  config_.set_use_kana_modifier_insensitive_conversion(true);
  CheckTokenExistenceCallback callback(t0.get());
  system_dic->LookupPrefix(k, convreq_, &callback);
  EXPECT_TRUE(callback.found()) << "Not found: " << PrintToken(*t0);
}

TEST_F(SystemDictionaryTest, DoNotReturnNoModifierTargetWithLoudsTrie) {
  const std::string k0 = "かつ";
  const std::string k1 = "かっこ";
  const std::string k2 = "かつこう";
  const std::string k3 = "かっこう";
  const std::string k4 = "がっこう";

  std::unique_ptr<Token> t0(CreateToken(k0, "aa"));
  std::unique_ptr<Token> t1(CreateToken(k1, "bb"));
  std::unique_ptr<Token> t2(CreateToken(k2, "cc"));
  std::unique_ptr<Token> t3(CreateToken(k3, "dd"));
  std::unique_ptr<Token> t4(CreateToken(k4, "ee"));

  std::vector<Token *> source_tokens;
  source_tokens.push_back(t0.get());
  source_tokens.push_back(t1.get());
  source_tokens.push_back(t2.get());
  source_tokens.push_back(t3.get());
  source_tokens.push_back(t4.get());

  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 100);

  std::unique_ptr<SystemDictionary> system_dic =
      SystemDictionary::Builder(dic_fn_).Build().value();
  ASSERT_TRUE(system_dic) << "Failed to open dictionary source:" << dic_fn_;

  request_.set_kana_modifier_insensitive_conversion(false);
  config_.set_use_kana_modifier_insensitive_conversion(false);

  // Prefix search
  {
    // "かっこう" (k3) -> "かっこ" (k1) and "かっこう" (k3)
    // Make sure "がっこう" is not in the results when searched by "かっこう"
    std::vector<Token *> to_be_looked_up, not_to_be_looked_up;
    to_be_looked_up.push_back(t1.get());
    to_be_looked_up.push_back(t3.get());
    not_to_be_looked_up.push_back(t0.get());
    not_to_be_looked_up.push_back(t2.get());
    not_to_be_looked_up.push_back(t4.get());
    for (size_t i = 0; i < to_be_looked_up.size(); ++i) {
      CheckTokenExistenceCallback callback(to_be_looked_up[i]);
      system_dic->LookupPrefix(k3, convreq_, &callback);
      EXPECT_TRUE(callback.found())
          << "Token is not found: " << PrintToken(*to_be_looked_up[i]);
    }
    for (size_t i = 0; i < not_to_be_looked_up.size(); ++i) {
      CheckTokenExistenceCallback callback(not_to_be_looked_up[i]);
      system_dic->LookupPrefix(k3, convreq_, &callback);
      EXPECT_FALSE(callback.found()) << "Token should not be found: "
                                     << PrintToken(*not_to_be_looked_up[i]);
    }
  }

  // Predictive search
  {
    // "かっこ" -> "かっこ" and "かっこう"
    // Make sure "がっこう" is not in the results when searched by "かっこ"
    std::vector<Token *> to_be_looked_up, not_to_be_looked_up;
    to_be_looked_up.push_back(t1.get());
    to_be_looked_up.push_back(t3.get());
    not_to_be_looked_up.push_back(t0.get());
    not_to_be_looked_up.push_back(t2.get());
    not_to_be_looked_up.push_back(t4.get());
    for (size_t i = 0; i < to_be_looked_up.size(); ++i) {
      CheckTokenExistenceCallback callback(to_be_looked_up[i]);
      system_dic->LookupPredictive(k1, convreq_, &callback);
      EXPECT_TRUE(callback.found())
          << "Token is not found: " << PrintToken(*to_be_looked_up[i]);
    }
    for (size_t i = 0; i < not_to_be_looked_up.size(); ++i) {
      CheckTokenExistenceCallback callback(not_to_be_looked_up[i]);
      system_dic->LookupPredictive(k3, convreq_, &callback);
      EXPECT_FALSE(callback.found()) << "Token should not be found: "
                                     << PrintToken(*not_to_be_looked_up[i]);
    }
  }
}

}  // namespace dictionary
}  // namespace mozc

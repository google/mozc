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

#include "dictionary/system/system_dictionary.h"

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/stl_util.h"
#include "base/system_util.h"
#include "base/util.h"
#include "converter/node_allocator.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/dictionary_test_util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/system_dictionary_builder.h"
#include "dictionary/text_dictionary_loader.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

using mozc::dictionary::CollectTokenCallback;

namespace {
// We cannot use #ifdef in DEFINE_int32.
#ifdef DEBUG
const uint32 kDefaultReverseLookupTestSize = 1000;
#else
const uint32 kDefaultReverseLookupTestSize = 10000;
#endif
}  // namespace

// TODO(noriyukit): Ideally, the copy rule of dictionary_oss/dictionary00.txt
// can be shared with one in
// data_manager/dictionary_oss/oss_data_manager_test.gyp. However, to avoid
// conflict of copy destination name, the copy destination here is changed from
// the original one. See also comments in system_dictionary_test.gyp.
DEFINE_string(
    dictionary_source,
    "data/system_dictionary_test/dictionary00.txt",
    "source dictionary file to run test");

DEFINE_int32(dictionary_test_size, 100000,
             "Dictionary size for this test.");
DEFINE_int32(dictionary_reverse_lookup_test_size, kDefaultReverseLookupTestSize,
             "Number of tokens to run reverse lookup test.");
DECLARE_string(test_srcdir);
DECLARE_string(test_tmpdir);
DECLARE_int32(min_key_length_to_use_small_cost_encoding);

namespace mozc {
namespace dictionary {

namespace {

const bool kEnableKanaModiferInsensitiveLookup = true;
const bool kDisableKanaModiferInsensitiveLookup = false;

}  // namespace

using mozc::storage::louds::KeyExpansionTable;

class SystemDictionaryTest : public testing::Test {
 protected:
  SystemDictionaryTest()
      : text_dict_(new TextDictionaryLoader(
          *UserPosManager::GetUserPosManager()->GetPOSMatcher())),
        dic_fn_(FLAGS_test_tmpdir + "/mozc.dic") {
    const string dic_path = FileUtil::JoinPath(FLAGS_test_srcdir,
                                               FLAGS_dictionary_source);
    text_dict_->LoadWithLineLimit(dic_path, "", FLAGS_dictionary_test_size);
  }

  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);

    // Don't use small cost encoding by default.
    original_flags_min_key_length_to_use_small_cost_encoding_ =
        FLAGS_min_key_length_to_use_small_cost_encoding;
    FLAGS_min_key_length_to_use_small_cost_encoding = kint32max;
  }

  virtual void TearDown() {
    FLAGS_min_key_length_to_use_small_cost_encoding =
        original_flags_min_key_length_to_use_small_cost_encoding_;
  }

  void BuildSystemDictionary(const vector <Token *>& tokens,
                             int num_tokens);
  Token* CreateToken(const string& key, const string& value) const;
  bool CompareTokensForLookup(const Token &a, const Token &b,
                              bool reverse) const;

  scoped_ptr<TextDictionaryLoader> text_dict_;
  const string dic_fn_;
  int original_flags_min_key_length_to_use_small_cost_encoding_;
};

void SystemDictionaryTest::BuildSystemDictionary(const vector<Token *>& source,
                                                 int num_tokens) {
  SystemDictionaryBuilder builder;
  vector<Token *> tokens;
  // Picks up first tokens.
  for (vector<Token *>::const_iterator it = source.begin();
       tokens.size() < num_tokens && it != source.end(); ++it) {
    tokens.push_back(*it);
  }
  builder.BuildFromTokens(tokens);
  builder.WriteToFile(dic_fn_);
}

Token* SystemDictionaryTest::CreateToken(const string& key,
                                         const string& value) const {
  Token* t = new Token;
  t->key = key;
  t->value = value;
  t->cost = 0;
  t->lid = 0;
  t->rid = 0;
  return t;
}

// Returns true if they seem to be same
bool SystemDictionaryTest::CompareTokensForLookup(
    const Token &a, const Token &b, bool reverse) const {
  const bool key_value_check = reverse ?
      (a.key == b.value && a.value == b.key) :
      (a.key == b.key && a.value == b.value);
  if (!key_value_check) {
    return false;
  }
  const bool comp_cost = a.cost == b.cost;
  if (!comp_cost) {
    return false;
  }
  const bool spelling_match =
      (a.attributes & Token::SPELLING_CORRECTION) ==
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
  vector<Token *> tokens;
  for (int i = 0; i < 4; ++i) {
    Token *token = new Token;
    // "きー%d"
    token->key = Util::StringPrintf("\xE3\x81\x8D\xE3\x83\xBC%d", i);
    // "バリュー%d"
    token->value = Util::StringPrintf(
        "\xE3\x83\x90\xE3\x83\xAA\xE3\x83\xA5\xE3\x83\xBC%d", i);
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

  // "ｆｕｌｌ"
  const string kFull = "\xEF\xBD\x86\xEF\xBD\x95\xEF\xBD\x8C\xEF\xBD\x8C";
  // "ひらがな"
  const string kHiragana = "\xE3\x81\xB2\xE3\x82\x89\xE3\x81\x8C\xE3\x81\xAA";
  // "かたかな"
  const string kKatakanaKey =
      "\xE3\x81\x8B\xE3\x81\x9F\xE3\x81\x8B\xE3\x81\xAA";
  // "カタカナ"
  const string kKatakanaValue =
      "\xE3\x82\xAB\xE3\x82\xBF\xE3\x82\xAB\xE3\x83\x8A";

  {  // Alphabet full width
    Token *token = new Token;
    token->key = "full";
    token->value = kFull;  // "ｆｕｌｌ"
    tokens.push_back(token);
  }

  {  // Hiragana
    Token *token = new Token;
    token->key = kHiragana;  // "ひらがな"
    token->value = kHiragana;  // "ひらがな"
    tokens.push_back(token);
  }

  {  // Katakana
    Token *token = new Token;
    token->key = kKatakanaKey;  // "かたかな"
    token->value = kKatakanaValue;  // "カタカナ"
    tokens.push_back(token);
  }

  BuildSystemDictionary(tokens, tokens.size());

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  EXPECT_TRUE(system_dic->HasValue(
      // "バリュー0"
      "\xE3\x83\x90\xE3\x83\xAA\xE3\x83\xA5\xE3\x83\xBC\x30"));
  EXPECT_TRUE(system_dic->HasValue(
      // "バリュー1"
      "\xE3\x83\x90\xE3\x83\xAA\xE3\x83\xA5\xE3\x83\xBC\x31"));
  EXPECT_TRUE(system_dic->HasValue(
      // "バリュー2"
      "\xE3\x83\x90\xE3\x83\xAA\xE3\x83\xA5\xE3\x83\xBC\x32"));
  EXPECT_TRUE(system_dic->HasValue(
      // "バリュー3"
      "\xE3\x83\x90\xE3\x83\xAA\xE3\x83\xA5\xE3\x83\xBC\x33"));
  EXPECT_FALSE(system_dic->HasValue(
      // "バリュー4"
      "\xE3\x83\x90\xE3\x83\xAA\xE3\x83\xA5\xE3\x83\xBC\x34"));
  EXPECT_FALSE(system_dic->HasValue(
      // "バリュー5"
      "\xE3\x83\x90\xE3\x83\xAA\xE3\x83\xA5\xE3\x83\xBC\x35"));
  EXPECT_FALSE(system_dic->HasValue(
      // "バリュー6"
      "\xE3\x83\x90\xE3\x83\xAA\xE3\x83\xA5\xE3\x83\xBC\x36"));

  EXPECT_TRUE(system_dic->HasValue("Mozc"));
  EXPECT_FALSE(system_dic->HasValue("mozc"));

  EXPECT_TRUE(system_dic->HasValue("UPPER"));
  EXPECT_FALSE(system_dic->HasValue("upper"));

  EXPECT_TRUE(system_dic->HasValue(kFull));  // "ｆｕｌｌ"
  EXPECT_FALSE(system_dic->HasValue("full"));

  EXPECT_TRUE(system_dic->HasValue(kHiragana));  //"ひらがな"
  EXPECT_FALSE(system_dic->HasValue(
      "\xE3\x83\x92\xE3\x83\xA9\xE3\x82\xAC\xE3\x83\x8A\x0A"));  // "ヒラガナ"

  EXPECT_TRUE(system_dic->HasValue(kKatakanaValue));  // "カタカナ"
  EXPECT_FALSE(system_dic->HasValue(kKatakanaKey));  // "かたかな"

  STLDeleteElements(&tokens);
}

TEST_F(SystemDictionaryTest, NormalWord) {
  vector<Token *> source_tokens;
  scoped_ptr<Token> t0(new Token);
  // "あ"
  t0->key = "\xe3\x81\x82";
  // "亜"
  t0->value = "\xe4\xba\x9c";
  t0->cost = 100;
  t0->lid = 50;
  t0->rid = 70;
  source_tokens.push_back(t0.get());
  BuildSystemDictionary(source_tokens, FLAGS_dictionary_test_size);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  CollectTokenCallback callback;

  // Look up by exact key.
  system_dic->LookupPrefix(t0->key, false, &callback);
  ASSERT_EQ(1, callback.tokens().size());
  EXPECT_TOKEN_EQ(*t0, callback.tokens().front());

  // Look up by prefix.
  callback.Clear();
  system_dic->LookupPrefix(
      "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86",  // "あいう"
      false, &callback);
  ASSERT_EQ(1, callback.tokens().size());
  EXPECT_TOKEN_EQ(*t0, callback.tokens().front());

  // Nothing should be looked up.
  callback.Clear();
  system_dic->LookupPrefix(
      "\xE3\x81\x8B\xE3\x81\x8D\xE3\x81\x8F",  // "かきく"
      false, &callback);
  EXPECT_TRUE(callback.tokens().empty());
}

TEST_F(SystemDictionaryTest, SameWord) {
  vector<Token> tokens(4);

  tokens[0].key = "\xe3\x81\x82";  // "あ"
  tokens[0].value = "\xe4\xba\x9c";  // "亜"
  tokens[0].cost = 100;
  tokens[0].lid = 50;
  tokens[0].rid = 70;

  tokens[1].key = "\xe3\x81\x82";  // "あ"
  tokens[1].value = "\xe4\xba\x9c";  // "亜"
  tokens[1].cost = 150;
  tokens[1].lid = 100;
  tokens[1].rid = 200;

  tokens[2].key = "\xe3\x81\x82";  // "あ"
  tokens[2].value = "\xe3\x81\x82";  // "あ"
  tokens[2].cost = 100;
  tokens[2].lid = 1000;
  tokens[2].rid = 2000;

  tokens[3].key = "\xe3\x81\x82";  // "あ"
  tokens[3].value = "\xe4\xba\x9c";  // "亜"
  tokens[3].cost = 1000;
  tokens[3].lid = 2000;
  tokens[3].rid = 3000;

  vector<Token *> source_tokens;
  for (size_t i = 0; i < tokens.size(); ++i) {
    source_tokens.push_back(&tokens[i]);
  }
  BuildSystemDictionary(source_tokens, FLAGS_dictionary_test_size);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  // All the tokens should be looked up.
  CollectTokenCallback callback;
  system_dic->LookupPrefix("\xe3\x81\x82",  // "あ"
                           false, &callback);
  EXPECT_TOKENS_EQ_UNORDERED(source_tokens, callback.tokens());
}

TEST_F(SystemDictionaryTest, LookupAllWords) {
  const vector<Token *> &source_tokens = text_dict_->tokens();
  BuildSystemDictionary(source_tokens, FLAGS_dictionary_test_size);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  // All the tokens should be looked up.
  for (size_t i = 0; i < source_tokens.size(); ++i) {
    CheckTokenExistenceCallback callback(source_tokens[i]);
    system_dic->LookupPrefix(source_tokens[i]->key, false, &callback);
    EXPECT_TRUE(callback.found())
        << "Token was not found: " << PrintToken(*source_tokens[i]);
  }
}

TEST_F(SystemDictionaryTest, SimpleLookupPrefix) {
  // "は"
  const string k0 = "\xe3\x81\xaf";
  // "はひふへほ"
  const string k1 = "\xe3\x81\xaf\xe3\x81\xb2\xe3\x81\xb5\xe3\x81\xb8\xe3\x81"
                    "\xbb";
  scoped_ptr<Token> t0(CreateToken(k0, "aa"));
  scoped_ptr<Token> t1(CreateToken(k1, "bb"));

  vector<Token *> source_tokens;
  source_tokens.push_back(t0.get());
  source_tokens.push_back(t1.get());
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 100);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  // |t0| should be looked up from |k1|.
  CheckTokenExistenceCallback callback(t0.get());
  system_dic->LookupPrefix(k1, false, &callback);
  EXPECT_TRUE(callback.found());
}

namespace {

class LookupPrefixTestCallback : public SystemDictionary::Callback {
 public:
  virtual ResultType OnKey(StringPiece key) {
    if (key == "\xE3\x81\x8B\xE3\x81\x8D") {  // key == "かき"
      return TRAVERSE_CULL;
    } else if (key == "\xE3\x81\x95") {  // key == "さ"
      return TRAVERSE_NEXT_KEY;
    } else if (key == "\xE3\x81\x9F") {  // key == "た"
      return TRAVERSE_DONE;
    }
    return TRAVERSE_CONTINUE;
  }

  virtual ResultType OnToken(StringPiece key, StringPiece actual_key,
                             const Token &token) {
    result_.insert(make_pair(token.key, token.value));
    return TRAVERSE_CONTINUE;
  }

  const set<pair<string, string> > &result() const {
    return result_;
  }

 private:
  set<pair<string, string> > result_;
};

}  // namespace

TEST_F(SystemDictionaryTest, LookupPrefix) {
  // Set up a test dictionary.
  struct {
    const char *key;
    const char *value;
  } kKeyValues[] = {
    // "あ", "亜"
    { "\xE3\x81\x82", "\xE4\xBA\x9C" },
    // "あ", "安"
    { "\xE3\x81\x82", "\xE5\xAE\x89" },
    // "あ", "在"
    { "\xE3\x81\x82", "\xE5\x9C\xA8" },
    // "あい", "愛"
    { "\xE3\x81\x82\xE3\x81\x84", "\xE6\x84\x9B" },
    // "あい", "藍"
    { "\xE3\x81\x82\xE3\x81\x84", "\xE8\x97\x8D" },
    // "あいう", "藍雨"
    { "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86", "\xE8\x97\x8D\xE9\x9B\xA8" },
    // "か", "可"
    { "\xE3\x81\x8B", "\xE5\x8F\xAF" },
    // "かき", "牡蠣"
    { "\xE3\x81\x8B\xE3\x81\x8D", "\xE7\x89\xA1\xE8\xA0\xA3" },
    // "かき", "夏季"
    { "\xE3\x81\x8B\xE3\x81\x8D", "\xE5\xA4\x8F\xE5\xAD\xA3" },
    // "かきく", "柿久"
    { "\xE3\x81\x8B\xE3\x81\x8D\xE3\x81\x8F", "\xE6\x9F\xBF\xE4\xB9\x85" },
    // "さ", "差"
    { "\xE3\x81\x95", "\xE5\xB7\xAE" },
    // "さ", "左"
    { "\xE3\x81\x95", "\xE5\xB7\xA6" },
    // "さし", "刺"
    { "\xE3\x81\x95\xE3\x81\x97", "\xE5\x88\xBA" },
    // "た", "田"
    { "\xE3\x81\x9F", "\xE7\x94\xB0" },
    // "た", "多"
    { "\xE3\x81\x9F", "\xE5\xA4\x9A" },
    // "たち", 多値"
    { "\xE3\x81\x9F\xE3\x81\xA1", "\xE5\xA4\x9A\xE5\x80\xA4" },
    // "たちつ", "タチツ"
    { "\xE3\x81\x9F\xE3\x81\xA1\xE3\x81\xA4",
      "\xE3\x82\xBF\xE3\x83\x81\xE3\x83\x84" },
    // "は", "葉"
    { "\xE3\x81\xAF", "\xE8\x91\x89" },
    // "は", "歯"
    { "\xE3\x81\xAF", "\xE6\xAD\xAF" },
    // "はひ", "ハヒ"
    { "\xE3\x81\xAF\xE3\x81\xB2", "\xE3\x83\x8F\xE3\x83\x92" },
    // "ば", "場"
    { "\xE3\x81\xB0", "\xE5\xA0\xB4" },
    // "はび", "波美"
    { "\xE3\x81\xAF\xE3\x81\xB3", "\xE6\xB3\xA2\xE7\xBE\x8E" },
    // "ばび", "馬尾"
    { "\xE3\x81\xB0\xE3\x81\xB3", "\xE9\xA6\xAC\xE5\xB0\xBE" },
    // "ばびぶ", "バビブ"
    { "\xE3\x81\xB0\xE3\x81\xB3\xE3\x81\xB6",
      "\xE3\x83\x90\xE3\x83\x93\xE3\x83\x96" },
  };
  const size_t kKeyValuesSize = arraysize(kKeyValues);
  scoped_ptr<Token> tokens[kKeyValuesSize];
  vector<Token *> source_tokens(kKeyValuesSize);
  for (size_t i = 0; i < kKeyValuesSize; ++i) {
    tokens[i].reset(CreateToken(kKeyValues[i].key, kKeyValues[i].value));
    source_tokens[i] = tokens[i].get();
  }
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, kKeyValuesSize);
  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  // Test for normal prefix lookup without key expansion.
  {
    LookupPrefixTestCallback callback;
    system_dic->LookupPrefix("\xE3\x81\x82\xE3\x81\x84",  // "あい"
                             false, &callback);
    const set<pair<string, string> > &result = callback.result();
    // "あ" -- "あい" should be found.
    for (size_t i = 0; i < 5; ++i) {
      const pair<string, string> entry(
          kKeyValues[i].key, kKeyValues[i].value);
      EXPECT_TRUE(result.end() != result.find(entry));
    }
    // The others should not be found.
    for (size_t i = 5; i < arraysize(kKeyValues); ++i) {
      const pair<string, string> entry(
          kKeyValues[i].key, kKeyValues[i].value);
      EXPECT_TRUE(result.end() == result.find(entry));
    }
  }

  // Test for normal prefix lookup without key expansion, but with culling
  // feature.
  {
    LookupPrefixTestCallback callback;
    system_dic->LookupPrefix(
        "\xE3\x81\x8B\xE3\x81\x8D\xE3\x81\x8F",  //"かきく"
        false,
        &callback);
    const set<pair<string, string> > &result = callback.result();
    // Only "か" should be found as the callback doesn't traverse the subtree of
    // "かき" due to culling request from LookupPrefixTestCallback::OnKey().
    for (size_t i = 0; i < kKeyValuesSize; ++i) {
      const pair<string, string> entry(
          kKeyValues[i].key, kKeyValues[i].value);
      EXPECT_EQ(entry.first == "\xE3\x81\x8B",  // "か"
                result.find(entry) != result.end());
    }
  }

  // Test for TRAVERSE_NEXT_KEY.
  {
    LookupPrefixTestCallback callback;
    system_dic->LookupPrefix(
        "\xE3\x81\x95\xE3\x81\x97\xE3\x81\x99",  // "さしす"
        false,
        &callback);
    const set<pair<string, string> > &result = callback.result();
    // Only "さし" should be found as tokens for "さ" is skipped (see
    // LookupPrefixTestCallback::OnKey()).
    for (size_t i = 0; i < kKeyValuesSize; ++i) {
      const pair<string, string> entry(
          kKeyValues[i].key, kKeyValues[i].value);
      EXPECT_EQ(entry.first == "\xE3\x81\x95\xE3\x81\x97",  // "さし"
                result.find(entry) != result.end());
    }
  }

  // Test for TRAVERSE_DONE.
  {
    LookupPrefixTestCallback callback;
    system_dic->LookupPrefix(
        "\xE3\x81\x9F\xE3\x81\xA1\xE3\x81\xA4",  // "たちつ"
        false,
        &callback);
    const set<pair<string, string> > &result = callback.result();
    // Nothing should be found as the traversal is immediately done after seeing
    // "た"; see LookupPrefixTestCallback::OnKey().
    EXPECT_TRUE(result.empty());
  }

  // Test for prefix lookup with key expansion.
  {
    LookupPrefixTestCallback callback;
    system_dic->LookupPrefix(
        "\xE3\x81\xAF\xE3\x81\xB2",  // "はひ"
        true,  // Use kana modifier insensitive lookup
        &callback);
    const set<pair<string, string> > &result = callback.result();
    const char *kExpectedKeys[] = {
      "\xE3\x81\xAF",  // "は"
      "\xE3\x81\xB0",  // "ば"
      "\xE3\x81\xAF\xE3\x81\xB2",  // "はひ"
      "\xE3\x81\xB0\xE3\x81\xB2",  // "ばひ"
      "\xE3\x81\xAF\xE3\x81\xB3",  // "はび"
      "\xE3\x81\xB0\xE3\x81\xB3",  // "ばび"
    };
    const set<string> expected(kExpectedKeys,
                               kExpectedKeys + arraysize(kExpectedKeys));
    for (size_t i = 0; i < kKeyValuesSize; ++i) {
      const bool to_be_found =
          expected.find(kKeyValues[i].key) != expected.end();
      const pair<string, string> entry(
          kKeyValues[i].key, kKeyValues[i].value);
      EXPECT_EQ(to_be_found, result.find(entry) != result.end());
    }
  }
}

TEST_F(SystemDictionaryTest, LookupPredictive) {
  vector<Token *> tokens;
  ScopedElementsDeleter<vector<Token *> > deleter(&tokens);

  // "まみむめもや" -> "value0"
  tokens.push_back(CreateToken("\xe3\x81\xbe\xe3\x81\xbf\xe3\x82\x80"
                               "\xe3\x82\x81\xe3\x82\x82\xe3\x82\x84",
                               "value0"));
  // "まみむめもやゆよ" -> "value1"
  tokens.push_back(CreateToken("\xe3\x81\xbe\xe3\x81\xbf\xe3\x82\x80"
                               "\xe3\x82\x81\xe3\x82\x82\xe3\x82\x84"
                               "\xe3\x82\x86\xe3\x82\x88",
                               "value1"));
  // Build a dictionary with the above two tokens plus those from test data.
  {
    vector<Token *> source_tokens = tokens;
    text_dict_->CollectTokens(&source_tokens);  // Load test data.
    BuildSystemDictionary(source_tokens, 10000);
  }
  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source: " << dic_fn_;

  // All the tokens in |tokens| should be looked up by "まみむめも".
  const char *kMamimumemo =
      "\xe3\x81\xbe\xe3\x81\xbf\xe3\x82\x80\xe3\x82\x81\xe3\x82\x82";
  CheckMultiTokensExistenceCallback callback(tokens);
  system_dic->LookupPredictive(kMamimumemo, false, &callback);
  EXPECT_TRUE(callback.AreAllFound());
}

TEST_F(SystemDictionaryTest, LookupPredictive_KanaModifierInsensitiveLookup) {
  vector<Token *> tokens;
  ScopedElementsDeleter<vector<Token *> > deleter(&tokens);

  // "がっこう" -> "学校"
  tokens.push_back(CreateToken(
      "\xE3\x81\x8C\xE3\x81\xA3\xE3\x81\x93\xE3\x81\x86",
      "\xE5\xAD\xA6\xE6\xA0\xA1"));
  // "かっこう" -> "格好"
  tokens.push_back(CreateToken(
      "\xE3\x81\x8B\xE3\x81\xA3\xE3\x81\x93\xE3\x81\x86",
      "\xE6\xA0\xBC\xE5\xA5\xBD"));

  BuildSystemDictionary(tokens, 100);
  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source: " << dic_fn_;

  // "かつこう"
  const string kKey = "\xE3\x81\x8B\xE3\x81\xA4\xE3\x81\x93\xE3\x81\x86";

  // Without Kana modifier insensitive lookup flag, nothing is looked up.
  CollectTokenCallback callback;
  system_dic->LookupPredictive(kKey, false, &callback);
  EXPECT_TRUE(callback.tokens().empty());

  // With Kana modifier insensitive lookup flag, every token is looked up.
  callback.Clear();
  system_dic->LookupPredictive(kKey, true, &callback);
  EXPECT_TOKENS_EQ_UNORDERED(tokens, callback.tokens());
}

TEST_F(SystemDictionaryTest, LookupPredictive_CutOffEmulatingBFS) {
  vector<Token *> tokens;
  ScopedElementsDeleter<vector<Token *> > deleter(&tokens);

  // "あい" -> "ai"
  tokens.push_back(CreateToken("\xe3\x81\x82\xe3\x81\x84", "ai"));
  // "あいうえお" -> "aiueo"
  tokens.push_back(CreateToken(
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a",
      "aiueo"));
  // Build a dictionary with the above two tokens plus those from test data.
  {
    vector<Token *> source_tokens = tokens;
    text_dict_->CollectTokens(&source_tokens);  // Load test data.
    BuildSystemDictionary(source_tokens, 10000);
  }
  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source: " << dic_fn_;

  // Since there are many entries starting with "あ" in test dictionary, it's
  // expected that "あいうえお" is not looked up because of longer key cut-off
  // mechanism.  However, "あい" is looked up as it's short.
  CheckMultiTokensExistenceCallback callback(tokens);
  system_dic->LookupPredictive("\xe3\x81\x82",  // "あ"
                               false, &callback);
  EXPECT_TRUE(callback.IsFound(tokens[0]));
  EXPECT_FALSE(callback.IsFound(tokens[1]));
}

TEST_F(SystemDictionaryTest, LookupExact) {
  vector<Token *> source_tokens;

  // "は"
  const string k0 = "\xe3\x81\xaf";
  // "はひふへほ"
  const string k1 = "\xe3\x81\xaf\xe3\x81\xb2\xe3\x81\xb5\xe3\x81\xb8\xe3\x81"
                    "\xbb";

  scoped_ptr<Token> t0(CreateToken(k0, "aa"));
  scoped_ptr<Token> t1(CreateToken(k1, "bb"));
  source_tokens.push_back(t0.get());
  source_tokens.push_back(t1.get());
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 100);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  // |t0| should not be looked up from |k1|.
  CheckTokenExistenceCallback callback0(t0.get());
  system_dic->LookupExact(k1, &callback0);
  EXPECT_FALSE(callback0.found());
  // But |t1| should be found.
  CheckTokenExistenceCallback callback1(t1.get());
  system_dic->LookupExact(k1, &callback1);
  EXPECT_TRUE(callback1.found());

  // Nothing should be found from "hoge".
  CollectTokenCallback callback_hoge;
  system_dic->LookupExact("hoge", &callback_hoge);
  EXPECT_TRUE(callback_hoge.tokens().empty());
}

TEST_F(SystemDictionaryTest, LookupReverse) {
  scoped_ptr<Token> t0(new Token);
  // "ど"
  t0->key = "\xe3\x81\xa9";
  // "ド"
  t0->value = "\xe3\x83\x89";
  t0->cost = 1;
  t0->lid = 2;
  t0->rid = 3;
  scoped_ptr<Token> t1(new Token);
  // "どらえもん"
  t1->key = "\xe3\x81\xa9\xe3\x82\x89\xe3\x81\x88\xe3\x82\x82\xe3\x82\x93";
  // "ドラえもん"
  t1->value = "\xe3\x83\x89\xe3\x83\xa9\xe3\x81\x88\xe3\x82\x82\xe3\x82\x93";
  t1->cost = 1;
  t1->lid = 2;
  t1->rid = 3;
  scoped_ptr<Token> t2(new Token);
  // "といざらす®"
  t2->key = "\xe3\x81\xa8\xe3\x81\x84\xe3\x81\x96\xe3\x82\x89\xe3\x81\x99\xc2"
            "\xae";
  // "トイザらス®"
  t2->value = "\xe3\x83\x88\xe3\x82\xa4\xe3\x82\xb6\xe3\x82\x89\xe3\x82\xb9\xc2"
              "\xae";
  t2->cost = 1;
  t2->lid = 2;
  t2->rid = 3;
  scoped_ptr<Token> t3(new Token);
  // "ああああああ"
  // Both t3 and t4 will be encoded into 3 bytes.
  t3->key = "\xe3\x81\x82\xe3\x81\x82\xe3\x81\x82"
      "\xe3\x81\x82\xe3\x81\x82\xe3\x81\x82";
  t3->value = t3->key;
  t3->cost = 32000;
  t3->lid = 1;
  t3->rid = 1;
  scoped_ptr<Token> t4(new Token);
  *t4 = *t3;
  t4->lid = 1;
  t4->rid = 2;
  scoped_ptr<Token> t5(new Token);
  // "いいいいいい"
  // t5 will be encoded into 3 bytes.
  t5->key = "\xe3\x81\x84\xe3\x81\x84\xe3\x81\x84"
      "\xe3\x81\x84\xe3\x81\x84\xe3\x81\x84";
  t5->value = t5->key;
  t5->cost = 32000;
  t5->lid = 1;
  t5->rid = 1;
  // spelling correction token should not be retrieved by reverse lookup.
  scoped_ptr<Token> t6(new Token);
  // "どらえもん"
  t6->key = "\xe3\x81\xa9\xe3\x82\x89\xe3\x81\x88\xe3\x82\x82\xe3\x82\x93";
  // "ドラえもん"
  t6->value = "\xe3\x83\x89\xe3\x83\xa9\xe3\x81\x88\xe3\x82\x82\xe3\x82\x93";
  t6->cost = 1;
  t6->lid = 2;
  t6->rid = 3;
  t6->attributes = Token::SPELLING_CORRECTION;
  scoped_ptr<Token> t7(new Token);
  // "こんさーと"
  t7->key = "\xe3\x81\x93\xe3\x82\x93\xe3\x81\x95\xe3\x83\xbc\xe3\x81\xa8";
  // "コンサート"
  t7->value = "\xe3\x82\xb3\xe3\x83\xb3\xe3\x82\xb5\xe3\x83\xbc\xe3\x83\x88";
  t7->cost = 1;
  t7->lid = 1;
  t7->rid = 1;
  // "バージョン" should not return a result with the key "ヴァージョン".
  scoped_ptr<Token> t8(new Token);
  // "ばーじょん"
  t8->key = "\xE3\x81\xB0\xE3\x83\xBC\xE3\x81\x98\xE3\x82\x87\xE3\x82\x93";
  // "バージョン"
  t8->value = "\xE3\x83\x90\xE3\x83\xBC\xE3\x82\xB8\xE3\x83\xA7\xE3\x83\xB3";
  t8->cost = 1;
  t8->lid = 1;
  t8->rid = 1;

  vector<Token *> source_tokens;
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

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;
  const size_t test_size = min(
      static_cast<size_t>(FLAGS_dictionary_reverse_lookup_test_size),
      source_tokens.size());
  for (size_t source_index = 0; source_index < test_size; ++source_index) {
    const Token &source_token = *source_tokens[source_index];
    CollectTokenCallback callback;
    system_dic->LookupReverse(source_token.value, NULL, &callback);
    const vector<Token> &tokens = callback.tokens();

    bool found = false;
    for (size_t i = 0; i < tokens.size(); ++i) {
      const Token &token = tokens[i];
      // Make sure any of the key lengths of the lookup results
      // doesn't exceed the original key length.
      // It happened once
      // when called with "バージョン", returning "ヴァージョン".
      EXPECT_LE(token.key.size(), source_token.value.size())
          << string(token.key) << ":" << string(token.value)
          << "\t" << string(source_token.value);
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
      EXPECT_TRUE(found)
          << "Failed to find " << source_token.key << ":" << source_token.value;
      if (!found) {
        return;
      }
    }
  }

  {
    // test for non exact transliterated index string.
    // append "が"
    const string key = t7->value + "\xe3\x81\x8c";
    CollectTokenCallback callback;
    system_dic->LookupReverse(key, NULL, &callback);
    const vector<Token> &tokens = callback.tokens();
    bool found = false;
    for (size_t i = 0; i < tokens.size(); ++i) {
      if (CompareTokensForLookup(*t7, tokens[i], true)) {
        found = true;
      }
    }
    EXPECT_TRUE(found)
        << "Missed token for non exact transliterated index " << key;
  }
}

TEST_F(SystemDictionaryTest, LookupReverseIndex) {
  const vector<Token *> &source_tokens = text_dict_->tokens();
  BuildSystemDictionary(source_tokens, FLAGS_dictionary_test_size);

  scoped_ptr<SystemDictionary> system_dic_without_index(
      SystemDictionary::CreateSystemDictionaryFromFileWithOptions(
          dic_fn_, SystemDictionary::NONE));
  ASSERT_TRUE(system_dic_without_index.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;
  scoped_ptr<SystemDictionary> system_dic_with_index(
      SystemDictionary::CreateSystemDictionaryFromFileWithOptions(
          dic_fn_, SystemDictionary::ENABLE_REVERSE_LOOKUP_INDEX));
  ASSERT_TRUE(system_dic_with_index.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  vector<Token *>::const_iterator it;
  int size = FLAGS_dictionary_reverse_lookup_test_size;
  for (it = source_tokens.begin();
       size > 0 && it != source_tokens.end(); ++it, --size) {
    const Token &t = **it;
    CollectTokenCallback callback1, callback2;
    system_dic_without_index->LookupReverse(t.value, NULL, &callback1);
    system_dic_with_index->LookupReverse(t.value, NULL, &callback2);

    const vector<Token> &tokens1 = callback1.tokens();
    const vector<Token> &tokens2 = callback2.tokens();
    ASSERT_EQ(tokens1.size(), tokens2.size());
    for (size_t i = 0; i < tokens1.size(); ++i) {
      EXPECT_TOKEN_EQ(tokens1[i], tokens2[i]);
    }
  }
}

TEST_F(SystemDictionaryTest, LookupReverseWithCache) {
  const string kDoraemon =
      "\xe3\x83\x89\xe3\x83\xa9\xe3\x81\x88\xe3\x82\x82\xe3\x82\x93";

  Token source_token;
  // "どらえもん"
  source_token.key =
      "\xe3\x81\xa9\xe3\x82\x89\xe3\x81\x88\xe3\x82\x82\xe3\x82\x93";
  // "ドラえもん"
  source_token.value = kDoraemon;
  source_token.cost = 1;
  source_token.lid = 2;
  source_token.rid = 3;
  vector<Token *> source_tokens;
  source_tokens.push_back(&source_token);
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, source_tokens.size());

  Token target_token = source_token;
  target_token.key.swap(target_token.value);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;
  NodeAllocator allocator;
  system_dic->PopulateReverseLookupCache(kDoraemon, &allocator);
  CheckTokenExistenceCallback callback(&target_token);
  system_dic->LookupReverse(kDoraemon, &allocator, &callback);
  EXPECT_TRUE(callback.found())
      << "Could not find " << PrintToken(source_token);
  system_dic->ClearReverseLookupCache(&allocator);
}

TEST_F(SystemDictionaryTest, SpellingCorrectionTokens) {
  vector<Token> tokens(3);

  // "あぼがど"
  tokens[0].key = "\xe3\x81\x82\xe3\x81\xbc\xe3\x81\x8c\xe3\x81\xa9";
  // "アボカド"
  tokens[0].value = "\xe3\x82\xa2\xe3\x83\x9c\xe3\x82\xab\xe3\x83\x89";
  tokens[0].cost = 1;
  tokens[0].lid = 0;
  tokens[0].rid = 2;
  tokens[0].attributes = Token::SPELLING_CORRECTION;

  // "しゅみれーしょん"
  tokens[1].key =
      "\xe3\x81\x97\xe3\x82\x85\xe3\x81\xbf\xe3\x82\x8c"
      "\xe3\x83\xbc\xe3\x81\x97\xe3\x82\x87\xe3\x82\x93";
  // "シミュレーション"
  tokens[1].value =
      "\xe3\x82\xb7\xe3\x83\x9f\xe3\x83\xa5\xe3\x83\xac"
      "\xe3\x83\xbc\xe3\x82\xb7\xe3\x83\xa7\xe3\x83\xb3";
  tokens[1].cost = 1;
  tokens[1].lid = 100;
  tokens[1].rid = 3;
  tokens[1].attributes = Token::SPELLING_CORRECTION;

  // "あきはばら"
  tokens[2].key =
      "\xe3\x81\x82\xe3\x81\x8d\xe3\x81\xaf\xe3\x81\xb0\xe3\x82\x89";
  // "秋葉原"
  tokens[2].value = "\xe7\xa7\x8b\xe8\x91\x89\xe5\x8e\x9f";
  tokens[2].cost = 1000;
  tokens[2].lid = 1;
  tokens[2].rid = 2;

  vector<Token *> source_tokens;
  for (size_t i = 0; i < tokens.size(); ++i) {
    source_tokens.push_back(&tokens[i]);
  }
  BuildSystemDictionary(source_tokens, source_tokens.size());

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  for (size_t i = 0; i < source_tokens.size(); ++i) {
    CheckTokenExistenceCallback callback(source_tokens[i]);
    system_dic->LookupPrefix(source_tokens[i]->key, false, &callback);
    EXPECT_TRUE(callback.found())
        << "Token " << i << " was not found: " << PrintToken(*source_tokens[i]);
  }
}

// Minimal modification of the codec for the TokenAfterSpellningToken.
class CodecForTest : public SystemDictionaryCodecInterface {
 public:
  CodecForTest() : counter_(0) {
  }

  virtual ~CodecForTest() {
  }

  // Just mock methods.
  const string GetSectionNameForKey() const { return "Mock"; }
  const string GetSectionNameForValue() const { return "Mock"; }
  const string GetSectionNameForTokens() const { return "Mock"; }
  const string GetSectionNameForPos() const { return "Mock"; }
  void EncodeKey(const StringPiece src, string *dst) const {}
  void DecodeKey(const StringPiece src, string *dst) const {}
  size_t GetEncodedKeyLength(const StringPiece src) const { return 0; }
  size_t GetDecodedKeyLength(const StringPiece src) const { return 0; }
  void EncodeValue(const StringPiece src, string *dst) const {}
  void EncodeTokens(
      const vector<TokenInfo> &tokens, string *output) const {}
  void DecodeTokens(
      const uint8 *ptr, vector<TokenInfo> *tokens) const {}
  bool ReadTokenForReverseLookup(
      const uint8 *ptr, int *value_id, int *read_bytes) const { return false; }
  uint8 GetTokensTerminationFlag() const { return 0xff; }

  // Mock methods which will be actually called.
  bool DecodeToken(
      const uint8 *ptr, TokenInfo *token_info, int *read_bytes) const {
    *read_bytes = 0;
    switch (counter_++) {
      case 0:
        token_info->id_in_value_trie = 0;
        token_info->value_type = TokenInfo::DEFAULT_VALUE;
        token_info->token->attributes = Token::SPELLING_CORRECTION;
        token_info->token->cost = 1;
        return true;
      case 1:
        token_info->value_type = TokenInfo::SAME_AS_PREV_VALUE;
        token_info->token->cost = 111;
        return false;
      default:
        LOG(FATAL) << "Should never reach here.";
    }
    return true;
  }

  void DecodeValue(const StringPiece src, string *dst) const {
    *dst = "DummyValue";
  }

 private:
  mutable int counter_;
  DISALLOW_COPY_AND_ASSIGN(CodecForTest);
};

TEST_F(SystemDictionaryTest, TokenAfterSpellningToken) {
  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  // Filter for reverse look up.
  SystemDictionary::FilterInfo filter;
  filter.conditions = SystemDictionary::FilterInfo::NO_SPELLING_CORRECTION;

  // The 2nd token refers previous token by SAME_AS_PREV_VALUE,
  // but the 1st token is spelling correction which will be ignored for
  // reverse conversion.

  // Inject a minimal codec.
  scoped_ptr<CodecForTest> codec(new CodecForTest());
  system_dic->codec_ = codec.get();

  CollectTokenCallback callback;
  system_dic->RegisterTokens(filter, "", "", NULL, &callback);

  const vector<Token> &tokens = callback.tokens();
  EXPECT_EQ(1, tokens.size());
  EXPECT_EQ(111, tokens[0].cost);
  EXPECT_EQ("DummyValue", tokens[0].value);
}

TEST_F(SystemDictionaryTest, EnableNoModifierTargetWithLoudsTrie) {
  // "かつ"
  const string k0 = "\xE3\x81\x8B\xE3\x81\xA4";
  // "かっこ"
  const string k1 = "\xE3\x81\x8B\xE3\x81\xA3\xE3\x81\x93";
  // "かつこう"
  const string k2 = "\xE3\x81\x8B\xE3\x81\xA4\xE3\x81\x93\xE3\x81\x86";
  // "かっこう"
  const string k3 = "\xE3\x81\x8B\xE3\x81\xA3\xE3\x81\x93\xE3\x81\x86";
  // "がっこう"
  const string k4 = "\xE3\x81\x8C\xE3\x81\xA3\xE3\x81\x93\xE3\x81\x86";

  scoped_ptr<Token> tokens[5];
  tokens[0].reset(CreateToken(k0, "aa"));
  tokens[1].reset(CreateToken(k1, "bb"));
  tokens[2].reset(CreateToken(k2, "cc"));
  tokens[3].reset(CreateToken(k3, "dd"));
  tokens[4].reset(CreateToken(k4, "ee"));

  vector<Token *> source_tokens;
  for (size_t i = 0; i < arraysize(tokens); ++i) {
    source_tokens.push_back(tokens[i].get());
  }
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 100);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  // Prefix search
  for (size_t i = 0; i < arraysize(tokens); ++i) {
    CheckTokenExistenceCallback callback(tokens[i].get());
    // "かつこう" -> "かつ", "かっこ", "かつこう", "かっこう" and "がっこう"
    system_dic->LookupPrefix(
        k2, kEnableKanaModiferInsensitiveLookup, &callback);
    EXPECT_TRUE(callback.found())
        << "Token " << i << " was not found: " << PrintToken(*tokens[i]);
  }

  // Predictive searches
  {
    // "かつ" -> "かつ", "かっこ", "かつこう", "かっこう" and "がっこう"
    vector<Token *> expected;
    for (size_t i = 0; i < arraysize(tokens); ++i) {
      expected.push_back(tokens[i].get());
    }
    CheckMultiTokensExistenceCallback callback(expected);
    system_dic->LookupPredictive(
        k0, kEnableKanaModiferInsensitiveLookup, &callback);
    EXPECT_TRUE(callback.AreAllFound());
  }
  {
    // "かっこ" -> "かっこ", "かっこう" and "がっこう"
    vector<Token *> expected;
    expected.push_back(tokens[1].get());
    expected.push_back(tokens[3].get());
    expected.push_back(tokens[4].get());
    CheckMultiTokensExistenceCallback callback(expected);
    system_dic->LookupPredictive(
        k1, kEnableKanaModiferInsensitiveLookup, &callback);
    EXPECT_TRUE(callback.AreAllFound());
  }
}

TEST_F(SystemDictionaryTest, NoModifierForKanaEntries) {
  // "ていすてぃんぐ", "テイスティング"
  scoped_ptr<Token> t0(CreateToken(
      "\xe3\x81\xa6\xe3\x81\x84\xe3\x81\x99\xe3\x81\xa6"
      "\xe3\x81\x83\xe3\x82\x93\xe3\x81\x90",
      "\xe3\x83\x86\xe3\x82\xa4\xe3\x82\xb9\xe3\x83\x86"
      "\xe3\x82\xa3\xe3\x83\xb3\xe3\x82\xb0"));
  // "てすとです", "てすとです"
  scoped_ptr<Token> t1(CreateToken(
      "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8\xe3\x81\xa7\xe3\x81\x99",
      "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8\xe3\x81\xa7\xe3\x81\x99"));

  vector<Token *> source_tokens;
  source_tokens.push_back(t0.get());
  source_tokens.push_back(t1.get());

  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 100);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  // Lookup |t0| from "ていすていんぐ"
  const string k = "\xe3\x81\xa6\xe3\x81\x84\xe3\x81\x99\xe3\x81\xa6"
      "\xe3\x81\x84\xe3\x82\x93\xe3\x81\x90";
  CheckTokenExistenceCallback callback(t0.get());
  system_dic->LookupPrefix(k, kEnableKanaModiferInsensitiveLookup,
                           &callback);
  EXPECT_TRUE(callback.found()) << "Not found: " << PrintToken(*t0);
}

TEST_F(SystemDictionaryTest, DoNotReturnNoModifierTargetWithLoudsTrie) {
  // "かつ"
  const string k0 = "\xE3\x81\x8B\xE3\x81\xA4";
  // "かっこ"
  const string k1 = "\xE3\x81\x8B\xE3\x81\xA3\xE3\x81\x93";
  // "かつこう"
  const string k2 = "\xE3\x81\x8B\xE3\x81\xA4\xE3\x81\x93\xE3\x81\x86";
  // "かっこう"
  const string k3 = "\xE3\x81\x8B\xE3\x81\xA3\xE3\x81\x93\xE3\x81\x86";
  // "がっこう"
  const string k4 = "\xE3\x81\x8C\xE3\x81\xA3\xE3\x81\x93\xE3\x81\x86";

  scoped_ptr<Token> t0(CreateToken(k0, "aa"));
  scoped_ptr<Token> t1(CreateToken(k1, "bb"));
  scoped_ptr<Token> t2(CreateToken(k2, "cc"));
  scoped_ptr<Token> t3(CreateToken(k3, "dd"));
  scoped_ptr<Token> t4(CreateToken(k4, "ee"));

  vector<Token *> source_tokens;
  source_tokens.push_back(t0.get());
  source_tokens.push_back(t1.get());
  source_tokens.push_back(t2.get());
  source_tokens.push_back(t3.get());
  source_tokens.push_back(t4.get());

  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 100);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  ASSERT_TRUE(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  // Prefix search
  {
    // "かっこう" (k3) -> "かっこ" (k1) and "かっこう" (k3)
    // Make sure "がっこう" is not in the results when searched by "かっこう"
    vector<Token *> to_be_looked_up, not_to_be_looked_up;
    to_be_looked_up.push_back(t1.get());
    to_be_looked_up.push_back(t3.get());
    not_to_be_looked_up.push_back(t0.get());
    not_to_be_looked_up.push_back(t2.get());
    not_to_be_looked_up.push_back(t4.get());
    for (size_t i = 0; i < to_be_looked_up.size(); ++i) {
      CheckTokenExistenceCallback callback(to_be_looked_up[i]);
      system_dic->LookupPrefix(
          k3, kDisableKanaModiferInsensitiveLookup, &callback);
      EXPECT_TRUE(callback.found())
          << "Token is not found: " << PrintToken(*to_be_looked_up[i]);
    }
    for (size_t i = 0; i < not_to_be_looked_up.size(); ++i) {
      CheckTokenExistenceCallback callback(not_to_be_looked_up[i]);
      system_dic->LookupPrefix(
          k3, kDisableKanaModiferInsensitiveLookup, &callback);
      EXPECT_FALSE(callback.found())
          << "Token should not be found: "
          << PrintToken(*not_to_be_looked_up[i]);
    }
  }

  // Predictive search
  {
    // "かっこ" -> "かっこ" and "かっこう"
    // Make sure "がっこう" is not in the results when searched by "かっこ"
    vector<Token *> to_be_looked_up, not_to_be_looked_up;
    to_be_looked_up.push_back(t1.get());
    to_be_looked_up.push_back(t3.get());
    not_to_be_looked_up.push_back(t0.get());
    not_to_be_looked_up.push_back(t2.get());
    not_to_be_looked_up.push_back(t4.get());
    for (size_t i = 0; i < to_be_looked_up.size(); ++i) {
      CheckTokenExistenceCallback callback(to_be_looked_up[i]);
      system_dic->LookupPredictive(
          k1, kDisableKanaModiferInsensitiveLookup, &callback);
      EXPECT_TRUE(callback.found())
          << "Token is not found: " << PrintToken(*to_be_looked_up[i]);
    }
    for (size_t i = 0; i < not_to_be_looked_up.size(); ++i) {
      CheckTokenExistenceCallback callback(not_to_be_looked_up[i]);
      system_dic->LookupPredictive(
          k3, kDisableKanaModiferInsensitiveLookup, &callback);
      EXPECT_FALSE(callback.found())
          << "Token should not be found: "
          << PrintToken(*not_to_be_looked_up[i]);
    }
  }
}

}  // namespace dictionary
}  // namespace mozc

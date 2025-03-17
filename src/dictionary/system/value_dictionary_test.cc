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

#include "dictionary/system/value_dictionary.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/dictionary_test_util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/system/codec_interface.h"
#include "request/conversion_request.h"
#include "storage/louds/louds_trie.h"
#include "storage/louds/louds_trie_builder.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace dictionary {
namespace {

using ::mozc::storage::louds::LoudsTrie;
using ::mozc::storage::louds::LoudsTrieBuilder;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::Return;

class ValueDictionaryTest : public ::testing::Test {
 protected:
  ValueDictionaryTest()
      : pos_matcher_(mock_data_manager_.GetPosMatcherData()) {}

  void SetUp() override {
    louds_trie_builder_ = std::make_unique<LoudsTrieBuilder>();
    louds_trie_ = std::make_unique<LoudsTrie>();
  }

  void TearDown() override {
    louds_trie_.reset();
    louds_trie_builder_.reset();
  }

  void AddValue(const absl::string_view value) {
    std::string encoded;
    SystemDictionaryCodecFactory::GetCodec()->EncodeValue(value, &encoded);
    louds_trie_builder_->Add(encoded);
  }

  ValueDictionary *BuildValueDictionary() {
    louds_trie_builder_->Build();
    louds_trie_->Open(
        reinterpret_cast<const uint8_t *>(louds_trie_builder_->image().data()));
    return new ValueDictionary(pos_matcher_, louds_trie_.get());
  }

  void InitToken(const absl::string_view value, Token *token) const {
    token->key = token->value = std::string(value);
    token->cost = 10000;
    token->lid = token->rid = pos_matcher_.GetSuggestOnlyWordId();
    token->attributes = Token::NONE;
  }

 private:
  const testing::MockDataManager mock_data_manager_;

 protected:
  const PosMatcher pos_matcher_;
  ConversionRequest convreq_;
  std::unique_ptr<LoudsTrieBuilder> louds_trie_builder_;
  std::unique_ptr<LoudsTrie> louds_trie_;
};

TEST_F(ValueDictionaryTest, Callback) {
  AddValue("star");
  AddValue("start");
  AddValue("starting");
  std::unique_ptr<ValueDictionary> dictionary(BuildValueDictionary());

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

    dictionary->LookupPredictive("start", convreq_, &mock_callback);
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

    dictionary->LookupExact("start", convreq_, &mock_callback);
  }
}

TEST_F(ValueDictionaryTest, HasValue) {
  AddValue("we");
  AddValue("war");
  AddValue("word");
  AddValue("world");
  std::unique_ptr<ValueDictionary> dictionary(BuildValueDictionary());

  // ValueDictionary is supposed to use the same data with SystemDictionary
  // and SystemDictionary::HasValue should return the same result with
  // ValueDictionary::HasValue.  So we can skip the actual logic of HasValue
  // and return just false.
  EXPECT_FALSE(dictionary->HasValue("we"));
  EXPECT_FALSE(dictionary->HasValue("war"));
  EXPECT_FALSE(dictionary->HasValue("word"));
  EXPECT_FALSE(dictionary->HasValue("world"));

  EXPECT_FALSE(dictionary->HasValue("hoge"));
  EXPECT_FALSE(dictionary->HasValue("piyo"));
}

TEST_F(ValueDictionaryTest, LookupPredictive) {
  AddValue("google");
  AddValue("we");
  AddValue("war");
  AddValue("word");
  AddValue("world");

  // These values are not fetched.
  AddValue("あいう");
  AddValue("東京");
  AddValue("アイウ");
  std::unique_ptr<ValueDictionary> dictionary(BuildValueDictionary());

  // Reading fields are irrelevant to value dictionary.  Prepare actual tokens
  // that are to be looked up.
  Token token_we, token_war, token_word, token_world;
  InitToken("we", &token_we);
  InitToken("war", &token_war);
  InitToken("word", &token_word);
  InitToken("world", &token_world);

  {
    CollectTokenCallback callback;
    dictionary->LookupPredictive("", convreq_, &callback);
    EXPECT_TRUE(callback.tokens().empty());
  }
  {
    CollectTokenCallback callback;
    dictionary->LookupPredictive("w", convreq_, &callback);
    std::vector<Token *> expected;
    expected.push_back(&token_we);
    expected.push_back(&token_war);
    expected.push_back(&token_word);
    expected.push_back(&token_world);
    EXPECT_TOKENS_EQ_UNORDERED(expected, callback.tokens());
  }
  {
    CollectTokenCallback callback;
    dictionary->LookupPredictive("wo", convreq_, &callback);
    std::vector<Token *> expected;
    expected.push_back(&token_word);
    expected.push_back(&token_world);
    EXPECT_TOKENS_EQ_UNORDERED(expected, callback.tokens());
  }
  {
    CollectTokenCallback callback;
    dictionary->LookupPredictive("ho", convreq_, &callback);
    EXPECT_TRUE(callback.tokens().empty());
  }
  {
    CollectTokenCallback callback;
    dictionary->LookupPredictive("あ", convreq_, &callback);
    EXPECT_TRUE(callback.tokens().empty());

    dictionary->LookupPredictive("東", convreq_, &callback);
    EXPECT_TRUE(callback.tokens().empty());

    dictionary->LookupPredictive("ア", convreq_, &callback);
    EXPECT_TRUE(callback.tokens().empty());
  }
}

TEST_F(ValueDictionaryTest, LookupExact) {
  AddValue("we");
  AddValue("war");
  AddValue("word");
  std::unique_ptr<ValueDictionary> dictionary(BuildValueDictionary());

  CollectTokenCallback callback;
  dictionary->LookupExact("war", convreq_, &callback);
  ASSERT_EQ(callback.tokens().size(), 1);
  EXPECT_EQ(callback.tokens()[0].value, "war");
}

}  // namespace
}  // namespace dictionary
}  // namespace mozc

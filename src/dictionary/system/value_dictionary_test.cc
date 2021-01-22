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

#include <memory>

#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_test_util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/system/codec_interface.h"
#include "request/conversion_request.h"
#include "storage/louds/louds_trie_builder.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

using mozc::storage::louds::LoudsTrie;
using mozc::storage::louds::LoudsTrieBuilder;

namespace mozc {
namespace dictionary {

class ValueDictionaryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    pos_matcher_.Set(mock_data_manager_.GetPOSMatcherData());
    louds_trie_builder_ = absl::make_unique<LoudsTrieBuilder>();
    louds_trie_ = absl::make_unique<LoudsTrie>();
  }

  void TearDown() override {
    louds_trie_.reset();
    louds_trie_builder_.reset();
  }

  void AddValue(const std::string &value) {
    std::string encoded;
    SystemDictionaryCodecFactory::GetCodec()->EncodeValue(value, &encoded);
    louds_trie_builder_->Add(encoded);
  }

  ValueDictionary *BuildValueDictionary() {
    louds_trie_builder_->Build();
    louds_trie_->Open(
        reinterpret_cast<const uint8 *>(louds_trie_builder_->image().data()));
    return new ValueDictionary(pos_matcher_, louds_trie_.get());
  }

  void InitToken(const std::string &value, Token *token) const {
    token->key = token->value = value;
    token->cost = 10000;
    token->lid = token->rid = pos_matcher_.GetSuggestOnlyWordId();
    token->attributes = Token::NONE;
  }

  const testing::MockDataManager mock_data_manager_;
  POSMatcher pos_matcher_;
  ConversionRequest convreq_;
  std::unique_ptr<LoudsTrieBuilder> louds_trie_builder_;
  std::unique_ptr<LoudsTrie> louds_trie_;
};

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
}

TEST_F(ValueDictionaryTest, LookupExact) {
  AddValue("we");
  AddValue("war");
  AddValue("word");
  std::unique_ptr<ValueDictionary> dictionary(BuildValueDictionary());

  CollectTokenCallback callback;
  dictionary->LookupExact("war", convreq_, &callback);
  ASSERT_EQ(1, callback.tokens().size());
  EXPECT_EQ("war", callback.tokens()[0].value);
}

}  // namespace dictionary
}  // namespace mozc

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

#include "dictionary/dictionary_mock.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "dictionary/dictionary_test_util.h"
#include "dictionary/dictionary_token.h"
#include "request/conversion_request.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

namespace mozc {
namespace dictionary {
namespace {

class DictionaryMockTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = absl::make_unique<DictionaryMock>();
  }

  DictionaryMock *GetMock() { return mock_.get(); }

  static bool SearchMatchingToken(const std::string &key,
                                  const std::string &value, uint8_t attributes,
                                  const std::vector<Token> &tokens);

  std::unique_ptr<DictionaryMock> mock_;
  ConversionRequest convreq_;
};

bool DictionaryMockTest::SearchMatchingToken(const std::string &key,
                                             const std::string &value,
                                             uint8_t attributes,
                                             const std::vector<Token> &tokens) {
  for (const Token &token : tokens) {
    if (token.key == key && token.value == value &&
        token.attributes == attributes) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<Token> CreateToken(const std::string &key,
                                   const std::string &value,
                                   Token::Attribute attr) {
  return CreateToken(key, value, DictionaryMock::kDefaultCost,
                     DictionaryMock::kDummyPosId, DictionaryMock::kDummyPosId,
                     attr);
}

std::unique_ptr<Token> CreateToken(const std::string &key,
                                   const std::string &value) {
  return CreateToken(key, value, Token::NONE);
}

TEST_F(DictionaryMockTest, HasValue) {
  DictionaryMock *dic = GetMock();

  std::unique_ptr<Token> t0 = CreateToken("k0", "v0");
  std::unique_ptr<Token> t1 = CreateToken("k1", "v1");
  std::unique_ptr<Token> t2 = CreateToken("k2", "v2");
  std::unique_ptr<Token> t3 = CreateToken("k3", "v3");

  dic->AddLookupPrefix(t0->key, t0->key, t0->value, Token::NONE);
  dic->AddLookupPredictive(t1->key, t1->key, t1->value, Token::NONE);
  dic->AddLookupReverse(t2->key, t2->key, t2->value, Token::NONE);
  dic->AddLookupExact(t3->key, t3->key, t3->value, Token::NONE);

  EXPECT_TRUE(dic->HasValue("v0"));
  EXPECT_TRUE(dic->HasValue("v1"));
  EXPECT_TRUE(dic->HasValue("v2"));
  EXPECT_TRUE(dic->HasValue("v3"));
  EXPECT_FALSE(dic->HasValue("v4"));
  EXPECT_FALSE(dic->HasValue("v5"));
  EXPECT_FALSE(dic->HasValue("v6"));
}

TEST_F(DictionaryMockTest, LookupPrefix) {
  DictionaryMock *dic = GetMock();

  std::unique_ptr<Token> t0 = CreateToken("は", "v0", Token::NONE);
  std::unique_ptr<Token> t1 = CreateToken("はひふへほ", "v1", Token::NONE);

  dic->AddLookupPrefix(t0->key, t0->key, t0->value, Token::NONE);
  dic->AddLookupPrefix(t1->key, t1->key, t1->value, Token::NONE);

  CollectTokenCallback callback;
  dic->LookupPrefix(t0->key, convreq_, &callback);
  ASSERT_EQ(1, callback.tokens().size());
  EXPECT_TOKEN_EQ(*t0, callback.tokens()[0]);

  callback.Clear();
  dic->LookupPrefix(t1->key, convreq_, &callback);
  ASSERT_EQ(2, callback.tokens().size());
  EXPECT_TOKEN_EQ(*t0, callback.tokens()[0]);
  EXPECT_TOKEN_EQ(*t1, callback.tokens()[1]);

  callback.Clear();
  dic->LookupPrefix("google", convreq_, &callback);
  EXPECT_TRUE(callback.tokens().empty());
}

TEST_F(DictionaryMockTest, LookupReverse) {
  DictionaryInterface *dic = GetMock();

  const std::string k0 = "今";
  const std::string v0 = "いま";
  const std::string k1 = "今日";
  const std::string v1 = "きょう";

  std::unique_ptr<Token> t0 = CreateToken(k0, v0);
  std::unique_ptr<Token> t1 = CreateToken(k1, v1);

  GetMock()->AddLookupReverse(t0->key, t0->key, t0->value, Token::NONE);
  GetMock()->AddLookupReverse(t1->key, t1->key, t1->value, Token::NONE);

  CollectTokenCallback callback;
  dic->LookupReverse(k1, convreq_, &callback);
  const std::vector<Token> &result_tokens = callback.tokens();
  EXPECT_TRUE(SearchMatchingToken(t0->key, t0->value, 0, result_tokens))
      << "Failed to find: " << t0->key;
  EXPECT_TRUE(SearchMatchingToken(t1->key, t1->value, 0, result_tokens))
      << "Failed to find: " << t1->key;
}

TEST_F(DictionaryMockTest, LookupPredictive) {
  DictionaryInterface *dic = GetMock();

  const std::string k0 = "は";
  const std::string k1 = "はひふ";
  const std::string k2 = "はひふへほ";

  std::unique_ptr<Token> t1 = CreateToken(k1, "v0", 100, 200, 300, Token::NONE);
  std::unique_ptr<Token> t2 = CreateToken(k2, "v1", 400, 500, 600, Token::NONE);

  GetMock()->AddLookupPredictive(k0, t1->key, t1->value, t1->cost, t1->lid,
                                 t1->rid, Token::NONE);
  GetMock()->AddLookupPredictive(k0, t2->key, t2->value, t2->cost, t2->lid,
                                 t2->rid, Token::NONE);

  CollectTokenCallback callback;
  dic->LookupPredictive(k0, convreq_, &callback);
  ASSERT_EQ(2, callback.tokens().size());
  EXPECT_TOKEN_EQ(*t1, callback.tokens()[0]);
  EXPECT_TOKEN_EQ(*t2, callback.tokens()[1]);
}

TEST_F(DictionaryMockTest, LookupExact) {
  DictionaryInterface *dic = GetMock();

  const char kKey[] = "ほげ";

  std::unique_ptr<Token> t0 = CreateToken(kKey, "value1", Token::NONE);
  std::unique_ptr<Token> t1 = CreateToken(kKey, "value2", Token::NONE);

  GetMock()->AddLookupExact(t0->key, t0->key, t0->value, Token::NONE);
  GetMock()->AddLookupExact(t1->key, t1->key, t1->value, Token::NONE);

  CollectTokenCallback callback;
  dic->LookupExact(kKey, convreq_, &callback);
  ASSERT_EQ(2, callback.tokens().size());
  EXPECT_TOKEN_EQ(*t0, callback.tokens()[0]);
  EXPECT_TOKEN_EQ(*t1, callback.tokens()[1]);

  callback.Clear();
  dic->LookupExact("hoge", convreq_, &callback);
  EXPECT_TRUE(callback.tokens().empty());

  callback.Clear();
  dic->LookupExact("ほ", convreq_, &callback);
  EXPECT_TRUE(callback.tokens().empty());
}

}  // namespace
}  // namespace dictionary
}  // namespace mozc

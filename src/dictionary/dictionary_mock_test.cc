// Copyright 2010-2018, Google Inc.
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

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "dictionary/dictionary_test_util.h"
#include "dictionary/dictionary_token.h"
#include "request/conversion_request.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace dictionary {
namespace {

using std::unique_ptr;

class DictionaryMockTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_.reset(new DictionaryMock);
  }

  DictionaryMock *GetMock() {
    return mock_.get();
  }

  static Token *CreateToken(const string &key, const string &value);
  static Token *CreateToken(const string &key, const string &value,
                            Token::Attribute attr);
  static bool SearchMatchingToken(const string &key,
                                  const string &value,
                                  uint8 attributes,
                                  const std::vector<Token> &tokens);

  unique_ptr<DictionaryMock> mock_;
  ConversionRequest convreq_;
};

bool DictionaryMockTest::SearchMatchingToken(const string &key,
                                             const string &value,
                                             uint8 attributes,
                                             const std::vector<Token> &tokens) {
  for (size_t i = 0; i < tokens.size(); ++i) {
    const Token &token = tokens[i];
    if (token.key == key && token.value == value &&
        token.attributes == attributes) {
      return true;
    }
  }
  return false;
}

Token *DictionaryMockTest::CreateToken(const string &key, const string &value) {
  Token *token = new Token;
  token->key = key;
  token->value = value;
  return token;
}

Token *DictionaryMockTest::CreateToken(const string &key, const string &value,
                                       Token::Attribute attr) {
  Token *token = new Token;
  token->key = key;
  token->value = value;
  // The same dummy cost and POS IDs set by DictionaryMock.
  token->cost = 0;
  token->lid = 1;
  token->rid = 1;
  token->attributes = attr;
  return token;
}

TEST_F(DictionaryMockTest, HasValue) {
  DictionaryMock *dic = GetMock();

  unique_ptr<Token> t0(CreateToken("k0", "v0"));
  unique_ptr<Token> t1(CreateToken("k1", "v1"));
  unique_ptr<Token> t2(CreateToken("k2", "v2"));
  unique_ptr<Token> t3(CreateToken("k3", "v3"));

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

  unique_ptr<Token> t0(CreateToken("は", "v0", Token::NONE));
  unique_ptr<Token> t1(CreateToken("はひふへほ", "v1", Token::NONE));

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

  const string k0 = "今";
  const string v0 = "いま";
  const string k1 = "今日";
  const string v1 = "きょう";

  std::vector<Token> source_tokens;
  unique_ptr<Token> t0(CreateToken(k0, v0));
  unique_ptr<Token> t1(CreateToken(k1, v1));

  source_tokens.push_back(*t0.get());
  source_tokens.push_back(*t1.get());

  for (std::vector<Token>::iterator it = source_tokens.begin();
       it != source_tokens.end(); ++it) {
    GetMock()->AddLookupReverse(it->key, it->key, it->value, Token::NONE);
  }

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

  const string k0 = "は";
  const string k1 = "はひふ";
  const string k2 = "はひふへほ";

  std::vector<Token> tokens;
  unique_ptr<Token> t1(CreateToken(k1, "v0", Token::NONE));
  unique_ptr<Token> t2(CreateToken(k2, "v1", Token::NONE));
  tokens.push_back(*t1.get());
  tokens.push_back(*t2.get());

  for (std::vector<Token>::iterator it = tokens.begin(); it != tokens.end();
       ++it) {
    GetMock()->AddLookupPredictive(k0, it->key, it->value, Token::NONE);
  }

  CollectTokenCallback callback;
  dic->LookupPredictive(k0, convreq_, &callback);
  ASSERT_EQ(2, callback.tokens().size());
  EXPECT_TOKEN_EQ(*t1, callback.tokens()[0]);
  EXPECT_TOKEN_EQ(*t2, callback.tokens()[1]);
}

TEST_F(DictionaryMockTest, LookupExact) {
  DictionaryInterface *dic = GetMock();

  const char kKey[] = "ほげ";

  unique_ptr<Token> t0(CreateToken(kKey, "value1", Token::NONE));
  unique_ptr<Token> t1(CreateToken(kKey, "value2", Token::NONE));

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

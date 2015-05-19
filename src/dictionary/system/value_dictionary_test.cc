// Copyright 2010-2012, Google Inc.
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

#include "base/base.h"
#include "base/stl_util.h"
#include "base/trie.h"
#include "base/util.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/system/system_dictionary_builder.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

class ValueDictionaryTest : public testing::Test {
 protected:
  ValueDictionaryTest() :
      dict_name_(FLAGS_test_tmpdir + "/value_dict_test.dic") {}

  virtual void SetUp() {
    STLDeleteElements(&tokens_);
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    Util::Unlink(dict_name_);
  }

  virtual void TearDown() {
    STLDeleteElements(&tokens_);
    Util::Unlink(dict_name_);
  }

  void AddToken(const string &key, const string &value) {
    Token *token = new Token;
    token->key = key;
    token->value = value;
    token->cost = 0;
    token->lid = 0;
    token->rid = 0;
    tokens_.push_back(token);
  }

  void BuildDictionary() {
    dictionary::SystemDictionaryBuilder builder;
    builder.BuildFromTokens(tokens_);
    builder.WriteToFile(dict_name_);
  }

  const string dict_name_;

 private:
  vector<Token *> tokens_;
};

TEST_F(ValueDictionaryTest, LookupPredictive) {
  NodeAllocator allocator;
  // "うぃー"
  AddToken("\xa4\xa6\xa4\xa3\xa1\xbc", "we");
  // "うぉー"
  AddToken("\xa4\xa6\xa4\xa9\xa1\xbc", "war");
  // "わーど"
  AddToken("\xa4\xef\xa1\xbc\xa4\xc9", "word");
  // "わーるど"
  AddToken("\xa4\xef\xa1\xbc\xa4\xeb\xa4\xc9", "world");
  BuildDictionary();

  scoped_ptr<ValueDictionary> dictionary(
      ValueDictionary::CreateValueDictionaryFromFile(dict_name_));
  const string lookup_key = "wo";
  Node *node = dictionary->LookupPredictive(lookup_key.c_str(),
                                            lookup_key.size(),
                                            &allocator);
  bool found = false;
  while (node) {
    if (node->value == "word") {
      found = true;
      break;
    }
    node = node->bnext;
  }
  EXPECT_TRUE(found);
}

TEST_F(ValueDictionaryTest, LookupPredictiveWithLimit) {
  NodeAllocator allocator;
  // "うぃー"
  AddToken("\xa4\xa6\xa4\xa3\xa1\xbc", "we");
  // "うぉー"
  AddToken("\xa4\xa6\xa4\xa9\xa1\xbc", "war");
  // "わーど"
  AddToken("\xa4\xef\xa1\xbc\xa4\xc9", "word");
  BuildDictionary();

  scoped_ptr<ValueDictionary> dictionary(
      ValueDictionary::CreateValueDictionaryFromFile(dict_name_));
  DictionaryInterface::Limit limit;
  Trie<string> trie;
  trie.AddEntry("e", "");
  trie.AddEntry("a", "");
  limit.begin_with_trie = &trie;

  const string lookup_key = "w";
  Node *node = dictionary->LookupPredictiveWithLimit(
      lookup_key.c_str(), lookup_key.size(), limit, &allocator);
  bool we_found = false;
  bool war_found = false;
  bool word_found = false;
  while (node) {
    if (node->value == "we") {
      we_found = true;
    } else if (node->value == "war") {
      war_found = true;
    } else if (node->value == "word") {
      word_found = true;
    }
    node = node->bnext;
  }
  EXPECT_TRUE(we_found);
  EXPECT_TRUE(war_found);
  EXPECT_FALSE(word_found);
}

}  // namespace mozc

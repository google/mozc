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

#include "dictionary/system/system_dictionary.h"

#include "base/base.h"
#include "base/singleton.h"
#include "base/trie.h"
#include "base/util.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/system/codec.h"
#include "dictionary/system/system_dictionary_builder.h"
#include "dictionary/text_dictionary_loader.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace {
// We cannot use #ifdef in DEFINE_int32.
#ifdef _DEBUG
const uint32 kDefaultReverseLookupTestSize = 1000;
#else
const uint32 kDefaultReverseLookupTestSize = 10000;
#endif
}  // namespace

DEFINE_string(
    dictionary_source,
    "data/dictionary/dictionary00.txt",
    "source dictionary file to run test");

DEFINE_int32(dictionary_test_size, 100000,
             "Dictionary size for this test.");
DEFINE_int32(dictionary_reverse_lookup_test_size, kDefaultReverseLookupTestSize,
             "Number of tokens to run reverse lookup test.");
DECLARE_string(test_srcdir);
DECLARE_string(test_tmpdir);

namespace mozc {


using dictionary::SystemDictionaryBuilder;
using dictionary::TokenInfo;

class SystemDictionaryTest : public testing::Test {
 protected:
  SystemDictionaryTest()
      : text_dict_(new TextDictionaryLoader(*Singleton<POSMatcher>::get())),
        dic_fn_(FLAGS_test_tmpdir + "/mozc.dic") {
    const string dic_path = Util::JoinPath(FLAGS_test_srcdir,
                                           FLAGS_dictionary_source);
    CHECK(text_dict_->OpenWithLineLimit(dic_path.c_str(),
                                        FLAGS_dictionary_test_size));
  }

  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }

  void BuildSystemDictionary(const vector <Token *>& tokens,
                             int num_tokens);
  Token* CreateToken(const string& key, const string& value) const;
  bool CompareForLookup(const Node *node, const Token *token,
                        bool reverse) const;
  // Only compares the higher byte since cost is sometimes encoded
  // into a byte.
  bool CompareCost(int c1, int c2) const {
    return abs(c1 - c2) < 256;
  }

  bool CompareNodeAndToken(const Token *token, const Node *node) const {
    if (node->lid == token->lid &&
        node->rid == token->rid &&
        CompareCost(node->wcost, token->cost) &&
        // key and value are opposite for reverse lookup.
        node->key == token->value &&
        node->value == token->key) {
      return true;
    }
    return false;
  }

  scoped_ptr<TextDictionaryLoader> text_dict_;
  const string dic_fn_;
};

void SystemDictionaryTest::BuildSystemDictionary(const vector <Token *>& source,
                                                 int num_tokens) {
  SystemDictionaryBuilder builder;
  vector<Token *> tokens;
  // Picks up first tokens.
  for (vector<Token *>::const_iterator it = source.begin();
       tokens.size() < num_tokens && it != source.end(); it++) {
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

// Return true if they seem to be same
bool SystemDictionaryTest::CompareForLookup(const Node *node,
                                            const Token *token,
                                            bool reverse) const {
  const bool key_value_check = reverse?
      (node->key == token->value && node->value == token->key) :
      (node->key == token->key && node->value == token->value);
  if (!key_value_check) {
    return false;
  }
  const uint16 token_lid = token->lid;
  const uint16 token_rid = token->rid;
  const bool comp_cost = CompareCost(node->wcost, token->cost);
  if (!comp_cost) {
    return false;
  }
  const bool spelling_match =
      (static_cast<bool>(token->attributes & Token::SPELLING_CORRECTION) ==
       static_cast<bool>(node->attributes & Node::SPELLING_CORRECTION));
  if (!spelling_match) {
    return false;
  }
  const bool id_match = (node->lid == token_lid) && (node->rid == token_rid);
  if (!id_match) {
    return false;
  }
  return true;
}

TEST_F(SystemDictionaryTest, test_normal_word) {
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
  CHECK(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  // Scans the tokens and check if they all exists.
  vector<Token *>::const_iterator it;
  for (it = source_tokens.begin(); it != source_tokens.end(); ++it) {
    bool found = false;
    Node *node = system_dic->LookupPrefix((*it)->key.c_str(), (*it)->key.size(),
                                          NULL);
    while (node) {
      if (CompareForLookup(node, *it, false)) {
        found = true;
      }
      Node *tmp_node = node;
      node = node->bnext;
      delete tmp_node;
    }
    EXPECT_TRUE(found) << "Failed to find " << (*it)->key.c_str() << ":"
                       << (*it)->value.c_str();
  }
}

TEST_F(SystemDictionaryTest, test_same_word) {
  vector<Token *> source_tokens;
  scoped_ptr<Token> t0(new Token);
  // "あ"
  t0->key = "\xe3\x81\x82";
  // "亜"
  t0->value = "\xe4\xba\x9c";
  t0->cost = 100;
  t0->lid = 50;
  t0->rid = 70;

  scoped_ptr<Token> t1(new Token);
  // "あ"
  t1->key = "\xe3\x81\x82";
  // "亜"
  t1->value = "\xe4\xba\x9c";
  t1->cost = 150;
  t1->lid = 100;
  t1->rid = 200;

  scoped_ptr<Token> t2(new Token);
  // "あ"
  t2->key = "\xe3\x81\x82";
  // "あ"
  t2->value = "\xe3\x81\x82";
  t2->cost = 100;
  t2->lid = 1000;
  t2->rid = 2000;

  scoped_ptr<Token> t3(new Token);
  // "あ"
  t3->key = "\xe3\x81\x82";
  // "亜"
  t3->value = "\xe4\xba\x9c";
  t3->cost = 1000;
  t3->lid = 2000;
  t3->rid = 3000;

  source_tokens.push_back(t0.get());
  source_tokens.push_back(t1.get());
  source_tokens.push_back(t2.get());
  source_tokens.push_back(t3.get());
  BuildSystemDictionary(source_tokens, FLAGS_dictionary_test_size);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  CHECK(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  // Scans the tokens and check if they all exists.
  vector<Token *>::const_iterator it;
  for (it = source_tokens.begin(); it != source_tokens.end(); ++it) {
    bool found = false;
    Node *node = system_dic->LookupPrefix((*it)->key.c_str(),
                                          (*it)->key.size(), NULL);
    while (node) {
      if (CompareForLookup(node, *it, false)) {
        found = true;
      }
      Node *tmp_node = node;
      node = node->bnext;
      delete tmp_node;
    }
    EXPECT_TRUE(found) << "Failed to find " << (*it)->key.c_str() << ":"
                       << (*it)->value.c_str();
  }
}

TEST_F(SystemDictionaryTest, test_words) {
  vector<Token *> source_tokens;
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, FLAGS_dictionary_test_size);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  CHECK(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  // Scans the tokens and check if they all exists.
  vector<Token *>::const_iterator it;
  for (it = source_tokens.begin(); it != source_tokens.end(); ++it) {
    bool found = false;
    Node *node = system_dic->LookupPrefix((*it)->key.c_str(), (*it)->key.size(),
                                          NULL);
    int count = 0;
    while (node) {
      ++count;
      if (CompareForLookup(node, *it, false)) {
        found = true;
      }
      Node *tmp_node = node;
      node = node->bnext;
      delete tmp_node;
    }
    EXPECT_TRUE(found) << "Failed to find " << (*it)->key << ":"
                       << (*it)->value << "\t" << (*it)->cost << "\t"
                       << (*it)->lid << "\t"
                       << (*it)->rid << "\tcount\t" << count;
    if (!found) {
      break;
    }
  }
}

TEST_F(SystemDictionaryTest, test_prefix) {
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
  CHECK(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  Node *node = system_dic->LookupPrefix(k1.c_str(), k1.size(), NULL);
  CHECK(node) << "no nodes found";
  bool found_k0 = false;
  while (node) {
    if (CompareForLookup(node, t0.get(), false)) {
      found_k0 = true;
    }
    Node *tmp_node = node;
    node = node->bnext;
    delete tmp_node;
  }
  EXPECT_TRUE(found_k0) << "Failed to find " << k0;
}

TEST_F(SystemDictionaryTest, test_predictive) {
  vector<Token *> source_tokens;

  // "まみむめも"
  // There are not be so many entries which start with "まみむめも".
  const string k0 = "\xe3\x81\xbe\xe3\x81\xbf\xe3\x82\x80"
      "\xe3\x82\x81\xe3\x82\x82";
  // "まみむめもや"
  const string k1 = "\xe3\x81\xbe\xe3\x81\xbf\xe3\x82\x80"
      "\xe3\x82\x81\xe3\x82\x82\xe3\x82\x84";
  // "まみむめもやゆよ"
  const string k2 = "\xe3\x81\xbe\xe3\x81\xbf\xe3\x82\x80"
      "\xe3\x82\x81\xe3\x82\x82\xe3\x82\x84\xe3\x82\x86\xe3\x82\x88";

  scoped_ptr<Token> t1(CreateToken(k1, "aa"));
  scoped_ptr<Token> t2(CreateToken(k2, "bb"));
  source_tokens.push_back(t1.get());
  source_tokens.push_back(t2.get());
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 10000);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  CHECK(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  Node *node = system_dic->LookupPredictive(k0.c_str(), k0.size(), NULL);
  CHECK(node) << "no nodes found";
  bool found_k1 = false;
  bool found_k2 = false;
  while (node) {
    if (CompareForLookup(node, t1.get(), false)) {
      found_k1 = true;
    }
    if (CompareForLookup(node, t2.get(), false)) {
      found_k2 = true;
    }
    Node *tmp_node = node;
    node = node->bnext;
    delete tmp_node;
  }
  EXPECT_TRUE(found_k1) << "Failed to find " << k1;
  EXPECT_TRUE(found_k2) << "Failed to find " << k2;
}

TEST_F(SystemDictionaryTest, test_predictive_with_limit) {
  vector<Token *> source_tokens;

  // "まみむめも"
  // There are not be so many entries which start with "まみむめも".
  const string k0 = "\xe3\x81\xbe\xe3\x81\xbf\xe3\x82\x80"
      "\xe3\x82\x81\xe3\x82\x82";
  // "まみむめもや"
  const string k1 = "\xe3\x81\xbe\xe3\x81\xbf\xe3\x82\x80"
      "\xe3\x82\x81\xe3\x82\x82\xe3\x82\x84";
  // "まみむめもやゆよ"
  const string k2 = "\xe3\x81\xbe\xe3\x81\xbf\xe3\x82\x80"
      "\xe3\x82\x81\xe3\x82\x82\xe3\x82\x84\xe3\x82\x86\xe3\x82\x88";
  // "まみむめもままま"
  const string k3 = "\xe3\x81\xbe\xe3\x81\xbf\xe3\x82\x80\xe3\x82\x81"
      "\xe3\x82\x82\xe3\x81\xbe\xe3\x81\xbe\xe3\x81\xbe";

  scoped_ptr<Token> t1(CreateToken(k1, "aa"));
  scoped_ptr<Token> t2(CreateToken(k2, "bb"));
  scoped_ptr<Token> t3(CreateToken(k3, "cc"));
  source_tokens.push_back(t1.get());
  source_tokens.push_back(t2.get());
  source_tokens.push_back(t3.get());
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 10000);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  CHECK(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  DictionaryInterface::Limit limit;
  Trie<string> trie;
  // "や"
  trie.AddEntry("\xe3\x82\x84", "");
  limit.begin_with_trie = &trie;
  Node *node = system_dic->LookupPredictiveWithLimit(k0.c_str(), k0.size(),
                                                     limit, NULL);
  EXPECT_TRUE(node != NULL) << "no nodes found";
  bool found_k1 = false;
  bool found_k2 = false;
  bool found_k3 = false;
  while (node) {
    if (CompareForLookup(node, t1.get(), false)) {
      found_k1 = true;
    }
    if (CompareForLookup(node, t2.get(), false)) {
      found_k2 = true;
    }
    if (CompareForLookup(node, t3.get(), false)) {
      found_k3 = true;
    }
    Node *tmp_node = node;
    node = node->bnext;
    delete tmp_node;
  }
  EXPECT_TRUE(found_k1) << "Failed to find " << k1;
  EXPECT_TRUE(found_k2) << "Failed to find " << k2;
  EXPECT_FALSE(found_k3) << "Failed to filter " << k3;
}

TEST_F(SystemDictionaryTest, test_predictive_cutoff) {
  vector<Token *> source_tokens;

  // "あ"
  // There are a lot of entries which start with "あ".
  const string k0 = "\xe3\x81\x82";
  // "あい"
  const string k1 = "\xe3\x81\x82\xe3\x81\x84";
  // "あいうえお"
  const string k2 =
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a";

  scoped_ptr<Token> t1(CreateToken(k1, "aa"));
  scoped_ptr<Token> t2(CreateToken(k2, "bb"));
  source_tokens.push_back(t1.get());
  source_tokens.push_back(t2.get());
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 10000);

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  CHECK(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  Node *node = system_dic->LookupPredictive(k0.c_str(), k0.size(), NULL);
  CHECK(node) << "no nodes found";
  bool found_k1 = false;
  bool found_k2 = false;
  int found_count = 0;
  while (node) {
    ++found_count;
    if (CompareForLookup(node, t1.get(), false)) {
      found_k1 = true;
    }
    if (CompareForLookup(node, t2.get(), false)) {
      found_k2 = true;
    }
    Node *tmp_node = node;
    node = node->bnext;
    delete tmp_node;
  }
  EXPECT_GE(found_count, 64);
  EXPECT_TRUE(found_k1) << "Failed to find " << k1;
  // We don't return all results and return only for 'short key' entry
  // if too many key are found by predictive lookup of key.
  EXPECT_FALSE(found_k2) << "Failed to find " << k2;
}

TEST_F(SystemDictionaryTest, test_reverse) {
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
  CHECK(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;
  vector<Token *>::const_iterator it;
  int size = FLAGS_dictionary_reverse_lookup_test_size;
  for (it = source_tokens.begin();
       size > 0 && it != source_tokens.end(); ++it, --size) {
    const Token *t = *it;
    Node *node = system_dic->LookupReverse(t->value.c_str(),
                                           t->value.size(),
                                           NULL);
    bool found = false;
    int count = 0;
    while (node != NULL) {
      ++count;
      // Make sure any of the key lengths of the lookup results
      // doesn't exceed the original key length.
      // It happened once
      // when called with "バージョン", returning "ヴァージョン".
      EXPECT_LE(node->key.size(), t->value.size())
          << string(node->key) << ":" << string(node->value)
          << "\t" << string(t->value);
      if (CompareForLookup(node, t, true)) {
        found = true;
      }
      Node *tmp_node = node;
      node = node->bnext;
      delete tmp_node;
    }
    if (t->attributes & Token::SPELLING_CORRECTION) {
      EXPECT_FALSE(found)
          << "Spelling correction token was retrieved:"
          << t->key << ":" << t->value;
      if (found) {
        return;
      }
    } else {
      EXPECT_TRUE(found) << "Failed to find " << t->key << ":" << t->value;
      if (!found) {
        return;
      }
    }
  }

  // test for non exact transliterated index string.
  // append "が"
  const string key = t7->value + "\xe3\x81\x8c";
  Node *node = system_dic->LookupReverse(key.c_str(),
                                         key.size(),
                                         NULL);
  bool found = false;
  while (node != NULL) {
    if (CompareNodeAndToken(t7.get(), node)) {
      found = true;
    }
    Node *tmp_node = node;
    node = node->bnext;
    delete tmp_node;
  }
  EXPECT_TRUE(found)
      << "Missed node for non exact transliterated index" << key;
}

TEST_F(SystemDictionaryTest, test_reverse_cache) {
  const string kDoraemon =
      "\xe3\x83\x89\xe3\x83\xa9\xe3\x81\x88\xe3\x82\x82\xe3\x82\x93";

  scoped_ptr<Token> t1(new Token);
  // "どらえもん"
  t1->key = "\xe3\x81\xa9\xe3\x82\x89\xe3\x81\x88\xe3\x82\x82\xe3\x82\x93";
  // "ドラえもん"
  t1->value = kDoraemon;
  t1->cost = 1;
  t1->lid = 2;
  t1->rid = 3;
  vector<Token *> source_tokens;
  source_tokens.push_back(t1.get());
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, source_tokens.size());

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  CHECK(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;
  NodeAllocator allocator;
  system_dic->PopulateReverseLookupCache(kDoraemon.c_str(), kDoraemon.size(),
                                         &allocator);
  Node *node = system_dic->LookupReverse(kDoraemon.c_str(),
                                         kDoraemon.size(),
                                         &allocator);
  bool found = false;
  while (node != NULL) {
    if (node->key == kDoraemon) {
      found = true;
    }
    node = node->bnext;
  }
  EXPECT_TRUE(found) << "Could not find " << t1->value;
  system_dic->ClearReverseLookupCache(&allocator);
}

TEST_F(SystemDictionaryTest, nodes_size) {
  vector<Token *> source_tokens;
  vector<Token *> added_tokens;
  string s;
  for (int i = 0; i < 10; ++i) {
    s += "1";
    Token *t = CreateToken(s, "1");
    source_tokens.push_back(t);
    added_tokens.push_back(t);
  }
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 10000);
  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  CHECK(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  const int kNumNodes = 5;

  // Tests LookupPrefix and LookupReverse.
  NodeAllocator allocator1;
  allocator1.set_max_nodes_size(kNumNodes);
  Node *node = system_dic->LookupPrefix(s.c_str(), s.size(), &allocator1);
  int count = 0;
  for (Node *tmp = node; tmp; tmp = tmp->bnext) {
    ++count;
  }
  EXPECT_EQ(kNumNodes, count);

  NodeAllocator allocator2;
  allocator2.set_max_nodes_size(kNumNodes);
  node = system_dic->LookupReverse("1", 1, &allocator2);
  count = 0;
  for (Node *tmp = node; tmp; tmp = tmp->bnext) {
    ++count;
  }
  EXPECT_EQ(kNumNodes, count);

  for (int i = 0; i < added_tokens.size(); ++i) {
    // Deletes them manually instead of STLDeleteContainerPointers
    // for portability.
    delete added_tokens[i];
  }
}

TEST_F(SystemDictionaryTest, spelling_correction_tokens) {
  scoped_ptr<Token> t1(new Token);
  // "あぼがど"
  t1->key = "\xe3\x81\x82\xe3\x81\xbc\xe3\x81\x8c\xe3\x81\xa9";
  // "アボカド"
  t1->value = "\xe3\x82\xa2\xe3\x83\x9c\xe3\x82\xab\xe3\x83\x89";
  t1->cost = 1;
  t1->lid = 0;
  t1->rid = 2;
  t1->attributes = Token::SPELLING_CORRECTION;

  scoped_ptr<Token> t2(new Token);
  // "しゅみれーしょん"
  t2->key =
      "\xe3\x81\x97\xe3\x82\x85\xe3\x81\xbf\xe3\x82\x8c"
      "\xe3\x83\xbc\xe3\x81\x97\xe3\x82\x87\xe3\x82\x93";
  // "シミュレーション"
  t2->value =
      "\xe3\x82\xb7\xe3\x83\x9f\xe3\x83\xa5\xe3\x83\xac"
      "\xe3\x83\xbc\xe3\x82\xb7\xe3\x83\xa7\xe3\x83\xb3";
  t2->cost = 1;
  t2->lid = 100;
  t2->rid = 3;
  t2->attributes = Token::SPELLING_CORRECTION;

  scoped_ptr<Token> t3(new Token);
  // "あきはばら"
  t3->key = "\xe3\x81\x82\xe3\x81\x8d\xe3\x81\xaf\xe3\x81\xb0\xe3\x82\x89";
  // "秋葉原"
  t3->value = "\xe7\xa7\x8b\xe8\x91\x89\xe5\x8e\x9f";
  t3->cost = 1000;
  t3->lid = 1;
  t3->rid = 2;

  vector<Token *> source_tokens;
  source_tokens.push_back(t1.get());
  source_tokens.push_back(t2.get());
  source_tokens.push_back(t3.get());
  BuildSystemDictionary(source_tokens, source_tokens.size());

  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  CHECK(system_dic.get() != NULL)
      << "Failed to open dictionary source:" << dic_fn_;

  vector<Token *>::const_iterator it;
  for (it = source_tokens.begin(); it != source_tokens.end(); ++it) {
    Node *node = system_dic->LookupPrefix((*it)->key.c_str(),
                                          (*it)->key.size(), NULL);
    bool found = false;
    while (node) {
      if (CompareForLookup(node, *it, false)) {
        found = true;
      }
      Node *tmp_node = node;
      node = node->bnext;
      delete tmp_node;
    }
    EXPECT_TRUE(found) << "Failed to find " << (*it)->key
                       << ":" << (*it)->value;
  }
}

// Minimal modification of the codec for the TokenAfterSpellningToken.
class CodecForTest : public dictionary::SystemDictionaryCodec {
 public:
  CodecForTest() {
  }

  virtual ~CodecForTest() {
  }

  static const char *kDummyValue;

  void DecodeValue(const string &src, string *dst) const {
    *dst = kDummyValue;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CodecForTest);
};

const char *CodecForTest::kDummyValue = "DummyValue";

TEST_F(SystemDictionaryTest, TokenAfterSpellningToken) {
  scoped_ptr<SystemDictionary> system_dic(
      SystemDictionary::CreateSystemDictionaryFromFile(dic_fn_));
  // Filter for reverse look up.
  SystemDictionary::FilterInfo filter;
  filter.conditions = SystemDictionary::FilterInfo::NO_SPELLING_CORRECTION;

  // The 2nd token refers previous token by SAME_AS_PREV_VALUE,
  // but the 1st token is spelling correction which will be ignored for
  // reverse conversion.
  vector<TokenInfo> tokens;
  scoped_ptr<Token> t0(new Token);
  t0->attributes = Token::SPELLING_CORRECTION;
  t0->cost = 1;
  tokens.push_back(dictionary::TokenInfo(t0.get()));
  scoped_ptr<Token> t1(new Token);
  t1->cost = 111;
  tokens.push_back(dictionary::TokenInfo(t1.get()));
  tokens[1].value_type = TokenInfo::SAME_AS_PREV_VALUE;

  // Inject a minimal codec.
  scoped_ptr<CodecForTest>
      codec(new CodecForTest());
  system_dic->codec_ = codec.get();

  const string tokens_key;
  Node *head = NULL;
  int limit = 10000;
  Node *res = system_dic->AppendNodesFromTokens(filter, tokens_key, &tokens,
                                                head, NULL, &limit);

  vector<Node *> nodes;
  while (res) {
    nodes.push_back(res);
    res = res->bnext;
  }
  // The 2nd token should survive with the same value of 1st token.
  EXPECT_EQ(1, nodes.size());
  EXPECT_EQ(111, nodes[0]->wcost);
  EXPECT_EQ(CodecForTest::kDummyValue, nodes[0]->value);

  for (size_t i = 0; i < nodes.size(); ++i) {
    delete nodes[i];
  }
}
}  // namespace mozc

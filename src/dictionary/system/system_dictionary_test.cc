// Copyright 2010-2011, Google Inc.
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
#include "base/util.h"
#include "converter/node.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/text_dictionary_loader.h"
#include "dictionary/system/system_dictionary_builder.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DEFINE_string(
    dictionary_source,
    "data/dictionary/dictionary00.txt",
    "source dictionary file to run test");

DEFINE_int32(dictionary_test_size, 100000,
             "Dictionary size for this test.");
DECLARE_string(test_srcdir);

namespace mozc {
namespace {

class TestNodeAllocator : public NodeAllocatorInterface {
 public:
  TestNodeAllocator() {}
  virtual ~TestNodeAllocator() {
    for (size_t i = 0; i < nodes_.size(); ++i) {
      delete nodes_[i];
    }
    nodes_.clear();
  }

  virtual Node *NewNode() {
    Node *node = new Node;
    CHECK(node);
    node->Init();
    nodes_.push_back(node);
    return node;
  }

 private:
  vector<Node *> nodes_;
};


class SystemDictionaryTest : public testing::Test {
 protected:
  SystemDictionaryTest()
      : text_dict_(new TextDictionaryLoader),
        dic_fn_(FLAGS_test_tmpdir + "/mozc.dic") {
    const string dic_path = Util::JoinPath(FLAGS_test_srcdir,
                                           FLAGS_dictionary_source);
    text_dict_->OpenWithLineLimit(dic_path.c_str(),
                                  FLAGS_dictionary_test_size);
  }
  void BuildSystemDictionary(const vector <Token *>& tokens,
                             int num_tokens);
  Token* CreateToken(const string& key, const string& value) const;
  // Only compares the higher byte since cost is sometimes encoded
  // into a byte.
  bool CompareCost(int c1, int c2) const {
    return abs(c1 - c2) < 256;
  }

  scoped_ptr<TextDictionaryLoader> text_dict_;
  const string dic_fn_;
};

void SystemDictionaryTest::BuildSystemDictionary(const vector <Token *>& source,
                                                 int num_tokens) {
  SystemDictionaryBuilder builder("dummy", dic_fn_.c_str());
  vector<Token *> tokens;
  vector<Token *>::const_iterator it;
  // Picks up first tokens.
  for (it = source.begin();
       tokens.size() < num_tokens &&
           it != source.end(); it++) {
    tokens.push_back(*it);
  }
  LOG(INFO) << "Start to build dictionary";
  builder.BuildFromTokenVector(tokens);
  LOG(INFO) << "Built dictionary";
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

TEST_F(SystemDictionaryTest, test_words) {
  vector<Token *> source_tokens;
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, FLAGS_dictionary_test_size);

  scoped_ptr<SystemDictionary> rx_dic(new SystemDictionary);
  CHECK(rx_dic->Open(dic_fn_.c_str()))
      << "Failed to open dictionary source:" << dic_fn_;

  // Scans the tokens and check if they all exists.
  vector<Token *>::const_iterator it;
  for (it = source_tokens.begin(); it != source_tokens.end(); it++) {
    bool found = false;
    Node *node = rx_dic->LookupPrefix((*it)->key.c_str(), (*it)->key.size(),
                                      NULL);
    while (node) {
      if (node->key == (*it)->key &&
          node->value == (*it)->value &&
          CompareCost(node->wcost, (*it)->cost) &&
          node->lid == (*it)->lid &&
          node->rid == (*it)->rid) {
        found = true;
      }
      Node *tmp_node = node;
      node = node->bnext;
      delete tmp_node;
    }
    CHECK(found) << "Failed to find " << (*it)->key.c_str() << ":"
                 << (*it)->value.c_str();
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

  scoped_ptr<SystemDictionary> rx_dic(new SystemDictionary);
  CHECK(rx_dic->Open(dic_fn_.c_str()))
      << "Failed to open dictionary source:" << dic_fn_;
  Node *node = rx_dic->LookupPrefix(k1.c_str(), k1.size(), NULL);
  CHECK(node) << "no nodes found";
  bool found_k0 = false;
  while (node) {
    if (node->key == k0 && node->value == "aa") {
      found_k0 = true;
    }
    Node *tmp_node = node;
    node = node->bnext;
    delete tmp_node;
  }
  CHECK(found_k0) << "failed to find :" << k0;
}

TEST_F(SystemDictionaryTest, test_predictive) {
  vector<Token *> source_tokens;

  // "は"
  const string k0 = "\xe3\x81\xaf";
  // "はひふ"
  const string k1 = "\xe3\x81\xaf\xe3\x81\xb2\xe3\x81\xb5";
  // "はひふへほ"
  const string k2 = "\xe3\x81\xaf\xe3\x81\xb2\xe3\x81\xb5\xe3\x81\xb8\xe3\x81"
      "\xbb";

  scoped_ptr<Token> t1(CreateToken(k1, "aa"));
  scoped_ptr<Token> t2(CreateToken(k2, "bb"));
  source_tokens.push_back(t1.get());
  source_tokens.push_back(t2.get());
  text_dict_->CollectTokens(&source_tokens);
  BuildSystemDictionary(source_tokens, 10000);

  scoped_ptr<SystemDictionary> rx_dic(new SystemDictionary);
  CHECK(rx_dic->Open(dic_fn_.c_str()))
      << "Failed to open dictionary source:" << dic_fn_;

  Node *node = rx_dic->LookupPredictive(k0.c_str(), k0.size(), NULL);
  CHECK(node) << "no nodes found";
  bool found_k1 = false;
  bool found_k2 = false;
  while (node) {
    // LOG(INFO) << node->key << ":" << node->value;
    if (node->key == k1 && node->value == "aa") {
      found_k1 = true;
    }
    if (node->key == k2 && node->value == "bb") {
      found_k2 = true;
    }
    Node *tmp_node = node;
    node = node->bnext;
    delete tmp_node;
  }
  CHECK(found_k1) << "failed to find :" << k1;
  CHECK(found_k2) << "failed to find :" << k2;
}

TEST_F(SystemDictionaryTest, index_coding) {
  scoped_ptr<SystemDictionary> rx_dic(new SystemDictionary);
  // "mozc・もずくー"
  string src("\x6d\x6f\x7a\x63"
             "\xe3\x83\xbb"
             "\xe3\x82\x82\xe3\x81\x9a\xe3\x81\x8f"
             "\xe3\x83\xbc");
  string encoded;
  rx_dic->EncodeIndexString(src, &encoded);
  string decoded;
  rx_dic->DecodeIndexString(encoded, &decoded);
  EXPECT_EQ(src, decoded);
}

TEST_F(SystemDictionaryTest, index_coding_all) {
  scoped_ptr<SystemDictionary> rx_dic(new SystemDictionary);
  string src;
  for (int i = 0; i < 255; ++i) {
    // Appends characters in all blocks. Though some might be invalid as
    // a character. This should safely encode/decode them.
    Util::UCS2ToUTF8Append(i * 256 + 1, &src);
  }
  string encoded;
  rx_dic->EncodeIndexString(src, &encoded);
  string decoded;
  rx_dic->DecodeIndexString(encoded, &decoded);
  EXPECT_EQ(src, decoded);
}

TEST_F(SystemDictionaryTest, token_coding) {
  scoped_ptr<SystemDictionary> rx_dic(new SystemDictionary);
  // "mozcもずく百雲"
  string src("\x6d\x6f\x7a\x63\xe3\x82\x82\xe3\x81\x9a\xe3\x81\x8f\xe7\x99\xbe"
             "\xe9\x9b\xb2");
  string encoded;
  string decoded;
  rx_dic->EncodeTokenString(src, &encoded);
  rx_dic->DecodeTokenString(encoded, &decoded);
  EXPECT_EQ(src, decoded);

  // "紀" is 0x7d00, so 00 handling
  // "政紀"
  src = "\xe6\x94\xbf\xe7\xb4\x80";
  encoded = "";
  decoded = "";
  rx_dic->EncodeTokenString(src, &encoded);
  rx_dic->DecodeTokenString(encoded, &decoded);
  EXPECT_EQ(src, decoded);

  // ASCII character.
  // "8月"
  src = "\x38\xe6\x9c\x88";
  encoded = "";
  decoded = "";
  rx_dic->EncodeTokenString(src, &encoded);
  rx_dic->DecodeTokenString(encoded, &decoded);
  EXPECT_EQ(src, decoded);

  // 0x30ce, above hiragana range.
  // "ノ"
  src = "\xe3\x83\x8e";
  encoded = "";
  decoded = "";
  rx_dic->EncodeTokenString(src, &encoded);
  rx_dic->DecodeTokenString(encoded, &decoded);
  EXPECT_EQ(src, decoded);

  // ぁァー
  src = "\xe3\x81\x81\xe3\x82\xa1\xe3\x83\xbc";
  encoded = "";
  decoded = "";
  rx_dic->EncodeTokenString(src, &encoded);
  rx_dic->DecodeTokenString(encoded, &decoded);
  EXPECT_EQ(src, decoded);
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
  scoped_ptr<SystemDictionary> rx_dic(new SystemDictionary);
  CHECK(rx_dic->Open(dic_fn_.c_str()))
      << "Failed to open dictionary source:" << dic_fn_;

  const int kNumNodes = 5;

  // Tests LookupPrefix
  TestNodeAllocator allocator1;
  allocator1.set_max_nodes_size(kNumNodes);
  Node *node = rx_dic->LookupPrefix(s.c_str(), s.size(), &allocator1);
  int count = 0;
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
  t1->lid = 0 + SystemDictionary::kSpellingCorrectionPosOffset;
  t1->rid = 2;

  scoped_ptr<Token> t2(new Token);
  // "しゅみれーしょん"
  t2->key = "\xe3\x81\x97\xe3\x82\x85\xe3\x81\xbf\xe3\x82\x8c"
      "\xe3\x83\xbc\xe3\x81\x97\xe3\x82\x87\xe3\x82\x93";
  // "シミュレーション"
  t2->value = "\xe3\x82\xb7\xe3\x83\x9f\xe3\x83\xa5\xe3\x83"
      "\xac\xe3\x83\xbc\xe3\x82\xb7\xe3\x83\xa7\xe3\x83\xb3";
  t2->cost = 1;
  t2->lid = 100 + SystemDictionary::kSpellingCorrectionPosOffset;
  t2->rid = 3;

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

  scoped_ptr<SystemDictionary> system_dic(new SystemDictionary);
  CHECK(system_dic->Open(dic_fn_.c_str()))
      << "Failed to open dictionary source:" << dic_fn_;

  vector<Token *>::const_iterator it;
  for (it = source_tokens.begin(); it != source_tokens.end(); ++it) {
    const Token *t = *it;
    Node *node = system_dic->LookupPrefix(t->key.c_str(), t->key.size(),
                                          NULL);
    while (node) {
      if (node->key == t->key) {
        EXPECT_EQ(static_cast<bool>(
            node->attributes & Node::SPELLING_CORRECTION),
                  t->lid >= SystemDictionary::kSpellingCorrectionPosOffset);

        if (node->attributes & Node::SPELLING_CORRECTION) {
          EXPECT_EQ(node->lid,
                    t->lid - SystemDictionary::kSpellingCorrectionPosOffset);
        } else {
          EXPECT_EQ(node->lid, t->lid);
        }

        EXPECT_EQ(node->rid, t->rid);
        EXPECT_TRUE(CompareCost(, t->cost));
        EXPECT_EQ(node->key, t->key);
        EXPECT_EQ(node->value, t->value);
      }

      Node *tmp_node = node;
      node = node->bnext;
      delete tmp_node;
    }
  }
}
}  // namespace
}  // namespace mozc

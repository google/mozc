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

#include "dictionary/dictionary_interface.h"
#include "dictionary/suppression_dictionary.h"
#include "base/base.h"
#include "converter/node.h"
#include "testing/base/public/gunit.h"

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

  Node *NewNode() {
    Node *node = new Node;
    CHECK(node);
    node->Init();
    nodes_.push_back(node);
    return node;
  }

 private:
  vector<Node *> nodes_;
};
}  // namespace

TEST(Dictionary_test, basic) {
  // delete without Open()
  DictionaryInterface *d1 = DictionaryFactory::GetDictionary();
  EXPECT_TRUE(d1 != NULL);

  DictionaryInterface *d2 = DictionaryFactory::GetDictionary();
  EXPECT_TRUE(d2 != NULL);

  EXPECT_EQ(d1, d2);
}

TEST(Dictionary_test, WordSuppressionTest) {
  DictionaryInterface *d = DictionaryFactory::GetDictionary();
  SuppressionDictionary *s =
      SuppressionDictionary::GetSuppressionDictionary();

  TestNodeAllocator allocator;

  // "ぐーぐるは"
  const char kQuery[] = "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90"
      "\xE3\x82\x8B\xE3\x81\xAF";

  // "ぐーぐる"
  const char kKey[]  = "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90"
      "\xE3\x82\x8B";

  // "グーグル"
  const char kValue[] = "\xE3\x82\xB0\xE3\x83\xBC\xE3"
      "\x82\xB0\xE3\x83\xAB";

  {
    s->Lock();
    s->Clear();
    s->AddEntry(kKey, kValue);
    s->UnLock();
    Node *node = d->LookupPrefix(kQuery, strlen(kQuery), &allocator);
    bool found = false;
    for (; node != NULL; node = node->bnext) {
      if (node->key == kKey && node->value == kValue) {
        found = true;
      }
    }

    EXPECT_FALSE(found);
  }

  {
    s->Lock();
    s->Clear();
    s->UnLock();
    Node *node = d->LookupPrefix(kQuery, strlen(kQuery), &allocator);
    bool found = false;
    for (; node != NULL; node = node->bnext) {
      if (node->key == kKey && node->value == kValue) {
        found = true;
      }
    }

    EXPECT_TRUE(found);
  }
}
}  // namespace mozc

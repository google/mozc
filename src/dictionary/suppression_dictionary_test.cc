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

#include "dictionary/suppression_dictionary.h"

#include "base/base.h"
#include "base/file_stream.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "base/util.h"
#include "converter/node.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

TEST(SupressionDictionary, BasicTest) {
  SuppressionDictionary *dic = Singleton<SuppressionDictionary>::get();
  CHECK(dic);

  // repeat 10 times
  for (int i = 0; i < 10; ++i) {
    // Not locked
    EXPECT_FALSE(dic->AddEntry("test", "test"));

    dic->Lock();
    // IsEmpty() returns always true when dic is locked
    EXPECT_TRUE(dic->IsEmpty());
    EXPECT_FALSE(dic->AddEntry("", ""));
    EXPECT_TRUE(dic->AddEntry("key1", "value1"));
    EXPECT_TRUE(dic->AddEntry("key2", "value2"));
    EXPECT_TRUE(dic->AddEntry("key3", "value3"));
    EXPECT_TRUE(dic->AddEntry("key4", ""));
    EXPECT_TRUE(dic->AddEntry("key5", ""));
    EXPECT_TRUE(dic->AddEntry("", "value4"));
    EXPECT_TRUE(dic->AddEntry("", "value5"));
    EXPECT_TRUE(dic->IsEmpty());
    dic->UnLock();

    EXPECT_FALSE(dic->IsEmpty());

    // Not locked
    EXPECT_FALSE(dic->AddEntry("test", "test"));

    // locked now => SuppressEntry always returns false
    dic->Lock();
    EXPECT_FALSE(dic->SuppressEntry("key1", "value1"));
    dic->UnLock();

    EXPECT_TRUE(dic->SuppressEntry("key1", "value1"));
    EXPECT_TRUE(dic->SuppressEntry("key2", "value2"));
    EXPECT_TRUE(dic->SuppressEntry("key3", "value3"));
    EXPECT_TRUE(dic->SuppressEntry("key4", ""));
    EXPECT_TRUE(dic->SuppressEntry("key5", ""));
    EXPECT_TRUE(dic->SuppressEntry("", "value4"));
    EXPECT_TRUE(dic->SuppressEntry("", "value5"));
    EXPECT_FALSE(dic->SuppressEntry("key1", ""));
    EXPECT_FALSE(dic->SuppressEntry("key2", ""));
    EXPECT_FALSE(dic->SuppressEntry("key3", ""));
    EXPECT_FALSE(dic->SuppressEntry("", "value1"));
    EXPECT_FALSE(dic->SuppressEntry("", "value2"));
    EXPECT_FALSE(dic->SuppressEntry("", "value3"));
    EXPECT_FALSE(dic->SuppressEntry("key1", "value2"));
    EXPECT_TRUE(dic->SuppressEntry("key4", "value2"));
    EXPECT_TRUE(dic->SuppressEntry("key4", "value3"));
    EXPECT_TRUE(dic->SuppressEntry("key5", "value0"));
    EXPECT_TRUE(dic->SuppressEntry("key5", "value4"));
    EXPECT_TRUE(dic->SuppressEntry("key0", "value5"));
    EXPECT_FALSE(dic->SuppressEntry("", ""));

    dic->Lock();
    dic->Clear();
    dic->UnLock();
  }
}

Node *MakeNodes(const vector<Node *>& nodes) {
  Node *head = nodes[0];
  Node *prev = NULL;
  for (int i = 0; i < nodes.size(); ++i) {
    Node *node = nodes[i];
    node->key = "key" + Util::SimpleItoa(i);
    node->value = "value" + Util::SimpleItoa(i);
    if (prev != NULL) {
      prev->bnext = node;
    }
    prev = node;
  }
  prev->bnext = NULL;
  return head;
}

TEST(SupressionDictionary, SuppressNodesTest) {
  vector<Node *> nodes;
  for (int i = 0; i < 10; ++i) {
    nodes.push_back(new Node);
  }

  SuppressionDictionary *dic = Singleton<SuppressionDictionary>::get();
  CHECK(dic);

  EXPECT_EQ(static_cast<Node*>(NULL), dic->SuppressNodes(NULL));

  // head item is deleted
  {
    dic->Lock();
    dic->Clear();
    dic->AddEntry("key0", "value0");
    dic->UnLock();
    Node *head = MakeNodes(nodes);
    Node *result = dic->SuppressNodes(head);
    EXPECT_NE(static_cast<Node*>(NULL), result);
    int size = 0;
    for (Node *node = result; node != NULL; node = node->bnext) {
      ++size;
      EXPECT_NE("key0", node->key);
      EXPECT_NE("value0", node->value);
    }
    EXPECT_EQ(9, size);
  }

  // head two items
  {
    dic->Lock();
    dic->Clear();
    dic->AddEntry("key0", "value0");
    dic->AddEntry("key1", "value1");
    dic->UnLock();
    Node *head = MakeNodes(nodes);
    Node *result = dic->SuppressNodes(head);
    EXPECT_NE(static_cast<Node*>(NULL), result);
    int size = 0;
    for (Node *node = result; node != NULL; node = node->bnext) {
      ++size;
      EXPECT_NE("key0", node->key);
      EXPECT_NE("value0", node->value);
      EXPECT_NE("key1", node->key);
      EXPECT_NE("value1", node->value);
    }
    EXPECT_EQ(8, size);
  }

  // tail item is deleted
  {
    dic->Lock();
    dic->Clear();
    dic->AddEntry("key9", "value9");
    dic->UnLock();
    Node *head = MakeNodes(nodes);
    Node *result = dic->SuppressNodes(head);
    EXPECT_NE(static_cast<Node*>(NULL), result);
    int size = 0;
    for (Node *node = result; node != NULL; node = node->bnext) {
      ++size;
      EXPECT_NE("key9", node->key);
      EXPECT_NE("value9", node->value);
    }
    EXPECT_EQ(9, size);
  }

  // tail two items
  {
    dic->Lock();
    dic->Clear();
    dic->AddEntry("key8", "value8");
    dic->AddEntry("key9", "value9");
    dic->UnLock();
    Node *head = MakeNodes(nodes);
    Node *result = dic->SuppressNodes(head);
    EXPECT_NE(static_cast<Node*>(NULL), result);
    int size = 0;
    for (Node *node = result; node != NULL; node = node->bnext) {
      ++size;
      EXPECT_NE("key8", node->key);
      EXPECT_NE("value8", node->value);
      EXPECT_NE("key9", node->key);
      EXPECT_NE("value9", node->value);
    }
    EXPECT_EQ(8, size);
  }

  // other random cases
  {
    dic->Lock();
    dic->Clear();
    dic->AddEntry("key0", "value0");
    dic->AddEntry("key2", "value2");
    dic->UnLock();
    Node *head = MakeNodes(nodes);
    Node *result = dic->SuppressNodes(head);
    EXPECT_NE(static_cast<Node*>(NULL), result);
    int size = 0;
    for (Node *node = result; node != NULL; node = node->bnext) {
      ++size;
      EXPECT_NE("key0", node->key);
      EXPECT_NE("value2", node->value);
      EXPECT_NE("key0", node->key);
      EXPECT_NE("value2", node->value);
    }
    EXPECT_EQ(8, size);
  }

  // other random cases
  {
    dic->Lock();
    dic->Clear();
    dic->AddEntry("key3", "value3");
    dic->AddEntry("key4", "value4");
    dic->UnLock();
    Node *head = MakeNodes(nodes);
    Node *result = dic->SuppressNodes(head);
    EXPECT_NE(static_cast<Node*>(NULL), result);
    int size = 0;
    for (Node *node = result; node != NULL; node = node->bnext) {
      ++size;
      EXPECT_NE("key3", node->key);
      EXPECT_NE("value4", node->value);
      EXPECT_NE("key3", node->key);
      EXPECT_NE("value4", node->value);
    }
    EXPECT_EQ(8, size);
  }

  // other random cases
  {
    dic->Lock();
    dic->Clear();
    dic->AddEntry("key5", "value5");
    dic->AddEntry("key8", "value8");
    dic->UnLock();
    Node *head = MakeNodes(nodes);
    Node *result = dic->SuppressNodes(head);
    EXPECT_NE(static_cast<Node*>(NULL), result);
    int size = 0;
    for (Node *node = result; node != NULL; node = node->bnext) {
      ++size;
      EXPECT_NE("key5", node->key);
      EXPECT_NE("value8", node->value);
      EXPECT_NE("key8", node->key);
      EXPECT_NE("value8", node->value);
    }
    EXPECT_EQ(8, size);
  }

  // all removed
  {
    dic->Lock();
    dic->Clear();
    for (int i = 0; i < 10; ++i) {
      dic->AddEntry("key" + Util::SimpleItoa(i),
                    "value" + Util::SimpleItoa(i));
    }
    dic->UnLock();
    Node *head = MakeNodes(nodes);
    Node *result = dic->SuppressNodes(head);
    EXPECT_EQ(static_cast<Node*>(NULL), result);
  }

  for (int i = 0; i < nodes.size(); ++i) {
    delete nodes[i];
  }
}

class DictionaryLoaderThread : public Thread {
 public:
  virtual void Run() {
    SuppressionDictionary *dic = Singleton<SuppressionDictionary>::get();
    CHECK(dic);
    dic->Lock();
    dic->Clear();
    for (int i = 0; i < 100; ++i) {
      const string key = "key" + Util::SimpleItoa(i);
      const string value = "value" + Util::SimpleItoa(i);
      EXPECT_TRUE(dic->AddEntry(key, value));
      Util::Sleep(5);
    }
    dic->UnLock();
  }
};

TEST(SupressionDictionary, ThreadTest) {
  SuppressionDictionary *dic = Singleton<SuppressionDictionary>::get();
  CHECK(dic);

  dic->Lock();

  for (int iter = 0; iter < 3; ++iter)  {
    DictionaryLoaderThread thread;

    // Load dictionary in another thread.
    thread.Start();

    for (int i = 0; i < 100; ++i) {
      const string key = "key" + Util::SimpleItoa(i);
      const string value = "value" + Util::SimpleItoa(i);
      if (!thread.IsRunning()) {
        EXPECT_TRUE(dic->SuppressEntry(key, value));
      }
    }

    thread.Join();
  }

  dic->UnLock();
}

}  // namespace
}  // namespace mozc

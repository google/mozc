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

// Unit tests for UserDictionary.

#include "dictionary/user_dictionary.h"

#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/singleton.h"
#include "base/trie.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "data_manager/user_dictionary_manager.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "dictionary/user_pos.h"
#include "dictionary/user_pos_interface.h"
#include "storage/registry.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.pb.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

const char kUserDictionary0[] =
    "start\tstart\tverb\n"
    "star\tstar\tnoun\n"
    "starting\tstarting\tnoun\n"
    "stamp\tstamp\tnoun\n"
    "stand\tstand\tverb\n"
    "smile\tsmile\tverb\n"
    "smog\tsmog\tnoun\n"
    // invalid characters in reading
    "水雲""\tvalue\tnoun\n"
    // Empty key
    "\tvalue\tnoun\n"
    // Empty value
    "start\t\tnoun\n"
    // Invalid POS
    "star\tvalue\tpos\n"
    // Empty POS
    "star\tvalue\t\n"
    // Duplicate entry
    "start\tstart\tverb\n";

const char kUserDictionary1[] = "end\tend\tverb\n";

void PushBackToken(const string &key,
                   const string &value,
                   uint16 id,
                   vector<UserPOS::Token> *tokens) {
  tokens->resize(tokens->size() + 1);
  UserPOS::Token *t = &tokens->back();
  t->key = key;
  t->value = value;
  t->id = id;
  t->cost = 0;
}

// This class is a mock class for writing unit tests of a class that
// depends on POS. It accepts only two values for part-of-speech:
// "noun" as words without inflection and "verb" as words with
// inflection.
class UserPOSMock : public UserPOSInterface {
 public:
  UserPOSMock() {}
  virtual ~UserPOSMock() {}

  // This method returns true if the given pos is "noun" or "verb".
  virtual bool IsValidPOS(const string &pos) const {
    return true;
  }

  // Given a verb, this method expands it to three different forms,
  // i.e. base form (the word itself), "-ed" form and "-ing" form. For
  // example, if the given word is "play", the method returns "play",
  // "played" and "playing". When a noun is passed, it returns only
  // base form. The method set lid and rid of the word as following:
  //
  //  POS              | lid | rid
  // ------------------+-----+-----
  //  noun             | 100 | 100
  //  verb (base form) | 200 | 200
  //  verb (-ed form)  | 210 | 210
  //  verb (-ing form) | 220 | 220
  virtual bool GetTokens(const string &key,
                         const string &value,
                         const string &pos,
                         vector<UserPOS::Token> *tokens) const {
    if (key.empty() ||
        value.empty() ||
        pos.empty() ||
        tokens == NULL) {
      return false;
    }

    tokens->clear();
    if (pos == "noun") {
      PushBackToken(key, value, 100, tokens);
      return true;
    } else if (pos == "verb") {
      PushBackToken(key, value, 200, tokens);
      PushBackToken(key + "ed", value + "ed", 210, tokens);
      PushBackToken(key + "ing", value + "ing", 220, tokens);
      return true;
    } else {
      return false;
    }
  }

  virtual void GetPOSList(vector<string> *pos_list) const {}

  virtual bool GetPOSIDs(const string &pos, uint16 *id) const {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UserPOSMock);
};

string GenRandomAlphabet(int size) {
  string result;
  const size_t len = Util::Random(size) + 1;
  for (int i = 0; i < len; ++i) {
    const uint16 l = Util::Random(static_cast<int>('z' - 'a')) + 'a';
    Util::UCS2ToUTF8Append(l, &result);
  }
  return result;
}

class UserDictionaryTest : public testing::Test {
 protected:
  virtual void SetUp() {
    pos_mock_.reset(new UserPOSMock);
    CHECK(pos_mock_.get() != NULL);
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    CHECK(storage::Registry::Clear());
    suppression_dictionary_.reset(new SuppressionDictionary);
  }

  virtual void TearDown() {
    pos_mock_.reset();
    CHECK(storage::Registry::Clear());
  }

  // Workaround for the constructor of UserDictionary being protected.
  // Creates a user dictionary with mock pos data.
  UserDictionary *CreateDictionaryWithMockPos() {
    // TODO(noriyukit): Prepare mock for POSMatcher that is consistent with
    // UserPOSMock.
    return new UserDictionary(
        pos_mock_.get(),
        UserDictionaryManager::GetUserDictionaryManager()->GetPOSMatcher(),
        suppression_dictionary_.get());
  }

  // Creates a user dictionary with actual pos data.
  UserDictionary *CreateDictionary() {
    const UserPOSInterface *user_pos =
        UserDictionaryManager::GetUserDictionaryManager()->GetUserPOS();
    return new UserDictionary(
        user_pos,
        UserDictionaryManager::GetUserDictionaryManager()->GetPOSMatcher(),
        Singleton<SuppressionDictionary>::get());
  }

  // Returns a sigleton of a user dictionary.
  UserDictionary *GetUserDictionarySingleton() {
    UserDictionaryManager *manager =
        UserDictionaryManager::GetUserDictionaryManager();
    return manager->GetUserDictionary();
  }

  struct Entry {
    string key;
    string value;
    uint16 lid;
    uint16 rid;
  };

  static void TestLookupPredictiveHelper(const Entry *expected,
                                         size_t expected_size,
                                         const char *key,
                                         size_t key_size,
                                         const UserDictionary &dic) {
    NodeAllocator allocator;
    Node *node = dic.LookupPredictive(key, key_size, &allocator);

    if (expected == NULL || expected_size == 0) {
      EXPECT_TRUE(NULL == node);
    } else {
      ASSERT_TRUE(NULL != node);
      CompareEntries(expected, expected_size, node);
    }
  }

  static void TestLookupPrefixHelper(const Entry *expected,
                                     size_t expected_size,
                                     const char *key,
                                     size_t key_size,
                                     const UserDictionary &dic) {
    NodeAllocator allocator;
    Node *node = dic.LookupPrefix(key, key_size, &allocator);

    if (expected == NULL || expected_size == 0) {
      EXPECT_TRUE(NULL == node);
    } else {
      ASSERT_TRUE(NULL != node);
      CompareEntries(expected, expected_size, node);
    }
  }

  static void CompareEntries(const Entry *expected, size_t expected_size,
                             const Node *node) {
    vector<string> expected_encode_items;
    for (size_t i = 0; i < expected_size; ++i) {
      const Entry &entry = expected[i];
      expected_encode_items.push_back(entry.key + "\t" +
                                      entry.value + "\t" +
                                      Util::SimpleItoa(entry.lid) + "\t" +
                                      Util::SimpleItoa(entry.rid) + "\n");
    }
    sort(expected_encode_items.begin(), expected_encode_items.end());
    string expected_encode;
    Util::JoinStrings(expected_encode_items, "", &expected_encode);

    vector<string> actual_encode_items;
    for ( ; node != NULL; node = node->bnext) {
      actual_encode_items.push_back(node->key + "\t" +
                                    node->value + "\t" +
                                    Util::SimpleItoa(node->lid) + "\t" +
                                    Util::SimpleItoa(node->rid) + "\n");
    }
    sort(actual_encode_items.begin(), actual_encode_items.end());
    string actual_encode;
    Util::JoinStrings(actual_encode_items, "", &actual_encode);

    EXPECT_EQ(expected_encode, actual_encode);
  }

  static void LoadFromString(const string &contents,
                             UserDictionaryStorage *storage) {
    istringstream is(contents);
    CHECK(is);

    storage->Clear();
    UserDictionaryStorage::UserDictionary *dic
        = storage->add_dictionaries();
    CHECK(dic);

    string line;
    while (getline(is, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      vector<string> fields;
      Util::SplitStringAllowEmpty(line, "\t", &fields);
      EXPECT_GE(fields.size(), 3) << line;
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      CHECK(entry);
      entry->set_key(fields[0]);
      entry->set_value(fields[1]);
      entry->set_pos(fields[2]);
    }
  }

  scoped_ptr<const UserPOSMock> pos_mock_;
  scoped_ptr<SuppressionDictionary> suppression_dictionary_;
};

TEST_F(UserDictionaryTest, TestLookupPredictive) {
  scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    UserDictionaryTest::LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage);
  }

  // A normal lookup operation.
  const Entry kExpected0[] = {
    { "start", "start", 200, 200 },
    { "started", "started", 210, 210 },
    { "starting", "starting", 100, 100 },
    { "starting", "starting", 220, 220 },
  };
  TestLookupPredictiveHelper(kExpected0, arraysize(kExpected0),
                             "start", 5, *dic.get());

  // Another normal lookup operation.
  const Entry kExpected1[] = {
    { "stamp", "stamp", 100, 100 },
    { "stand", "stand", 200, 200 },
    { "standed", "standed", 210, 210 },
    { "standing", "standing", 220, 220 },
    { "star", "star", 100, 100 },
    { "start", "start", 200, 200 },
    { "started", "started", 210, 210 },
    { "starting", "starting", 100, 100 },
    { "starting", "starting", 220, 220 },
  };
  TestLookupPredictiveHelper(kExpected1, arraysize(kExpected1),
                             "st", 2, *dic.get());

  // Invalid input values should be just ignored.
  TestLookupPredictiveHelper(NULL, 0, "", 0, *dic.get());
  TestLookupPredictiveHelper(NULL, 0, "\xE6\xB0\xB4\xE9\x9B\xB2",  // "水雲"
                             strlen("\xE6\xB0\xB4\xE9\x9B\xB2"), *dic.get());


  // Make a change to the dictionary file and load it again.
  {
    UserDictionaryStorage storage("");
    LoadFromString(kUserDictionary1, &storage);
    dic->Load(storage);
  }

  // A normal lookup.
  const Entry kExpected2[] = {
    { "end", "end", 200, 200 },
    { "ended", "ended", 210, 210 },
    { "ending", "ending", 220, 220 },
  };
  TestLookupPredictiveHelper(kExpected2, arraysize(kExpected2),
                         "end", 3, *dic.get());

  // Lookup for entries which are gone should returns empty result.
  TestLookupPredictiveHelper(NULL, 0, "start", 5, *dic.get());
  TestLookupPredictiveHelper(NULL, 0, "st", 2, *dic.get());
}

TEST_F(UserDictionaryTest, TestLookupPredictiveWithLimit) {
  scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    UserDictionaryTest::LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage);
  }

  {
    NodeAllocator allocator;
    DictionaryInterface::Limit limit;
    Trie<string> trie;
    trie.AddEntry("m", "");
    trie.AddEntry("n", "");
    limit.begin_with_trie = &trie;

    const string key = "sta";
    Node *node = dic->LookupPredictiveWithLimit(
        key.data(), key.size(), limit, &allocator);
    const Entry kExpected[] = {
      { "stamp", "stamp", 100, 100 },
      { "stand", "stand", 200, 200 },
      { "standed", "standed", 210, 210 },
      { "standing", "standing", 220, 220 },
    };
    CompareEntries(kExpected, arraysize(kExpected), node);
  }

  {
    NodeAllocator allocator;
    DictionaryInterface::Limit limit;
    const string key = "stan";
    Node *node = dic->LookupPredictiveWithLimit(
        key.data(), key.size(), limit, &allocator);
    const Entry kExpected[] = {
      { "stand", "stand", 200, 200 },
      { "standed", "standed", 210, 210 },
      { "standing", "standing", 220, 220 },
    };
    CompareEntries(kExpected, arraysize(kExpected), node);
  }
}

TEST_F(UserDictionaryTest, TestLookupPrefix) {
  scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage);
  }

  // A normal lookup operation.
  const Entry kExpected0[] = {
    { "star", "star", 100, 100 },
    { "start", "start", 200, 200 },
    { "started", "started", 210, 210 },
  };
  TestLookupPrefixHelper(kExpected0, arraysize(kExpected0),
                         "started", 7, *dic.get());

  // Another normal lookup operation.
  const Entry kExpected1[] = {
    { "star", "star", 100, 100 },
    { "start", "start", 200, 200 },
    { "starting", "starting", 100, 100 },
    { "starting", "starting", 220, 220 },
  };
  TestLookupPrefixHelper(kExpected1, arraysize(kExpected1),
                         "starting", 8, *dic.get());

  // Invalid input values should be just ignored.
  TestLookupPrefixHelper(NULL, 0, "", 0, *dic.get());
  TestLookupPrefixHelper(NULL, 0, "\xE6\xB0\xB4\xE9\x9B\xB2",  // "水雲"
                         strlen("\xE6\xB0\xB4\xE9\x9B\xB2"), *dic.get());

  // Make a change to the dictionary file and load it again.
  {
    UserDictionaryStorage storage("");
    LoadFromString(kUserDictionary1, &storage);
    dic->Load(storage);
  }

  // A normal lookup.
  const Entry kExpected2[] = {
    { "end", "end", 200, 200 },
    { "ending", "ending", 220, 220 },
  };
  TestLookupPrefixHelper(kExpected2, arraysize(kExpected2),
                         "ending", 6, *dic.get());

  // Lookup for entries which are gone should returns empty result.
  TestLookupPrefixHelper(NULL, 0, "started", 7, *dic.get());
  TestLookupPrefixHelper(NULL, 0, "starting", 8, *dic.get());
}

TEST_F(UserDictionaryTest, IncognitoModeTest) {
  config::Config config;
  config::ConfigHandler::GetConfig(&config);
  config.set_incognito_mode(true);
  config::ConfigHandler::SetConfig(config);

  scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    UserDictionaryTest::LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage);
  }

  NodeAllocator allocator;

  EXPECT_EQ(NULL, dic->LookupPrefix("start", 4, &allocator));
  EXPECT_EQ(NULL, dic->LookupPredictive("s", 1, &allocator));

  config.set_incognito_mode(false);
  config::ConfigHandler::SetConfig(config);

  EXPECT_FALSE(NULL == dic->LookupPrefix("start", 4, &allocator));
  EXPECT_FALSE(NULL == dic->LookupPredictive("s", 1, &allocator));
}

TEST_F(UserDictionaryTest, AsyncLoadTest) {
  const string filename = Util::JoinPath(FLAGS_test_tmpdir,
                                         "async_load_test.db");
  Util::Unlink(filename);

  // Create dictionary
  vector<string> keys;
  {
    UserDictionaryStorage storage(filename);

    EXPECT_FALSE(storage.Load());
    EXPECT_TRUE(storage.Lock());

    uint64 id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.mutable_dictionaries(0);
    for (size_t j = 0; j < 10000; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      entry->set_key(GenRandomAlphabet(10));
      entry->set_value(GenRandomAlphabet(10));
      entry->set_pos(GenRandomAlphabet(10));
      entry->set_comment(GenRandomAlphabet(10));
      keys.push_back(entry->key());
    }
    EXPECT_TRUE(storage.Save());
    EXPECT_TRUE(storage.UnLock());
  }

  {
    UserDictionary *dic = GetUserDictionarySingleton();
    // Wait for async reload called from the constructor.
    dic->WaitForReloader();
    dic->SetUserDictionaryName(filename);

    NodeAllocator allocator;
    for (int i = 0; i < 32; ++i) {
      random_shuffle(keys.begin(), keys.end());
      dic->Reload();
      for (int i = 0; i < 1000; ++i) {
        dic->LookupPrefix(keys[i].c_str(),
                          keys[i].size(), &allocator);
      }
    }
    dic->WaitForReloader();
  }
  Util::Unlink(filename);
}

TEST_F(UserDictionaryTest, AddToAutoRegisteredDictionary) {
  const string filename = Util::JoinPath(FLAGS_test_tmpdir,
                                         "add_to_auto_registered.db");
  Util::Unlink(filename);

  // Create dictionary
  {
    UserDictionaryStorage storage(filename);
    EXPECT_FALSE(storage.Load());
    EXPECT_TRUE(storage.Lock());
    EXPECT_TRUE(storage.Save());
    EXPECT_TRUE(storage.UnLock());
  }

  // Add entries.
  {
    scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
    dic->WaitForReloader();
    dic->SetUserDictionaryName(filename);
    for (int i = 0; i < 100; ++i) {
      EXPECT_TRUE(dic->AddToAutoRegisteredDictionary(
                      "key" + Util::SimpleItoa(i),
                      "value" + Util::SimpleItoa(i),
                      "noun"));
      dic->WaitForReloader();
    }
  }

  // Verify the contents.
  {
    UserDictionaryStorage storage(filename);
    EXPECT_TRUE(storage.Load());
#ifdef ENABLE_CLOUD_SYNC
    int index = 1;
    EXPECT_EQ(2, storage.dictionaries_size());
#else
    int index = 0;
    EXPECT_EQ(1, storage.dictionaries_size());
#endif
    EXPECT_EQ(100, storage.dictionaries(index).entries_size());
    for (int i = 0; i < 100; ++i) {
      EXPECT_EQ("key" + Util::SimpleItoa(i),
                storage.dictionaries(index).entries(i).key());
      EXPECT_EQ("value" + Util::SimpleItoa(i),
                storage.dictionaries(index).entries(i).value());
      EXPECT_EQ("noun",
                storage.dictionaries(index).entries(i).pos());
    }
  }

  Util::Unlink(filename);

  // Create dictionary
  {
    UserDictionaryStorage storage(filename);
    EXPECT_FALSE(storage.Load());
    EXPECT_TRUE(storage.Lock());
    EXPECT_TRUE(storage.Save());
    EXPECT_TRUE(storage.UnLock());
  }

  // Add same entries.
  {
    scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
    dic->WaitForReloader();
    dic->SetUserDictionaryName(filename);
    EXPECT_TRUE(dic->AddToAutoRegisteredDictionary("key", "value", "noun"));
    dic->WaitForReloader();
    // Duplicated one is not registered.
    EXPECT_FALSE(dic->AddToAutoRegisteredDictionary("key", "value", "noun"));
    dic->WaitForReloader();
  }

  // Verify the contents.
  {
    UserDictionaryStorage storage(filename);
    EXPECT_TRUE(storage.Load());
#ifdef ENABLE_CLOUD_SYNC
    EXPECT_EQ(2, storage.dictionaries_size());
    EXPECT_EQ(1, storage.dictionaries(1).entries_size());
    EXPECT_EQ("key", storage.dictionaries(1).entries(0).key());
    EXPECT_EQ("value", storage.dictionaries(1).entries(0).value());
    EXPECT_EQ("noun", storage.dictionaries(1).entries(0).pos());
#else
    EXPECT_EQ(1, storage.dictionaries_size());
    EXPECT_EQ(1, storage.dictionaries(0).entries_size());
    EXPECT_EQ("key", storage.dictionaries(0).entries(0).key());
    EXPECT_EQ("value", storage.dictionaries(0).entries(0).value());
    EXPECT_EQ("noun", storage.dictionaries(0).entries(0).pos());
#endif
  }
}

TEST_F(UserDictionaryTest, TestSuppressionDictionary) {
  scoped_ptr<UserDictionary> user_dic(CreateDictionaryWithMockPos());
  user_dic->WaitForReloader();

  const string filename = Util::JoinPath(FLAGS_test_tmpdir,
                                         "suppression_test.db");
  Util::Unlink(filename);

  UserDictionaryStorage storage(filename);

  // Create dictionary
  {
    uint64 id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.mutable_dictionaries(0);
    for (size_t j = 0; j < 10000; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      entry->set_key("no_suppress_key" + Util::SimpleItoa(j));
      entry->set_value("no_suppress_value" + Util::SimpleItoa(j));
      entry->set_pos("noun");
    }

    for (size_t j = 0; j < 10; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      entry->set_key("suppress_key" + Util::SimpleItoa(j));
      entry->set_value("suppress_value" + Util::SimpleItoa(j));
      // entry->set_pos("抑制単語");
      entry->set_pos("\xE6\x8A\x91\xE5\x88\xB6\xE5\x8D\x98\xE8\xAA\x9E");
    }

    suppression_dictionary_->Lock();
    EXPECT_TRUE(suppression_dictionary_->IsLocked());
    user_dic->Load(storage);
    EXPECT_FALSE(suppression_dictionary_->IsLocked());

    for (size_t j = 0; j < 10; ++j) {
      EXPECT_TRUE(suppression_dictionary_->SuppressEntry(
          "suppress_key" + Util::SimpleItoa(j),
          "suppress_value" + Util::SimpleItoa(j)));
    }
  }

  // Remove suppression entry
  {
    storage.Clear();
    uint64 id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.mutable_dictionaries(0);
    for (size_t j = 0; j < 10000; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      entry->set_key("no_suppress_key" + Util::SimpleItoa(j));
      entry->set_value("no_suppress_value" + Util::SimpleItoa(j));
      entry->set_pos("noun");
    }

    suppression_dictionary_->Lock();
    user_dic->Load(storage);
    EXPECT_FALSE(suppression_dictionary_->IsLocked());

    for (size_t j = 0; j < 10; ++j) {
      EXPECT_FALSE(suppression_dictionary_->SuppressEntry(
          "suppress_key" + Util::SimpleItoa(j),
          "suppress_value" + Util::SimpleItoa(j)));
    }
  }
  Util::Unlink(filename);
}

TEST_F(UserDictionaryTest, TestSuggestionOnlyWord) {
  scoped_ptr<UserDictionary> user_dic(CreateDictionary());
  user_dic->WaitForReloader();

  const string filename = Util::JoinPath(FLAGS_test_tmpdir,
                                         "suggestion_only_test.db");
  Util::Unlink(filename);

  UserDictionaryStorage storage(filename);

  // Create dictionary
  {
    uint64 id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.mutable_dictionaries(0);

    for (size_t j = 0; j < 10; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      entry->set_key("key" + Util::SimpleItoa(j));
      entry->set_value("default");
      // "名詞"
      entry->set_pos("\xE5\x90\x8D\xE8\xA9\x9E");
    }

    for (size_t j = 0; j < 10; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      entry->set_key("key" + Util::SimpleItoa(j));
      entry->set_value("suggest_only");
      // "サジェストのみ"
      entry->set_pos("\xE3\x82\xB5\xE3\x82\xB8\xE3\x82\xA7"
                     "\xE3\x82\xB9\xE3\x83\x88\xE3\x81\xAE\xE3\x81\xBF");
    }

    user_dic->Load(storage);
  }

  NodeAllocator allocator;

  {
    const char kKey[] = "key0123";
    const Node *node = user_dic->LookupPrefix(kKey, strlen(kKey),
                                              &allocator);
    CHECK(node);
    for (; node != NULL; node = node->bnext) {
      EXPECT_TRUE("suggest_only" != node->value && "default" == node->value);
    }
  }

  {
    const char kKey[] = "key";
    const Node *node = user_dic->LookupPredictive(kKey, strlen(kKey),
                                                  &allocator);
    CHECK(node);
    for (; node != NULL; node = node->bnext) {
      EXPECT_TRUE("suggest_only" == node->value || "default" == node->value);
    }
  }

  Util::Unlink(filename);
}

TEST_F(UserDictionaryTest, TestUsageStats) {
  scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();
  UserDictionaryStorage storage("");

  {
    UserDictionaryStorage::UserDictionary *dic1 = storage.add_dictionaries();
    CHECK(dic1);
    UserDictionaryStorage::UserDictionaryEntry *entry;
    entry = dic1->add_entries();
    CHECK(entry);
    entry->set_key("key1");
    entry->set_value("value1");
    entry->set_pos("noun");
    entry = dic1->add_entries();
    CHECK(entry);
    entry->set_key("key2");
    entry->set_value("value2");
    entry->set_pos("noun");
  }
  {
    UserDictionaryStorage::UserDictionary *dic2 = storage.add_dictionaries();
    dic2->set_syncable(true);
    CHECK(dic2);
    UserDictionaryStorage::UserDictionaryEntry *entry;
    entry = dic2->add_entries();
    CHECK(entry);
    entry->set_key("key3");
    entry->set_value("value3");
    entry->set_pos("noun");
    entry = dic2->add_entries();
    CHECK(entry);
    entry->set_key("key4");
    entry->set_value("value4");
    entry->set_pos("noun");
    entry = dic2->add_entries();
    CHECK(entry);
    entry->set_key("key5");
    entry->set_value("value5");
    entry->set_pos("noun");
  }
  dic->Load(storage);

  string reg_str;
  usage_stats::Stats stats;

  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.UserRegisteredWord",
                                         &reg_str));
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("UserRegisteredWord", stats.name());
  EXPECT_EQ(usage_stats::Stats::INTEGER, stats.type());
  EXPECT_EQ(5, stats.int_value());

  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.UserRegisteredSyncWord",
                                         &reg_str));
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("UserRegisteredSyncWord", stats.name());
  EXPECT_EQ(usage_stats::Stats::INTEGER, stats.type());
  EXPECT_EQ(3, stats.int_value());
}
}  // namespace
}  // namespace mozc

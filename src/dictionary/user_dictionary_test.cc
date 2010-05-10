// Copyright 2010, Google Inc.
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
#include "base/util.h"
#include "converter/converter_data.h"
#include "converter/pos_mock.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

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

int Random(int size) {
  return static_cast<int> (1.0 * size * rand() / (RAND_MAX + 1.0));
}

string GenRandomAlphabet(int size) {
  string result;
  const size_t len = Random(size) + 1;
  for (int i = 0; i < len; ++i) {
    const uint16 l = Random(static_cast<int>('z' - 'a')) + 'a';
    Util::UCS2ToUTF8Append(l, &result);
  }
  return result;
}
}  // namespace

class UserDictionaryTest : public testing::Test {
 protected:
  static void SetUpTestCase() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    POS::SetHandler(new POSMockHandler);
  }

  // Workaround for the constructor of UserDictionary being protected.
  UserDictionary *CreateDictionary() {
    return new UserDictionary;
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
    ConverterData data;
    Node *node = dic.LookupPredictive(key, key_size, &data);

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
    ConverterData data;
    Node *node = dic.LookupPrefix(key, key_size, &data);

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
};

TEST_F(UserDictionaryTest, TestLookupPredictive) {
  scoped_ptr<UserDictionary> dic(CreateDictionary());
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

TEST_F(UserDictionaryTest, TestLookupPrefix) {
  scoped_ptr<UserDictionary> dic(CreateDictionary());
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

TEST_F(UserDictionaryTest, AsyncLoadTest) {
  const string filename = Util::JoinPath(FLAGS_test_tmpdir, "test.db");
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
  }

  {
    UserDictionary dic;
    // Wait for async reload called from the constructor.
    dic.WaitForReloader();
    dic.SetUserDictionaryName(filename);

    ConverterData data;
    for (int i = 0; i < 32; ++i) {
      random_shuffle(keys.begin(), keys.end());
      dic.AsyncReload();
      for (int i = 0; i < 1000; ++i) {
        dic.LookupPrefix(keys[i].c_str(),
                         keys[i].size(), &data);
      }
    }
  }
}
}  // namespace mozc

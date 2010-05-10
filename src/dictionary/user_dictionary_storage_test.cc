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

#include "dictionary/user_dictionary_storage.h"

#ifndef OS_WINDOWS
#include <sys/stat.h>
#endif

#include <algorithm>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "dictionary/user_dictionary_importer.h"
#include "dictionary/user_dictionary_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

int Random(int size) {
  return static_cast<int> (1.0 * size * rand() / (RAND_MAX + 1.0));
}

string GenRandomString(int size) {
  string result;
  const size_t len = Random(size) + 1;
  for (int i = 0; i < len; ++i) {
    const uint16 l = Random(static_cast<int>('~' - ' ')) + ' ';
    Util::UCS2ToUTF8Append(l, &result);
  }
  return result;
}

string GetUserDictionaryFile() {
#ifndef OS_WINDOWS
  chmod(FLAGS_test_tmpdir.c_str(), 0777);
#endif
  return Util::JoinPath(FLAGS_test_tmpdir, "test.db");
}
}   // namespace

TEST(UserDictionaryStorage, FileTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  Util::Unlink(GetUserDictionaryFile());
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_EQ(storage.filename(), GetUserDictionaryFile());
  EXPECT_FALSE(storage.Exists());
}

TEST(UserDictionaryStorage, LockTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  Util::Unlink(GetUserDictionaryFile());
  UserDictionaryStorage storage1(GetUserDictionaryFile());
  UserDictionaryStorage storage2(GetUserDictionaryFile());
  EXPECT_TRUE(storage1.Lock());
  EXPECT_FALSE(storage2.Lock());
  EXPECT_FALSE(storage2.Save());
  EXPECT_TRUE(storage1.UnLock());
  EXPECT_TRUE(storage2.Lock());
  EXPECT_TRUE(storage2.Save());
}

TEST(UserDictionaryStorage, BasicOperationsTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  Util::Unlink(GetUserDictionaryFile());

  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load());

  const size_t kDictionariesSize = 3;
  uint64 id[kDictionariesSize];

  for (size_t i = 0; i < kDictionariesSize; ++i) {
    EXPECT_TRUE(storage.CreateDictionary("test" + Util::SimpleItoa(i), &id[i]));
    EXPECT_EQ(i + 1, storage.dictionaries_size());
  }

  for (size_t i = 0; i < kDictionariesSize; ++i) {
    EXPECT_EQ(i, storage.GetUserDictionaryIndex(id[i]));
    EXPECT_EQ(-1, storage.GetUserDictionaryIndex(id[i] + 1));
  }

  for (size_t i = 0; i < kDictionariesSize; ++i) {
    EXPECT_EQ(storage.mutable_dictionaries(i),
              storage.GetUserDictionary(id[i]));
    EXPECT_EQ(NULL, storage.GetUserDictionary(id[i] + 1));
  }

  // empty
  EXPECT_FALSE(storage.RenameDictionary(id[0], ""));

  // invalid id
  EXPECT_FALSE(storage.RenameDictionary(0, ""));

  EXPECT_TRUE(storage.RenameDictionary(id[0], "renamed0"));
  EXPECT_EQ("renamed0", storage.dictionaries(0).name());

  // invalid id
  EXPECT_FALSE(storage.DeleteDictionary(0));

  EXPECT_TRUE(storage.DeleteDictionary(id[1]));
  EXPECT_EQ(2, storage.dictionaries_size());
}

TEST(UserDictionaryStorage, DeleteTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  Util::Unlink(GetUserDictionaryFile());

  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load());

  // repeat 10 times
  for (int i = 0; i < 10; ++i) {
    storage.Clear();
    vector<uint64> ids(100);
    for (size_t i = 0; i < ids.size(); ++i) {
      EXPECT_TRUE(storage.CreateDictionary("test", &ids[i]));
    }

    vector<uint64> alive;
    for (size_t i = 0; i < ids.size(); ++i) {
      if (Random(3) == 0) {   // 30%
        EXPECT_TRUE(storage.DeleteDictionary(ids[i]));
        continue;
      }
      alive.push_back(ids[i]);
    }

    EXPECT_EQ(alive.size(), storage.dictionaries_size());

    for (size_t i = 0; i < alive.size(); ++i) {
      EXPECT_EQ(alive[i], storage.dictionaries(i).id());
    }
  }
}

TEST(UserDictionaryStorage, ExportTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  UserDictionaryStorage storage(GetUserDictionaryFile());
  uint64 id = 0;

  EXPECT_TRUE(storage.CreateDictionary("test", &id));

  UserDictionaryStorage::UserDictionary *dic =
      storage.GetUserDictionary(id);

  for (size_t i = 0; i < 1000; ++i) {
    UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
    const string prefix = Util::SimpleItoa(i);
    // set empty fields randomly
    entry->set_key(prefix + "key");
    entry->set_value(prefix + "value");
    // "名詞"
    entry->set_pos("\xe5\x90\x8d\xe8\xa9\x9e");
    entry->set_comment(prefix + "comment");
  }

  const string export_file = Util::JoinPath(FLAGS_test_tmpdir,
                                            "export.txt");

  EXPECT_FALSE(storage.ExportDictionary(id + 1, export_file));
  EXPECT_TRUE(storage.ExportDictionary(id, export_file));

  UserDictionaryStorage::UserDictionary dic2;
  InputFileStream ifs(export_file.c_str());
  CHECK(ifs);
  UserDictionaryImporter::IStreamTextLineIterator iter(&ifs);

  EXPECT_EQ(UserDictionaryImporter::IMPORT_NO_ERROR,
            UserDictionaryImporter::ImportFromTextLineIterator(
                UserDictionaryImporter::MOZC,
                &iter, &dic2));

  dic2.set_id(id);
  dic2.set_name("test");

  EXPECT_EQ(dic2.DebugString(), dic->DebugString());
}

TEST(UserDictionaryStorage, SerializeTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  // repeat 20 times
  for (int i = 0; i < 20; ++i) {
    Util::Unlink(GetUserDictionaryFile());
    UserDictionaryStorage storage1(GetUserDictionaryFile());

    {
      EXPECT_FALSE(storage1.Load());
      const size_t dic_size = Random(50) + 1;

      for (size_t i = 0; i < dic_size; ++i) {
        uint64 id = 0;
        EXPECT_TRUE(storage1.CreateDictionary(GenRandomString(10),
                                             &id));
        const size_t entry_size = Random(100) + 1;
        for (size_t j = 0; j < entry_size; ++j) {
          UserDictionaryStorage::UserDictionary *dic =
              storage1.mutable_dictionaries(i);
          UserDictionaryStorage::UserDictionaryEntry *entry =
              dic->add_entries();
          entry->set_key(GenRandomString(10));
          entry->set_value(GenRandomString(10));
          entry->set_pos(GenRandomString(10));
          entry->set_comment(GenRandomString(10));
        }
      }

      EXPECT_TRUE(storage1.Lock());
      EXPECT_TRUE(storage1.Save());
      EXPECT_TRUE(storage1.UnLock());
    }

    UserDictionaryStorage storage2(GetUserDictionaryFile());
    {
      EXPECT_TRUE(storage2.Load());
    }

    EXPECT_EQ(storage1.DebugString(), storage2.DebugString());
  }
}
}  // namespace mozc

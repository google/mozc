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

#include "dictionary/user_dictionary_storage.h"

#ifndef OS_WINDOWS
#include <sys/stat.h>
#endif

#include <algorithm>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/number_util.h"
#include "base/protobuf/unknown_field_set.h"
#include "base/testing_util.h"
#include "base/util.h"
#include "dictionary/user_dictionary_importer.h"
#include "dictionary/user_dictionary_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

using mozc::protobuf::UnknownField;
using mozc::protobuf::UnknownFieldSet;
using mozc::testing::SerializeUnknownFieldSetAsString;
using user_dictionary::UserDictionary;

namespace {

string GenRandomString(int size) {
  string result;
  const size_t len = Util::Random(size) + 1;
  for (int i = 0; i < len; ++i) {
    const uint16 l =
        static_cast<uint16>(Util::Random(static_cast<int>('~' - ' ')) + ' ');
    Util::UCS2ToUTF8Append(l, &result);
  }
  return result;
}

}   // namespace

class UserDictionaryStorageTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    backup_user_profile_directory_ = Util::GetUserProfileDirectory();
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
#ifndef OS_WINDOWS
    // TODO(hidehiko): Do we really need this?
    chmod(FLAGS_test_tmpdir.c_str(), 0777);
#endif
    Util::Unlink(GetUserDictionaryFile());
  }

  virtual void TearDown() {
    Util::Unlink(GetUserDictionaryFile());
    Util::SetUserProfileDirectory(backup_user_profile_directory_);
  }

  static string GetUserDictionaryFile() {
    return Util::JoinPath(FLAGS_test_tmpdir, "test.db");
  }

 private:
  string backup_user_profile_directory_;
};

TEST_F(UserDictionaryStorageTest, FileTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_EQ(storage.filename(), GetUserDictionaryFile());
  EXPECT_FALSE(storage.Exists());
}

TEST_F(UserDictionaryStorageTest, LockTest) {
  UserDictionaryStorage storage1(GetUserDictionaryFile());
  UserDictionaryStorage storage2(GetUserDictionaryFile());
  EXPECT_TRUE(storage1.Lock());
  EXPECT_FALSE(storage2.Lock());
  EXPECT_FALSE(storage2.Save());
  EXPECT_TRUE(storage1.UnLock());
  EXPECT_TRUE(storage2.Lock());
  EXPECT_TRUE(storage2.Save());
}

TEST_F(UserDictionaryStorageTest, SyncDictionaryBinarySizeTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load());
#ifndef ENABLE_CLOUD_SYNC
  // In binaries built without sync feature, we should make sync dictionary
  // intentionally.
  storage.EnsureSyncDictionaryExists();
#endif  // ENABLE_CLOUD_SYNC

  EXPECT_TRUE(storage.Lock());

  EXPECT_TRUE(storage.Save());
  {
    // Creates an entry which exceeds the sync dictionary limit
    // outside of the sync dictionary.
    UserDictionaryStorage::UserDictionary *dic = storage.add_dictionaries();
    dic->set_name("foo");
    dic->set_syncable(false);
    UserDictionaryStorage::UserDictionaryEntry *entry =
        dic->add_entries();
    entry->set_key(string(UserDictionaryStorage::max_sync_binary_size(), 'a'));
    entry->set_value("value");
    entry->set_pos(UserDictionary::NOUN);
  }
  // Okay to store such entry in the normal dictionary.
  EXPECT_TRUE(storage.Save());

  {
    // Same on the sync dictionary.
    UserDictionaryStorage::UserDictionary *dic =
        storage.mutable_dictionaries(0);
    EXPECT_TRUE(dic->syncable());  // just in case.
    UserDictionaryStorage::UserDictionaryEntry *entry =
        dic->add_entries();
    entry->set_key(string(UserDictionaryStorage::max_sync_binary_size(), 'a'));
    entry->set_value("value");
    entry->set_pos(UserDictionary::NOUN);
  }
  // Such size is unacceptable for sync.
  EXPECT_FALSE(storage.Save());

  EXPECT_TRUE(storage.UnLock());
}

TEST_F(UserDictionaryStorageTest, SyncDictionaryEntrySizeTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load());
#ifndef ENABLE_CLOUD_SYNC
  // In binaries built without sync feature, we should make sync dictionary
  // intentionally.
  storage.EnsureSyncDictionaryExists();
#endif  // ENABLE_CLOUD_SYNC

  EXPECT_TRUE(storage.Lock());

  EXPECT_TRUE(storage.Save());
  {
    // Creates an entry which exceeds the sync dictionary limit
    // outside of the sync dictionary.
    UserDictionaryStorage::UserDictionary *dic = storage.add_dictionaries();
    dic->set_name("foo");
    dic->set_syncable(false);
    for (size_t i = 0; i < UserDictionaryStorage::max_sync_entry_size() + 1;
         ++i) {
      // add empty entries.
      dic->add_entries();
    }
  }
  // Okay to store such entry in the normal dictionary.
  EXPECT_TRUE(storage.Save());

  {
    // Same on the sync dictionary.
    UserDictionaryStorage::UserDictionary *dic =
        storage.mutable_dictionaries(0);
    EXPECT_TRUE(dic->syncable());  // just in case.
    for (size_t i = 0; i < UserDictionaryStorage::max_sync_entry_size() + 1;
         ++i) {
      // add empty entries.
      dic->add_entries();
    }
  }
  // Such entry size is unacceptable for sync.
  EXPECT_FALSE(storage.Save());

  EXPECT_TRUE(storage.UnLock());
}

TEST_F(UserDictionaryStorageTest, BasicOperationsTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load());

  const size_t kDictionariesSize = 3;
  uint64 id[kDictionariesSize];

  const size_t dict_size = storage.dictionaries_size();

  for (size_t i = 0; i < kDictionariesSize; ++i) {
    EXPECT_TRUE(storage.CreateDictionary("test" + NumberUtil::SimpleItoa(i),
                                         &id[i]));
    EXPECT_EQ(i + 1 + dict_size, storage.dictionaries_size());
  }

  for (size_t i = 0; i < kDictionariesSize; ++i) {
    EXPECT_EQ(i + dict_size, storage.GetUserDictionaryIndex(id[i]));
    EXPECT_EQ(-1, storage.GetUserDictionaryIndex(id[i] + 1));
  }

  for (size_t i = 0; i < kDictionariesSize; ++i) {
    EXPECT_EQ(storage.mutable_dictionaries(i + dict_size),
              storage.GetUserDictionary(id[i]));
    EXPECT_EQ(NULL, storage.GetUserDictionary(id[i] + 1));
  }

  // empty
  EXPECT_FALSE(storage.RenameDictionary(id[0], ""));

  // duplicated
  uint64 tmp_id = 0;
  EXPECT_FALSE(storage.CreateDictionary("test0", &tmp_id));
  EXPECT_EQ(UserDictionaryStorage::DUPLICATED_DICTIONARY_NAME,
            storage.GetLastError());

  // invalid id
  EXPECT_FALSE(storage.RenameDictionary(0, ""));

  // duplicated
  EXPECT_FALSE(storage.RenameDictionary(id[0], "test1"));
  EXPECT_EQ(UserDictionaryStorage::DUPLICATED_DICTIONARY_NAME,
            storage.GetLastError());

  // no name
  EXPECT_TRUE(storage.RenameDictionary(id[0], "test0"));

  EXPECT_TRUE(storage.RenameDictionary(id[0], "renamed0"));
  EXPECT_EQ("renamed0", storage.GetUserDictionary(id[0])->name());

  // invalid id
  EXPECT_FALSE(storage.DeleteDictionary(0));

  EXPECT_TRUE(storage.DeleteDictionary(id[1]));
  EXPECT_EQ(kDictionariesSize + dict_size - 1, storage.dictionaries_size());
}

TEST_F(UserDictionaryStorageTest, DeleteTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load());

  // repeat 10 times
  for (int i = 0; i < 10; ++i) {
    storage.Clear();
    vector<uint64> ids(100);
    for (size_t i = 0; i < ids.size(); ++i) {
      EXPECT_TRUE(storage.CreateDictionary("test" + NumberUtil::SimpleItoa(i),
                                           &ids[i]));
    }

    vector<uint64> alive;
    for (size_t i = 0; i < ids.size(); ++i) {
      if (Util::Random(3) == 0) {   // 33%
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

TEST_F(UserDictionaryStorageTest, ExportTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  uint64 id = 0;

  EXPECT_TRUE(storage.CreateDictionary("test", &id));

  UserDictionaryStorage::UserDictionary *dic =
      storage.GetUserDictionary(id);

  for (size_t i = 0; i < 1000; ++i) {
    UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
    const string prefix = NumberUtil::SimpleItoa(i);
    // set empty fields randomly
    entry->set_key(prefix + "key");
    entry->set_value(prefix + "value");
    // "名詞"
    entry->set_pos(UserDictionary::NOUN);
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

TEST_F(UserDictionaryStorageTest, SerializeTest) {
  // repeat 20 times
  for (int i = 0; i < 20; ++i) {
    Util::Unlink(GetUserDictionaryFile());
    UserDictionaryStorage storage1(GetUserDictionaryFile());

    {
      EXPECT_FALSE(storage1.Load());
      const size_t dic_size = Util::Random(50) + 1;

      for (size_t i = 0; i < dic_size; ++i) {
        uint64 id = 0;
        EXPECT_TRUE(
            storage1.CreateDictionary("test" + NumberUtil::SimpleItoa(i),
                                      &id));
        const size_t entry_size = Util::Random(100) + 1;
        for (size_t j = 0; j < entry_size; ++j) {
          UserDictionaryStorage::UserDictionary *dic =
              storage1.mutable_dictionaries(i);
          UserDictionaryStorage::UserDictionaryEntry *entry =
              dic->add_entries();
          entry->set_key(GenRandomString(10));
          entry->set_value(GenRandomString(10));
          entry->set_pos(UserDictionary::NOUN);
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

TEST_F(UserDictionaryStorageTest, GetUserDictionaryIdTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load());

  const size_t kDictionariesSize = 3;
  uint64 id[kDictionariesSize];
  EXPECT_TRUE(storage.CreateDictionary("testA", &id[0]));
  EXPECT_TRUE(storage.CreateDictionary("testB", &id[1]));

  uint64 ret_id[kDictionariesSize];
  EXPECT_TRUE(storage.GetUserDictionaryId("testA", &ret_id[0]));
  EXPECT_TRUE(storage.GetUserDictionaryId("testB", &ret_id[1]));
  EXPECT_FALSE(storage.GetUserDictionaryId("testC", &ret_id[2]));

  EXPECT_EQ(ret_id[0], id[0]);
  EXPECT_EQ(ret_id[1], id[1]);
}

// Following test is valid only when sync feature is enabled.
#ifdef ENABLE_CLOUD_SYNC
TEST_F(UserDictionaryStorageTest, SyncDictionaryTests) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load());

  // Always the storage has the sync dictionary.
  EXPECT_EQ(1, storage.dictionaries_size());
  const UserDictionaryStorage::UserDictionary &sync_dict =
      storage.dictionaries(0);
  uint64 sync_dict_id = sync_dict.id();
  EXPECT_NE(0, sync_dict_id);
  EXPECT_TRUE(sync_dict.syncable());

  uint64 normal_dict_id = 0;
  EXPECT_TRUE(storage.CreateDictionary("test", &normal_dict_id));
  EXPECT_NE(sync_dict_id, normal_dict_id);

  // Storing the file.
  EXPECT_TRUE(storage.Lock());
  EXPECT_TRUE(storage.Save());
  EXPECT_TRUE(storage.UnLock());

  // Clear and reload from the file again.
  storage.Clear();
  EXPECT_TRUE(storage.Load());
  // Regardless of the existence of the user dictionary file,
  // UserDictionaryStorage::Load() does not append a new sync dictionary if it
  // already contains at least one sync dictionary.
  EXPECT_EQ(2, storage.dictionaries_size());
  bool has_sync_dict = false;
  bool has_normal_dict = false;
  for (size_t i = 0; i < storage.dictionaries_size(); ++i) {
    const UserDictionaryStorage::UserDictionary &dict =
        storage.dictionaries(i);
    if (dict.syncable()) {
      // sync dictionary
      EXPECT_EQ(sync_dict_id, dict.id());
      has_sync_dict = true;
    } else {
      EXPECT_EQ(normal_dict_id, dict.id());
      has_normal_dict = true;
    }
  }
  EXPECT_TRUE(has_sync_dict);
  EXPECT_TRUE(has_normal_dict);
}
#endif  // ENABLE_CLOUD_SYNC

TEST_F(UserDictionaryStorageTest, SyncDictionaryOperations) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load());
#ifndef ENABLE_CLOUD_SYNC
  // In binaries built without sync feature, we should make sync dictionary
  // intentionally.
  storage.EnsureSyncDictionaryExists();
#endif  // ENABLE_CLOUD_SYNC

  uint64 sync_dic_id = 0;
  EXPECT_TRUE(storage.GetUserDictionaryId(
      UserDictionaryStorage::default_sync_dictionary_name(), &sync_dic_id));

  // Cannot delete.
  EXPECT_FALSE(storage.DeleteDictionary(sync_dic_id));
  // Cannot rename.
  EXPECT_FALSE(storage.RenameDictionary(sync_dic_id, "foo"));
  // Cannot copy.
  uint64 dummy_id = 0;
  EXPECT_FALSE(storage.CopyDictionary(sync_dic_id, "bar", &dummy_id));
  EXPECT_EQ(0, dummy_id);
}

TEST_F(UserDictionaryStorageTest, CreateDefaultSyncDictionaryIfNeccesary) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load());
#ifdef ENABLE_CLOUD_SYNC
  EXPECT_GT(UserDictionaryStorage::CountSyncableDictionaries(&storage), 0) <<
      "When ENABLE_CLOUD_SYNC is defined, at least one sync dictionary should "
      "exist after |storage.Load() is finished. Currently this is by design.";
#else
  EXPECT_EQ(0, UserDictionaryStorage::CountSyncableDictionaries(&storage)) <<
      "When ENABLE_CLOUD_SYNC is not defined, there should be no sync "
      "dictionary by default.";
#endif  // ENABLE_CLOUD_SYNC
}

#ifndef ENABLE_CLOUD_SYNC
TEST_F(UserDictionaryStorageTest, Issue6004671) {
  { // Emulate the situation in b/6004671.
    UserDictionaryStorage storage(GetUserDictionaryFile());
    EXPECT_FALSE(storage.Load());
    ASSERT_TRUE(storage.EnsureSyncDictionaryExists());
    ASSERT_GT(UserDictionaryStorage::CountSyncableDictionaries(&storage), 0);
    ASSERT_TRUE(storage.Lock());
    ASSERT_TRUE(storage.Save());
    ASSERT_TRUE(storage.UnLock());
  }

  {
    UserDictionaryStorage storage(GetUserDictionaryFile());
    ASSERT_TRUE(storage.Load());
    EXPECT_EQ(0, UserDictionaryStorage::CountSyncableDictionaries(&storage))
        << "To work around Issue 6004671, we silently remove an empty sync "
           "dictionary in UserDictionaryStorage::Load() when "
           "ENABLE_CLOUD_SYNC is not defined.";
  }
}
#endif  // ENABLE_CLOUD_SYNC

TEST_F(UserDictionaryStorageTest, RemoveUnusedSyncDictionariesIfExist) {
  // "名詞"
  static const UserDictionary::PosType kPos = UserDictionary::NOUN;

  {
    UserDictionaryStorage storage(GetUserDictionaryFile());
    EXPECT_FALSE(storage.LoadWithoutChangingSyncDictionary()) <<
        "At first, we expect there is not user dictionary file.";

    { // Add an non-empty non-sync dictionary.
      uint64 dict_id = 0;
      ASSERT_TRUE(storage.CreateDictionary(
          "Non-empty non-sync Dictionary", &dict_id));
      UserDictionaryStorage::UserDictionary *dict =
          storage.mutable_dictionaries(
              storage.GetUserDictionaryIndex(dict_id));
      dict->set_syncable(false);
      UserDictionaryStorage::UserDictionaryEntry *entry = dict->add_entries();
      entry->set_key("A");
      entry->set_value("AA");
      entry->set_pos(kPos);
    }

    { // Add an empty non-sync dictionary.
      uint64 dict_id = 0;
      ASSERT_TRUE(storage.CreateDictionary(
          "Empty non-sync Dictionary", &dict_id));
      UserDictionaryStorage::UserDictionary *dict =
          storage.mutable_dictionaries(
              storage.GetUserDictionaryIndex(dict_id));
      dict->set_syncable(false);
    }

    { // Add an non-empty sync dictionary.
      uint64 dict_id = 0;
      ASSERT_TRUE(storage.CreateDictionary(
          "Non-empty sync Dictionary", &dict_id));
      UserDictionaryStorage::UserDictionary *dict =
          storage.mutable_dictionaries(
              storage.GetUserDictionaryIndex(dict_id));
      dict->set_syncable(true);
      UserDictionaryStorage::UserDictionaryEntry *entry = dict->add_entries();
      entry->set_key("A");
      entry->set_value("AA");
      entry->set_pos(kPos);
    }

    { // Add an empty sync dictionary.
      uint64 dict_id = 0;
      ASSERT_TRUE(storage.CreateDictionary("Empty sync Dictionary", &dict_id));
      UserDictionaryStorage::UserDictionary *dict =
          storage.mutable_dictionaries(
              storage.GetUserDictionaryIndex(dict_id));
      dict->set_syncable(true);
    }

    EXPECT_GT(UserDictionaryStorage::CountSyncableDictionaries(&storage), 0);
    EXPECT_TRUE(storage.Lock());
    EXPECT_TRUE(storage.Save());
    EXPECT_TRUE(storage.UnLock());
  }

  ASSERT_TRUE(Util::FileExists(GetUserDictionaryFile()))
      << "A temporary user dictionary file shoudl exist.";

  // Remove unused sync dictionaries.
  {
    UserDictionaryStorage storage(GetUserDictionaryFile());
    ASSERT_TRUE(storage.LoadWithoutChangingSyncDictionary()) <<
        "Failed to load test user dictionary file.";
    EXPECT_EQ(2, UserDictionaryStorage::CountSyncableDictionaries(&storage));
    storage.RemoveUnusedSyncDictionariesIfExist();
    EXPECT_EQ(1, UserDictionaryStorage::CountSyncableDictionaries(&storage))
        << "One sync dictionary should be removed because if is empty.";
    EXPECT_TRUE(storage.Lock());
    EXPECT_TRUE(storage.Save());
    EXPECT_TRUE(storage.UnLock());
  }

  // Reload and test it again.
  {
    UserDictionaryStorage storage(GetUserDictionaryFile());
    ASSERT_TRUE(storage.LoadWithoutChangingSyncDictionary()) <<
        "Failed to load test user dictionary file.";
    EXPECT_EQ(1, UserDictionaryStorage::CountSyncableDictionaries(&storage));
    storage.RemoveUnusedSyncDictionariesIfExist();
    EXPECT_EQ(1, UserDictionaryStorage::CountSyncableDictionaries(&storage));
  }
}

TEST_F(UserDictionaryStorageTest, AddToAutoRegisteredDictionary) {
  {
    UserDictionaryStorage storage(GetUserDictionaryFile());
    EXPECT_EQ(0, storage.dictionaries_size());
    EXPECT_TRUE(storage.AddToAutoRegisteredDictionary(
        "key1", "value1", UserDictionary::NOUN));
    EXPECT_EQ(1, storage.dictionaries_size());
    EXPECT_EQ(1, storage.dictionaries(0).entries_size());
    const UserDictionaryStorage::UserDictionaryEntry &entry1 =
        storage.dictionaries(0).entries(0);
    EXPECT_EQ("key1", entry1.key());
    EXPECT_EQ("value1", entry1.value());
    EXPECT_EQ(UserDictionary::NOUN, entry1.pos());
    EXPECT_TRUE(entry1.auto_registered());

    EXPECT_TRUE(storage.AddToAutoRegisteredDictionary(
        "key2", "value2", UserDictionary::NOUN));
    EXPECT_EQ(1, storage.dictionaries_size());
    EXPECT_EQ(2, storage.dictionaries(0).entries_size());
    const UserDictionaryStorage::UserDictionaryEntry &entry2 =
        storage.dictionaries(0).entries(1);
    EXPECT_EQ("key2", entry2.key());
    EXPECT_EQ("value2", entry2.value());
    EXPECT_EQ(UserDictionary::NOUN, entry2.pos());
    EXPECT_TRUE(entry1.auto_registered());
  }

  {
    Util::Unlink(GetUserDictionaryFile());
    UserDictionaryStorage storage(GetUserDictionaryFile());
    storage.Lock();
    // Already locked.
    EXPECT_FALSE(storage.AddToAutoRegisteredDictionary(
        "key", "value", UserDictionary::NOUN));
  }
}

#ifndef OS_ANDROID
// This is a test to check if the data stored by the new binary can be read
// by an older binary, considering the Dev -> Stable converting users.
// We can remove when the new Stable binary which supports reading new format
// is spread enough.
TEST_F(UserDictionaryStorageTest, BackwardCompatibilityTest) {
  {
    UserDictionaryStorage storage(GetUserDictionaryFile());
    // Add dummy entry.
    {
      UserDictionary *dictionary = storage.add_dictionaries();
      UserDictionary::Entry *entry = dictionary->add_entries();
      entry->set_key("key");
      entry->set_value("value");
      entry->set_comment("comment");
      entry->set_pos(UserDictionary::NOUN);
    }

    ASSERT_TRUE(storage.Lock());
    ASSERT_TRUE(storage.Save());
    ASSERT_TRUE(storage.UnLock());
  }

  // Make sure that the deprecated field is filled (in string).
  UserDictionaryStorage storage(GetUserDictionaryFile());
  ASSERT_TRUE(storage.LoadWithoutMigration());

#ifndef ENABLE_CLOUD_SYNC
  // In binaries built without sync feature, we should make sync dictionary
  // intentionally.
  storage.EnsureSyncDictionaryExists();
#endif  // ENABLE_CLOUD_SYNC

  ASSERT_EQ(2, storage.dictionaries_size());
  const UserDictionary &dictionary = storage.dictionaries(0);
  ASSERT_EQ(1, dictionary.entries_size());
  const UserDictionary::Entry &entry = dictionary.entries(0);
  const UnknownFieldSet &unknown_field_set = entry.unknown_fields();
  ASSERT_EQ(1, unknown_field_set.field_count());
  const UnknownField &unknown_field = unknown_field_set.field(0);
  EXPECT_EQ(3, unknown_field.number());
  EXPECT_EQ(UnknownField::TYPE_LENGTH_DELIMITED, unknown_field.type());
  EXPECT_EQ("\xE5\x90\x8D\xE8\xA9\x9E", unknown_field.length_delimited());
}
#endif

TEST_F(UserDictionaryStorageTest, Export) {
  const int kDummyDictionaryId = 10;
  const string kPath = Util::JoinPath(FLAGS_test_tmpdir, "exported_file");

  {
    UserDictionaryStorage storage(GetUserDictionaryFile());
    {
      UserDictionary *dictionary = storage.add_dictionaries();
      dictionary->set_id(kDummyDictionaryId);
      UserDictionary::Entry *entry = dictionary->add_entries();
      entry->set_key("key");
      entry->set_value("value");
      entry->set_pos(UserDictionary::NOUN);
      entry->set_comment("comment");
    }
    storage.ExportDictionary(kDummyDictionaryId, kPath);
  }

  Mmap mapped_data;
  ASSERT_TRUE(mapped_data.Open(kPath.c_str()));

  // Make sure the exported format, especially that the pos is exported in
  // Japanese.
  // "key value 名詞 comment" separted by a tab character.
#ifdef OS_WINDOWS
  EXPECT_EQ("key\tvalue\t\xE5\x90\x8D\xE8\xA9\x9E\tcomment\r\n",
            string(mapped_data.begin(), mapped_data.size()));
#else
  EXPECT_EQ("key\tvalue\t\xE5\x90\x8D\xE8\xA9\x9E\tcomment\n",
            string(mapped_data.begin(), mapped_data.size()));
#endif  // OS_WINDOWS
}

namespace {

enum SerializedPosType {
  NEW_ENUM_POS,
  LEGACY_STRING_POS,
  LEGACY_ENUM_POS,
};

void AddTestDummyUserDictionary(
    SerializedPosType serialized_pos_type, UnknownFieldSet *storage) {
  UnknownFieldSet dictionary;
  for (int i = UserDictionary::PosType_MIN;
       i <= UserDictionary::PosType_MAX; ++i) {
    UnknownFieldSet entry;
    entry.AddLengthDelimited(1, Util::StringPrintf("key%d", i));
    entry.AddLengthDelimited(2, Util::StringPrintf("value%d", i));
    switch (serialized_pos_type) {
      case NEW_ENUM_POS:
        entry.AddVarint(5, i);
        break;
      case LEGACY_STRING_POS:
        entry.AddLengthDelimited(
            3,
            UserDictionaryUtil::GetStringPosType(
                static_cast<UserDictionary::PosType>(i)));
        break;
      case LEGACY_ENUM_POS:
        entry.AddVarint(3, i);
        break;
      default:
        LOG(FATAL) << "Unknown serialized pos type: " << serialized_pos_type;
    }
    entry.AddLengthDelimited(4, Util::StringPrintf("comment%d", i));
    dictionary.AddLengthDelimited(
        4, SerializeUnknownFieldSetAsString(entry));
  }
  storage->AddLengthDelimited(
      2, SerializeUnknownFieldSetAsString(dictionary));
}

void WriteToFile(const string &value, const string &filepath) {
  OutputFileStream stream(filepath.c_str(), ios::out|ios::binary|ios::trunc);
  stream.write(value.data(), value.size());
}

}  // namespace

class UserDictionaryStorageMigrationTest
    : public UserDictionaryStorageTest,
      public ::testing::WithParamInterface<SerializedPosType> {
};

TEST_P(UserDictionaryStorageMigrationTest, Load) {
  {
    // Create serialized user dictionary data directly.
    UnknownFieldSet storage;
    AddTestDummyUserDictionary(GetParam(), &storage);
    WriteToFile(SerializeUnknownFieldSetAsString(storage),
                GetUserDictionaryFile());
  }

  UserDictionaryStorage storage(GetUserDictionaryFile());
  ASSERT_TRUE(storage.Load());

#ifndef ENABLE_CLOUD_SYNC
  // In binaries built without sync feature, we should make sync dictionary
  // intentionally.
  storage.EnsureSyncDictionaryExists();
#endif  // ENABLE_CLOUD_SYNC

  ASSERT_EQ(2, storage.dictionaries_size()) << storage.Utf8DebugString();
  const UserDictionary &dictionary = storage.dictionaries(0);
  ASSERT_EQ(UserDictionary::PosType_MAX - UserDictionary::PosType_MIN + 1,
            dictionary.entries_size())
      << dictionary.Utf8DebugString();
  for (int i = 0; i < dictionary.entries_size(); ++i) {
    const int id = UserDictionary::PosType_MIN + i;
    const UserDictionary::Entry &entry = dictionary.entries(i);
    EXPECT_EQ(Util::StringPrintf("key%d", id), entry.key())
        << entry.Utf8DebugString();
    EXPECT_EQ(Util::StringPrintf("value%d", id), entry.value())
        << entry.Utf8DebugString();
    EXPECT_EQ(id, entry.pos()) << entry.Utf8DebugString();
    EXPECT_EQ(Util::StringPrintf("comment%d", id), entry.comment())
        << entry.Utf8DebugString();
  }
}

TEST_P(UserDictionaryStorageMigrationTest, LoadWithoutMigration) {
  {
    // Create serialized user dictionary data directly.
    UnknownFieldSet storage;
    AddTestDummyUserDictionary(GetParam(), &storage);
    WriteToFile(SerializeUnknownFieldSetAsString(storage),
                GetUserDictionaryFile());
  }

  UserDictionaryStorage storage(GetUserDictionaryFile());
  ASSERT_TRUE(storage.LoadWithoutMigration());

#ifndef ENABLE_CLOUD_SYNC
  // In binaries built without sync feature, we should make sync dictionary
  // intentionally.
  storage.EnsureSyncDictionaryExists();
#endif  // ENABLE_CLOUD_SYNC

  ASSERT_EQ(2, storage.dictionaries_size()) << storage.Utf8DebugString();
  const UserDictionary &dictionary = storage.dictionaries(0);
  ASSERT_EQ(UserDictionary::PosType_MAX - UserDictionary::PosType_MIN + 1,
            dictionary.entries_size())
      << dictionary.Utf8DebugString();
  for (int i = 0; i < dictionary.entries_size(); ++i) {
    const int id = UserDictionary::PosType_MIN + i;
    const UserDictionary::Entry &entry = dictionary.entries(i);
    EXPECT_EQ(Util::StringPrintf("key%d", id), entry.key())
        << entry.Utf8DebugString();
    EXPECT_EQ(Util::StringPrintf("value%d", id), entry.value())
        << entry.Utf8DebugString();
    switch (GetParam()) {
      case NEW_ENUM_POS:
        ASSERT_TRUE(entry.has_pos());
        EXPECT_EQ(id, entry.pos());
        break;
      case LEGACY_STRING_POS: {
        const UnknownFieldSet &unknown_field_set = entry.unknown_fields();
        ASSERT_EQ(1, unknown_field_set.field_count());
        const UnknownField &unknown_field = unknown_field_set.field(0);
        EXPECT_EQ(UnknownField::TYPE_LENGTH_DELIMITED, unknown_field.type());
        EXPECT_EQ(3, unknown_field.number());
        EXPECT_EQ(
            UserDictionaryUtil::GetStringPosType(
                static_cast<UserDictionary::PosType>(id)),
            unknown_field.length_delimited());
        break;
      }
      case LEGACY_ENUM_POS: {
        const UnknownFieldSet &unknown_field_set = entry.unknown_fields();
        ASSERT_EQ(1, unknown_field_set.field_count());
        const UnknownField &unknown_field = unknown_field_set.field(0);
        EXPECT_EQ(UnknownField::TYPE_VARINT, unknown_field.type());
        EXPECT_EQ(3, unknown_field.number());
        EXPECT_EQ(id, unknown_field.varint());
        break;
      }
      default:
        LOG(FATAL) << "Unknown serialized pos type: " << GetParam();
    }
    EXPECT_EQ(Util::StringPrintf("comment%d", id), entry.comment())
        << entry.Utf8DebugString();
  }
}

INSTANTIATE_TEST_CASE_P(
    UserDictionaryStorageProtoMigration,
    UserDictionaryStorageMigrationTest,
    ::testing::Values(NEW_ENUM_POS, LEGACY_STRING_POS, LEGACY_ENUM_POS));

}  // namespace mozc

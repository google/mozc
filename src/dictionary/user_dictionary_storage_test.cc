// Copyright 2010-2021, Google Inc.
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

#include <cstdint>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/random.h"
#include "base/system_util.h"
#include "dictionary/user_dictionary_importer.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "testing/gmock.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "absl/flags/flag.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"

namespace mozc {
namespace {

using user_dictionary::UserDictionary;

}  // namespace

class UserDictionaryStorageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    backup_user_profile_directory_ = SystemUtil::GetUserProfileDirectory();
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
    EXPECT_OK(FileUtil::UnlinkIfExists(GetUserDictionaryFile()));
  }

  void TearDown() override {
    EXPECT_OK(FileUtil::UnlinkIfExists(GetUserDictionaryFile()));
    SystemUtil::SetUserProfileDirectory(backup_user_profile_directory_);
  }

  static std::string GetUserDictionaryFile() {
    return FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test.db");
  }

 private:
  std::string backup_user_profile_directory_;
};

TEST_F(UserDictionaryStorageTest, FileTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_EQ(GetUserDictionaryFile(), storage.filename());
  EXPECT_FALSE(storage.Exists().ok());
}

TEST_F(UserDictionaryStorageTest, LockTest) {
  UserDictionaryStorage storage1(GetUserDictionaryFile());
  UserDictionaryStorage storage2(GetUserDictionaryFile());

  EXPECT_FALSE(storage1.Save().ok());
  EXPECT_FALSE(storage2.Save().ok());

  EXPECT_TRUE(storage1.Lock());
  EXPECT_FALSE(storage2.Lock());
  EXPECT_OK(storage1.Save());
  EXPECT_FALSE(storage2.Save().ok());

  EXPECT_TRUE(storage1.UnLock());
  EXPECT_FALSE(storage1.Save().ok());
  EXPECT_FALSE(storage2.Save().ok());

  EXPECT_TRUE(storage2.Lock());
  EXPECT_FALSE(storage1.Save().ok());
  EXPECT_OK(storage2.Save());
}

TEST_F(UserDictionaryStorageTest, BasicOperationsTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load().ok());

  constexpr size_t kDictionariesSize = 3;
  uint64_t id[kDictionariesSize];

  const size_t dict_size = storage.GetProto().dictionaries_size();

  for (size_t i = 0; i < kDictionariesSize; ++i) {
    EXPECT_TRUE(storage.CreateDictionary(
        "test" + std::to_string(static_cast<uint32_t>(i)), &id[i]));
    EXPECT_EQ(storage.GetProto().dictionaries_size(), i + 1 + dict_size);
  }

  for (size_t i = 0; i < kDictionariesSize; ++i) {
    EXPECT_EQ(storage.GetUserDictionaryIndex(id[i]), i + dict_size);
    EXPECT_EQ(storage.GetUserDictionaryIndex(id[i] + 1), -1);
  }

  for (size_t i = 0; i < kDictionariesSize; ++i) {
    EXPECT_EQ(storage.GetUserDictionary(id[i]),
              storage.GetProto().mutable_dictionaries(i + dict_size));
    EXPECT_EQ(storage.GetUserDictionary(id[i] + 1), nullptr);
  }

  // empty
  EXPECT_FALSE(storage.RenameDictionary(id[0], ""));

  // duplicated
  uint64_t tmp_id = 0;
  EXPECT_FALSE(storage.CreateDictionary("test0", &tmp_id));
  EXPECT_EQ(storage.GetLastError(),
            UserDictionaryStorage::DUPLICATED_DICTIONARY_NAME);

  // invalid id
  EXPECT_FALSE(storage.RenameDictionary(0, ""));

  // duplicated
  EXPECT_FALSE(storage.RenameDictionary(id[0], "test1"));
  EXPECT_EQ(storage.GetLastError(),
            UserDictionaryStorage::DUPLICATED_DICTIONARY_NAME);

  // no name
  EXPECT_TRUE(storage.RenameDictionary(id[0], "test0"));

  EXPECT_TRUE(storage.RenameDictionary(id[0], "renamed0"));
  EXPECT_EQ(storage.GetUserDictionary(id[0])->name(), "renamed0");

  // invalid id
  EXPECT_FALSE(storage.DeleteDictionary(0));

  EXPECT_TRUE(storage.DeleteDictionary(id[1]));
  EXPECT_EQ(storage.GetProto().dictionaries_size(),
            kDictionariesSize + dict_size - 1);
}

TEST_F(UserDictionaryStorageTest, DeleteTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load().ok());
  absl::BitGen gen;

  // repeat 10 times
  for (int i = 0; i < 10; ++i) {
    storage.GetProto().Clear();
    std::vector<uint64_t> ids(100);
    for (size_t i = 0; i < ids.size(); ++i) {
      EXPECT_TRUE(storage.CreateDictionary(
          "test" + std::to_string(static_cast<uint32_t>(i)), &ids[i]));
    }

    std::vector<uint64_t> alive;
    for (size_t i = 0; i < ids.size(); ++i) {
      if (absl::Bernoulli(gen, 1.0 / 3)) {  // 33%
        EXPECT_TRUE(storage.DeleteDictionary(ids[i]));
        continue;
      }
      alive.push_back(ids[i]);
    }

    EXPECT_EQ(storage.GetProto().dictionaries_size(), alive.size());

    for (size_t i = 0; i < alive.size(); ++i) {
      EXPECT_EQ(storage.GetProto().dictionaries(i).id(), alive[i]);
    }
  }
}

TEST_F(UserDictionaryStorageTest, ExportTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  uint64_t id = 0;

  EXPECT_TRUE(storage.CreateDictionary("test", &id));

  UserDictionaryStorage::UserDictionary *dic = storage.GetUserDictionary(id);

  for (size_t i = 0; i < 1000; ++i) {
    UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
    const std::string prefix = std::to_string(static_cast<uint32_t>(i));
    // set empty fields randomly
    entry->set_key(prefix + "key");
    entry->set_value(prefix + "value");
    // "名詞"
    entry->set_pos(UserDictionary::NOUN);
    entry->set_comment(prefix + "comment");
  }

  const std::string export_file =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "export.txt");

  EXPECT_FALSE(storage.ExportDictionary(id + 1, export_file));
  EXPECT_TRUE(storage.ExportDictionary(id, export_file));

  std::string file_string;
  // Copy whole contents of the file into |file_string|.
  {
    InputFileStream ifs(export_file);
    CHECK(ifs);
    ifs.seekg(0, std::ios::end);
    file_string.resize(ifs.tellg());
    ifs.seekg(0, std::ios::beg);
    ifs.read(&file_string[0], file_string.size());
    ifs.close();
  }
  UserDictionaryImporter::StringTextLineIterator iter(file_string);

  UserDictionaryStorage::UserDictionary dic2;
  EXPECT_EQ(UserDictionaryImporter::ImportFromTextLineIterator(
                UserDictionaryImporter::MOZC, &iter, &dic2),
            UserDictionaryImporter::IMPORT_NO_ERROR);

  dic2.set_id(id);
  dic2.set_name("test");

  EXPECT_EQ(dic->DebugString(), dic2.DebugString());
}

TEST_F(UserDictionaryStorageTest, SerializeTest) {
  // Repeat 20 times
  const std::string filepath = GetUserDictionaryFile();
  Random random;
  for (int n = 0; n < 20; ++n) {
    ASSERT_OK(FileUtil::UnlinkIfExists(filepath));
    UserDictionaryStorage storage1(filepath);

    {
      EXPECT_FALSE(storage1.Load().ok()) << "n = " << n;
      const size_t dic_size =
          absl::Uniform(absl::IntervalClosed, random, 1, 50);

      for (size_t i = 0; i < dic_size; ++i) {
        uint64_t id = 0;
        const std::string dic_name =
            "test" + std::to_string(static_cast<uint32_t>(i));
        EXPECT_TRUE(storage1.CreateDictionary(dic_name, &id)) << "n = " << n;
        const size_t entry_size =
            absl::Uniform(absl::IntervalClosed, random, 1, 100);
        for (size_t j = 0; j < entry_size; ++j) {
          UserDictionaryStorage::UserDictionary *dic =
              storage1.GetProto().mutable_dictionaries(i);
          UserDictionaryStorage::UserDictionaryEntry *entry =
              dic->add_entries();
          constexpr char32_t lo = ' ', hi = '~';
          entry->set_key(random.Utf8StringRandomLen(10, lo, hi));
          entry->set_value(random.Utf8StringRandomLen(10, lo, hi));
          entry->set_pos(UserDictionary::NOUN);
          entry->set_comment(random.Utf8StringRandomLen(10, lo, hi));
        }
      }

      EXPECT_TRUE(storage1.Lock()) << "n = " << n;
      EXPECT_OK(storage1.Save()) << "n = " << n;
      EXPECT_TRUE(storage1.UnLock()) << "n = " << n;
    }

    UserDictionaryStorage storage2(GetUserDictionaryFile());
    EXPECT_OK(storage2.Load()) << "n = " << n;

    EXPECT_EQ(storage2.GetProto().DebugString(),
              storage1.GetProto().DebugString())
        << "n = " << n;
  }
}

TEST_F(UserDictionaryStorageTest, GetUserDictionaryIdTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load().ok());

  constexpr size_t kDictionariesSize = 3;
  uint64_t id[kDictionariesSize];
  EXPECT_TRUE(storage.CreateDictionary("testA", &id[0]));
  EXPECT_TRUE(storage.CreateDictionary("testB", &id[1]));

  uint64_t ret_id[kDictionariesSize];
  EXPECT_TRUE(storage.GetUserDictionaryId("testA", &ret_id[0]));
  EXPECT_TRUE(storage.GetUserDictionaryId("testB", &ret_id[1]));
  EXPECT_FALSE(storage.GetUserDictionaryId("testC", &ret_id[2]));

  EXPECT_EQ(ret_id[0], id[0]);
  EXPECT_EQ(ret_id[1], id[1]);
}

TEST_F(UserDictionaryStorageTest, ConvertSyncDictionariesToNormalDictionaries) {
  // "名詞"
  const UserDictionary::PosType kPos = UserDictionary::NOUN;

  const struct TestData {
    bool is_sync_dictionary;
    bool is_removed_dictionary;
    bool has_normal_entry;
    bool has_removed_entry;
    std::string dictionary_name;
  } test_data[] = {
      {false, false, false, false, "non-sync dictionary (empty)"},
      {false, false, true, false, "non-sync dictionary (normal entry)"},
      {true, false, false, false, "sync dictionary (empty)"},
      {true, false, false, true, "sync dictionary (removed entry)"},
      {true, false, true, false, "sync dictionary (normal entry)"},
      {true, false, true, true, "sync dictionary (normal & removed entries)"},
      {true, true, false, false, "removed sync dictionary (empty)"},
      {true, true, false, true, "removed sync dictionary (removed entry)"},
      {true, true, true, false, "removed sync dictionary (normal entry)"},
      {true, true, true, true,
       "removed sync dictionary (normal & removed entries)"},
      {true, false, true, false,
       UserDictionaryStorage::default_sync_dictionary_name()},
  };

  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_FALSE(storage.Load().ok())
      << "At first, we expect there is not user dictionary file.";
  EXPECT_FALSE(storage.ConvertSyncDictionariesToNormalDictionaries())
      << "No sync dictionary available.";

  for (size_t i = 0; i < std::size(test_data); ++i) {
    SCOPED_TRACE(absl::StrFormat("add %d", static_cast<int>(i)));
    const TestData &data = test_data[i];
    CHECK(data.is_sync_dictionary ||
          !(data.is_removed_dictionary || data.has_removed_entry))
        << "Non-sync dictionary should NOT have removed dictionary / entry.";

    uint64_t dict_id = 0;
    ASSERT_TRUE(storage.CreateDictionary(data.dictionary_name, &dict_id));
    UserDictionaryStorage::UserDictionary *dict =
        storage.GetProto().mutable_dictionaries(
            storage.GetUserDictionaryIndex(dict_id));
    dict->set_syncable(data.is_sync_dictionary);
    dict->set_removed(data.is_removed_dictionary);
    if (data.has_normal_entry) {
      UserDictionaryStorage::UserDictionaryEntry *entry = dict->add_entries();
      entry->set_key("normal");
      entry->set_value("normal entry");
      entry->set_pos(kPos);
    }
    if (data.has_removed_entry) {
      UserDictionaryStorage::UserDictionaryEntry *entry = dict->add_entries();
      entry->set_key("removed");
      entry->set_value("removed entry");
      entry->set_pos(kPos);
      entry->set_removed(true);
    }
  }
  EXPECT_EQ(
      UserDictionaryStorage::CountSyncableDictionaries(storage.GetProto()), 9);

  ASSERT_TRUE(storage.ConvertSyncDictionariesToNormalDictionaries());

  constexpr char kDictionaryNameConvertedFromSyncableDictionary[] =
      "同期用辞書";
  const struct ExpectedData {
    bool has_normal_entry;
    std::string dictionary_name;
  } expected_data[] = {
      {false, "non-sync dictionary (empty)"},
      {true, "non-sync dictionary (normal entry)"},
      {true, "sync dictionary (normal entry)"},
      {true, "sync dictionary (normal & removed entries)"},
      {true, kDictionaryNameConvertedFromSyncableDictionary},
  };

  EXPECT_EQ(
      0, UserDictionaryStorage::CountSyncableDictionaries(storage.GetProto()));
  ASSERT_EQ(std::size(expected_data), storage.GetProto().dictionaries_size());
  for (size_t i = 0; i < std::size(expected_data); ++i) {
    SCOPED_TRACE(absl::StrFormat("verify %d", static_cast<int>(i)));
    const ExpectedData &expected = expected_data[i];
    const UserDictionaryStorage::UserDictionary &dict =
        storage.GetProto().dictionaries(i);

    EXPECT_EQ(dict.name(), expected.dictionary_name);
    EXPECT_FALSE(dict.syncable());
    EXPECT_FALSE(dict.removed());
    if (expected.has_normal_entry) {
      ASSERT_EQ(dict.entries_size(), 1);
      EXPECT_EQ(dict.entries(0).key(), "normal");
    } else {
      EXPECT_EQ(dict.entries_size(), 0);
    }
  }

  // Test duplicated dictionary name.
  storage.GetProto().Clear();
  {
    uint64_t dict_id = 0;
    storage.CreateDictionary(
        UserDictionaryStorage::default_sync_dictionary_name(), &dict_id);
    storage.CreateDictionary(kDictionaryNameConvertedFromSyncableDictionary,
                             &dict_id);
    ASSERT_EQ(2, storage.GetProto().dictionaries_size());
    UserDictionaryStorage::UserDictionary *dict;
    dict = storage.GetProto().mutable_dictionaries(0);
    dict->set_syncable(true);
    dict->add_entries()->set_key("0");
    dict = storage.GetProto().mutable_dictionaries(1);
    dict->set_syncable(false);
    dict->add_entries()->set_key("1");
  }
  ASSERT_TRUE(storage.ConvertSyncDictionariesToNormalDictionaries());
  EXPECT_EQ(
      UserDictionaryStorage::CountSyncableDictionaries(storage.GetProto()), 0);
  EXPECT_EQ(storage.GetProto().dictionaries_size(), 2);
  EXPECT_EQ(
      storage.GetProto().dictionaries(0).name(),
      absl::StrFormat("%s_1", kDictionaryNameConvertedFromSyncableDictionary));
  EXPECT_EQ(storage.GetProto().dictionaries(1).name(),
            kDictionaryNameConvertedFromSyncableDictionary);
}

TEST_F(UserDictionaryStorageTest, Export) {
  constexpr int kDummyDictionaryId = 10;
  const std::string kPath =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "exported_file");

  {
    UserDictionaryStorage storage(GetUserDictionaryFile());
    {
      UserDictionary *dictionary = storage.GetProto().add_dictionaries();
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
#ifdef OS_WIN
  EXPECT_EQ(std::string(mapped_data.begin(), mapped_data.size()),
            "key\tvalue\t名詞\tcomment\r\n");
#else   // OS_WIN
  EXPECT_EQ(std::string(mapped_data.begin(), mapped_data.size()),
            "key\tvalue\t名詞\tcomment\n");
#endif  // OS_WIN
}

}  // namespace mozc

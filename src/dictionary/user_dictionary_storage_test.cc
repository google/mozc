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

#include <cstddef>
#include <cstdint>
#include <ios>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/file/temp_dir.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/mmap.h"
#include "base/random.h"
#include "base/system_util.h"
#include "dictionary/user_dictionary_importer.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

#define EXPECT_NOT_OK(expr) EXPECT_FALSE((expr).ok())

using ::mozc::user_dictionary::UserDictionary;

class UserDictionaryStorageTest : public testing::TestWithTempUserProfile {
 protected:
  std::string GetUserDictionaryFile() {
    return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), "test.db");
  }
};

TEST_F(UserDictionaryStorageTest, FileTest) {
  const UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_EQ(GetUserDictionaryFile(), storage.filename());
  EXPECT_NOT_OK(storage.Exists());
}

TEST_F(UserDictionaryStorageTest, LockTest) {
  UserDictionaryStorage storage1(GetUserDictionaryFile());
  UserDictionaryStorage storage2(GetUserDictionaryFile());

  EXPECT_NOT_OK(storage1.Save());
  EXPECT_NOT_OK(storage2.Save());

  EXPECT_TRUE(storage1.Lock());
  EXPECT_FALSE(storage2.Lock());
  EXPECT_OK(storage1.Save());
  EXPECT_NOT_OK(storage2.Save());

  EXPECT_TRUE(storage1.UnLock());
  EXPECT_NOT_OK(storage1.Save());
  EXPECT_NOT_OK(storage2.Save());

  EXPECT_TRUE(storage2.Lock());
  EXPECT_NOT_OK(storage1.Save());
  EXPECT_OK(storage2.Save());
}

TEST_F(UserDictionaryStorageTest, BasicOperationsTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_NOT_OK(storage.Load());

  constexpr size_t kDictionariesSize = 3;
  uint64_t id[kDictionariesSize];

  const size_t dict_size = storage.GetProto().dictionaries_size();

  for (size_t i = 0; i < kDictionariesSize; ++i) {
    absl::StatusOr<uint64_t> s =
        storage.CreateDictionary(absl::StrCat("test", i));
    EXPECT_OK(s);
    id[i] = s.value();
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
  EXPECT_NOT_OK(storage.RenameDictionary(id[0], ""));

  // duplicated
  EXPECT_EQ(storage.CreateDictionary("test0").status().raw_code(),
            UserDictionaryStorage::DUPLICATED_DICTIONARY_NAME);

  // invalid id
  EXPECT_NOT_OK(storage.RenameDictionary(0, ""));

  // duplicated
  EXPECT_EQ(storage.RenameDictionary(id[0], "test1").raw_code(),
            UserDictionaryStorage::DUPLICATED_DICTIONARY_NAME);

  // no name
  EXPECT_OK(storage.RenameDictionary(id[0], "test0"));

  EXPECT_OK(storage.RenameDictionary(id[0], "renamed0"));
  EXPECT_EQ(storage.GetUserDictionary(id[0])->name(), "renamed0");

  // invalid id
  EXPECT_NOT_OK(storage.DeleteDictionary(0));

  EXPECT_OK(storage.DeleteDictionary(id[1]));
  EXPECT_EQ(storage.GetProto().dictionaries_size(),
            kDictionariesSize + dict_size - 1);
}

TEST_F(UserDictionaryStorageTest, DeleteTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_NOT_OK(storage.Load());
  absl::BitGen gen;

  // repeat 10 times
  for (int i = 0; i < 10; ++i) {
    storage.GetProto().Clear();
    std::vector<uint64_t> ids(100);
    for (size_t i = 0; i < ids.size(); ++i) {
      ids[i] = storage.CreateDictionary(absl::StrCat("test", i)).value();
    }

    std::vector<uint64_t> alive;
    for (size_t i = 0; i < ids.size(); ++i) {
      if (absl::Bernoulli(gen, 1.0 / 3)) {  // 33%
        EXPECT_OK(storage.DeleteDictionary(ids[i]));
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
  const uint64_t id = storage.CreateDictionary("test").value();

  UserDictionaryStorage::UserDictionary *dic = storage.GetUserDictionary(id);
  EXPECT_TRUE(dic);

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

  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string export_file =
      FileUtil::JoinPath(temp_dir.path(), "export.txt");

  EXPECT_NOT_OK(storage.ExportDictionary(id + 1, export_file));
  EXPECT_OK(storage.ExportDictionary(id, export_file));

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

  EXPECT_EQ(absl::StrCat(*dic), absl::StrCat(dic2));
}

TEST_F(UserDictionaryStorageTest, SerializeTest) {
  // Repeat 20 times
  const std::string filepath = GetUserDictionaryFile();
  Random random;
  for (int n = 0; n < 20; ++n) {
    ASSERT_OK(FileUtil::UnlinkIfExists(filepath));
    UserDictionaryStorage storage1(filepath);

    {
      EXPECT_NOT_OK(storage1.Load()) << "n = " << n;
      const size_t dic_size =
          absl::Uniform(absl::IntervalClosed, random, 1, 50);

      for (size_t i = 0; i < dic_size; ++i) {
        EXPECT_OK(storage1.CreateDictionary(absl::StrCat("test", i)));
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

    EXPECT_EQ(absl::StrCat(storage2.GetProto()),
              absl::StrCat(storage1.GetProto()))
        << "n = " << n;
  }
}

TEST_F(UserDictionaryStorageTest, GetUserDictionaryIdTest) {
  UserDictionaryStorage storage(GetUserDictionaryFile());
  EXPECT_NOT_OK(storage.Load());

  constexpr size_t kDictionariesSize = 3;
  uint64_t id[kDictionariesSize];
  id[0] = storage.CreateDictionary("testA").value();
  id[1] = storage.CreateDictionary("testB").value();

  uint64_t ret_id[kDictionariesSize];
  ret_id[0] = storage.GetUserDictionaryId("testA").value();
  ret_id[1] = storage.GetUserDictionaryId("testB").value();
  EXPECT_NOT_OK(storage.GetUserDictionaryId("testC"));

  EXPECT_EQ(ret_id[0], id[0]);
  EXPECT_EQ(ret_id[1], id[1]);
}

TEST_F(UserDictionaryStorageTest, Export) {
  constexpr int kDummyDictionaryId = 10;
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string kPath =
      FileUtil::JoinPath(temp_dir.path(), "exported_file");

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
    EXPECT_OK(storage.ExportDictionary(kDummyDictionaryId, kPath));
  }

  const absl::StatusOr<Mmap> mapped_data = Mmap::Map(kPath);
  ASSERT_OK(mapped_data) << mapped_data.status();

  // Make sure the exported format, especially that the pos is exported in
  // Japanese.
#ifdef _WIN32
  EXPECT_EQ(std::string(mapped_data->begin(), mapped_data->size()),
            "key\tvalue\t名詞\tcomment\r\n");
#else   // _WIN32
  EXPECT_EQ(std::string(mapped_data->begin(), mapped_data->size()),
            "key\tvalue\t名詞\tcomment\n");
#endif  // _WIN32
}

}  // namespace
}  // namespace mozc

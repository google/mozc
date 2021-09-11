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

#include "dictionary/user_dictionary_util.h"

#include <cstdint>

#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/testing_util.h"

namespace mozc {

using user_dictionary::UserDictionary;
using user_dictionary::UserDictionaryCommandStatus;

static void TestNormalizeReading(const std::string &golden,
                                 const std::string &input) {
  std::string output;
  UserDictionaryUtil::NormalizeReading(input, &output);
  EXPECT_EQ(golden, output);
}

TEST(UserDictionaryUtilTest, TestIsValidReading) {
  EXPECT_TRUE(UserDictionaryUtil::IsValidReading("ABYZabyz0189"));
  EXPECT_TRUE(UserDictionaryUtil::IsValidReading("〜「」"));
  EXPECT_TRUE(UserDictionaryUtil::IsValidReading("あいうわをんゔ"));
  EXPECT_TRUE(UserDictionaryUtil::IsValidReading("アイウワヲンヴ"));
  EXPECT_FALSE(UserDictionaryUtil::IsValidReading("水雲"));

  // COMBINING KATAKANA-HIRAGANA VOICED/SEMI-VOICED SOUND MARK (u3099, u309A)
  EXPECT_FALSE(UserDictionaryUtil::IsValidReading("゙゚"));

  // KATAKANA-HIRAGANA VOICED/SEMI-VOICED SOUND MARK (u309B, u309C)
  EXPECT_TRUE(UserDictionaryUtil::IsValidReading("゛゜"));

  EXPECT_FALSE(UserDictionaryUtil::IsValidReading("𠮷"));
  EXPECT_FALSE(UserDictionaryUtil::IsValidReading("😁"));
  EXPECT_FALSE(UserDictionaryUtil::IsValidReading("ヷ"));
  EXPECT_FALSE(UserDictionaryUtil::IsValidReading("ヺ"));
  EXPECT_TRUE(UserDictionaryUtil::IsValidReading("。「」、・"));
}

TEST(UserDictionaryUtilTest, TestNormalizeReading) {
  TestNormalizeReading("あいうゔゎ", "アイウヴヮ");
  TestNormalizeReading("あいうゃ", "ｱｲｳｬ");
  TestNormalizeReading("ABab01@&=|", "ＡＢａｂ０１＠＆＝｜");
  TestNormalizeReading("。「」、・", "｡｢｣､･");
}

namespace {
struct UserDictionaryEntryData {
  const char *key;
  const char *value;
  const UserDictionary::PosType pos;
  const char *comment;
};

void ConvertUserDictionaryEntry(const UserDictionaryEntryData &input,
                                UserDictionary::Entry *output) {
  output->set_key(input.key);
  output->set_value(input.value);
  output->set_pos(input.pos);
  output->set_comment(input.comment);
}
}  // namespace

TEST(UserDictionaryUtilTest, TestSanitizeEntry) {
  const UserDictionaryEntryData golden_data = {
      "abc",
      "abc",
      UserDictionary::NOUN,
      "abc",
  };

  UserDictionary::Entry golden, input;
  ConvertUserDictionaryEntry(golden_data, &golden);

  {
    UserDictionaryEntryData input_data = {"abc", "abc", UserDictionary::NOUN,
                                          "abc"};
    ConvertUserDictionaryEntry(input_data, &input);
    EXPECT_FALSE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_EQ(golden.DebugString(), input.DebugString());
  }

  {
    UserDictionaryEntryData input_data = {"ab\tc", "abc", UserDictionary::NOUN,
                                          "abc"};
    ConvertUserDictionaryEntry(input_data, &input);
    EXPECT_TRUE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_EQ(golden.DebugString(), input.DebugString());
  }

  {
    UserDictionaryEntryData input_data = {"abc", "ab\tc", UserDictionary::NOUN,
                                          "ab\tc"};
    ConvertUserDictionaryEntry(input_data, &input);
    EXPECT_TRUE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_EQ(golden.DebugString(), input.DebugString());
  }

  {
    UserDictionaryEntryData input_data = {"ab\tc", "ab\tc",
                                          UserDictionary::NOUN, "ab\tc"};
    ConvertUserDictionaryEntry(input_data, &input);
    EXPECT_TRUE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_EQ(golden.DebugString(), input.DebugString());
  }
}

TEST(UserDictionaryUtilTest, TestSanitize) {
  std::string str(10, '\t');
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 5));
  EXPECT_EQ("", str);

  str = "ab\tc";
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 10));
  EXPECT_EQ("abc", str);

  str = "かしゆか";
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 3));
  EXPECT_EQ("か", str);

  str = "かしゆか";
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 4));
  EXPECT_EQ("か", str);

  str = "かしゆか";
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 5));
  EXPECT_EQ("か", str);

  str = "かしゆか";
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 6));
  EXPECT_EQ("かし", str);

  str = "かしゆか";
  EXPECT_FALSE(UserDictionaryUtil::Sanitize(&str, 100));
  EXPECT_EQ("かしゆか", str);
}

TEST(UserDictionaryUtilTest, ValidateEntry) {
  // Create a valid entry.
  UserDictionary::Entry base_entry;
  base_entry.set_key("よみ");
  base_entry.set_value("単語");
  base_entry.set_pos(UserDictionary::NOUN);
  base_entry.set_comment("コメント");

  UserDictionary::Entry entry = base_entry;
  EXPECT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            UserDictionaryUtil::ValidateEntry(entry));

  entry = base_entry;
  entry.clear_key();
  EXPECT_EQ(UserDictionaryCommandStatus::READING_EMPTY,
            UserDictionaryUtil::ValidateEntry(entry));

  entry = base_entry;
  entry.set_key(std::string(500, 'a'));
  EXPECT_EQ(UserDictionaryCommandStatus::READING_TOO_LONG,
            UserDictionaryUtil::ValidateEntry(entry));

  entry = base_entry;
  entry.set_key("a\nb");
  EXPECT_EQ(UserDictionaryCommandStatus::READING_CONTAINS_INVALID_CHARACTER,
            UserDictionaryUtil::ValidateEntry(entry));

  entry = base_entry;
  entry.clear_value();
  EXPECT_EQ(UserDictionaryCommandStatus::WORD_EMPTY,
            UserDictionaryUtil::ValidateEntry(entry));

  entry = base_entry;
  entry.set_value(std::string(500, 'a'));
  EXPECT_EQ(UserDictionaryCommandStatus::WORD_TOO_LONG,
            UserDictionaryUtil::ValidateEntry(entry));

  entry = base_entry;
  entry.set_value("a\nb");
  EXPECT_EQ(UserDictionaryCommandStatus::WORD_CONTAINS_INVALID_CHARACTER,
            UserDictionaryUtil::ValidateEntry(entry));

  entry = base_entry;
  entry.clear_comment();
  EXPECT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            UserDictionaryUtil::ValidateEntry(entry));

  entry = base_entry;
  entry.set_comment(std::string(500, 'a'));
  EXPECT_EQ(UserDictionaryCommandStatus::COMMENT_TOO_LONG,
            UserDictionaryUtil::ValidateEntry(entry));

  entry = base_entry;
  entry.set_comment("a\nb");
  EXPECT_EQ(UserDictionaryCommandStatus::COMMENT_CONTAINS_INVALID_CHARACTER,
            UserDictionaryUtil::ValidateEntry(entry));

  entry = base_entry;
  entry.clear_pos();
  EXPECT_EQ(UserDictionaryCommandStatus::INVALID_POS_TYPE,
            UserDictionaryUtil::ValidateEntry(entry));
}

TEST(UserDictionaryUtilTest, ValidateDictionaryName) {
  EXPECT_EQ(
      UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY,
      UserDictionaryUtil::ValidateDictionaryName(
          user_dictionary::UserDictionaryStorage::default_instance(), ""));

  EXPECT_EQ(UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG,
            UserDictionaryUtil::ValidateDictionaryName(
                user_dictionary::UserDictionaryStorage::default_instance(),
                std::string(500, 'a')));

  EXPECT_EQ(
      UserDictionaryCommandStatus ::DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER,
      UserDictionaryUtil::ValidateDictionaryName(
          user_dictionary::UserDictionaryStorage::default_instance(), "a\nbc"));

  user_dictionary::UserDictionaryStorage storage;
  storage.add_dictionaries()->set_name("abc");
  EXPECT_EQ(UserDictionaryCommandStatus::DICTIONARY_NAME_DUPLICATED,
            UserDictionaryUtil::ValidateDictionaryName(storage, "abc"));
}

TEST(UserDictionaryUtilTest, IsStorageFull) {
  user_dictionary::UserDictionaryStorage storage;
  for (int i = 0; i < UserDictionaryUtil::max_dictionary_size(); ++i) {
    EXPECT_FALSE(UserDictionaryUtil::IsStorageFull(storage));
    storage.add_dictionaries();
  }

  EXPECT_TRUE(UserDictionaryUtil::IsStorageFull(storage));
}

TEST(UserDictionaryUtilTest, IsDictionaryFull) {
  UserDictionary dictionary;
  for (int i = 0; i < UserDictionaryUtil::max_entry_size(); ++i) {
    EXPECT_FALSE(UserDictionaryUtil::IsDictionaryFull(dictionary));
    dictionary.add_entries();
  }

  EXPECT_TRUE(UserDictionaryUtil::IsDictionaryFull(dictionary));
}

TEST(UserDictionaryUtilTest, GetUserDictionaryById) {
  user_dictionary::UserDictionaryStorage storage;
  storage.add_dictionaries()->set_id(1);
  storage.add_dictionaries()->set_id(2);

  EXPECT_TRUE(UserDictionaryUtil::GetUserDictionaryById(storage, 1) ==
              &storage.dictionaries(0));
  EXPECT_TRUE(UserDictionaryUtil::GetUserDictionaryById(storage, 2) ==
              &storage.dictionaries(1));
  EXPECT_TRUE(UserDictionaryUtil::GetUserDictionaryById(storage, -1) ==
              nullptr);
}

TEST(UserDictionaryUtilTest, GetMutableUserDictionaryById) {
  user_dictionary::UserDictionaryStorage storage;
  storage.add_dictionaries()->set_id(1);
  storage.add_dictionaries()->set_id(2);

  EXPECT_TRUE(UserDictionaryUtil::GetMutableUserDictionaryById(&storage, 1) ==
              storage.mutable_dictionaries(0));
  EXPECT_TRUE(UserDictionaryUtil::GetMutableUserDictionaryById(&storage, 2) ==
              storage.mutable_dictionaries(1));
  EXPECT_TRUE(UserDictionaryUtil::GetMutableUserDictionaryById(&storage, -1) ==
              nullptr);
}

TEST(UserDictionaryUtilTest, GetUserDictionaryIndexById) {
  user_dictionary::UserDictionaryStorage storage;
  storage.add_dictionaries()->set_id(1);
  storage.add_dictionaries()->set_id(2);

  EXPECT_EQ(0, UserDictionaryUtil::GetUserDictionaryIndexById(storage, 1));
  EXPECT_EQ(1, UserDictionaryUtil::GetUserDictionaryIndexById(storage, 2));

  // Return -1 for a failing case.
  EXPECT_EQ(-1, UserDictionaryUtil::GetUserDictionaryIndexById(storage, -1));
}

TEST(UserDictionaryUtilTest, CreateDictionary) {
  user_dictionary::UserDictionaryStorage storage;
  uint64_t dictionary_id;

  // Check dictionary validity.
  EXPECT_EQ(UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY,
            UserDictionaryUtil::CreateDictionary(&storage, "", &dictionary_id));

  // Check the limit of the number of dictionaries.
  storage.Clear();
  for (int i = 0; i < UserDictionaryUtil::max_dictionary_size(); ++i) {
    storage.add_dictionaries();
  }

  EXPECT_EQ(UserDictionaryCommandStatus::DICTIONARY_SIZE_LIMIT_EXCEEDED,
            UserDictionaryUtil::CreateDictionary(&storage, "new dictionary",
                                                 &dictionary_id));

  storage.Clear();
  EXPECT_EQ(UserDictionaryCommandStatus::UNKNOWN_ERROR,
            UserDictionaryUtil::CreateDictionary(&storage, "new dictionary",
                                                 nullptr));

  ASSERT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            UserDictionaryUtil::CreateDictionary(&storage, "new dictionary",
                                                 &dictionary_id));

  EXPECT_PROTO_PEQ(
      "dictionaries <\n"
      "  name: \"new dictionary\"\n"
      ">\n",
      storage);
  EXPECT_EQ(dictionary_id, storage.dictionaries(0).id());
}

TEST(UserDictionaryUtilTest, DeleteDictionary) {
  user_dictionary::UserDictionaryStorage storage;
  storage.add_dictionaries()->set_id(1);
  storage.add_dictionaries()->set_id(2);

  // Simplest deleting case.
  int original_index;
  ASSERT_TRUE(UserDictionaryUtil::DeleteDictionary(&storage, 1, &original_index,
                                                   nullptr));
  EXPECT_EQ(0, original_index);
  ASSERT_EQ(1, storage.dictionaries_size());
  EXPECT_EQ(2, storage.dictionaries(0).id());

  // Deletion for unknown dictionary should fail.
  storage.Clear();
  storage.add_dictionaries()->set_id(1);
  storage.add_dictionaries()->set_id(2);
  EXPECT_FALSE(
      UserDictionaryUtil::DeleteDictionary(&storage, 100, nullptr, nullptr));

  // Keep deleted dictionary.
  storage.Clear();
  storage.add_dictionaries()->set_id(1);
  storage.add_dictionaries()->set_id(2);
  UserDictionary *deleted_dictionary;
  EXPECT_TRUE(UserDictionaryUtil::DeleteDictionary(&storage, 1, nullptr,
                                                   &deleted_dictionary));
  ASSERT_EQ(1, storage.dictionaries_size());
  EXPECT_EQ(2, storage.dictionaries(0).id());
  EXPECT_EQ(1, deleted_dictionary->id());

  // Delete to avoid memoary leaking.
  delete deleted_dictionary;
}

}  // namespace mozc

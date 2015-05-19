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

#include "dictionary/user_dictionary_util.h"

#include "base/testing_util.h"
#include "base/util.h"
#include "base/protobuf/message.h"
#include "base/protobuf/unknown_field_set.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {

using mozc::protobuf::UnknownField;
using mozc::protobuf::UnknownFieldSet;
using user_dictionary::UserDictionary;
using user_dictionary::UserDictionaryCommandStatus;

static void TestNormalizeReading(const string &golden, const string &input) {
  string output;
  UserDictionaryUtil::NormalizeReading(input, &output);
  EXPECT_EQ(golden, output);
}

TEST(UserDictionaryUtilTest, TestIsValidReading) {
  EXPECT_TRUE(UserDictionaryUtil::IsValidReading("ABYZabyz0189"));
  // "〜「」"
  EXPECT_TRUE(UserDictionaryUtil::IsValidReading(
                  "\xe3\x80\x9c\xe3\x80\x8c\xe3\x80\x8d"));
  // "あいうわをんゔ"
  EXPECT_TRUE(UserDictionaryUtil::IsValidReading(
                  "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x82\x8f\xe3\x82\x92"
                  "\xe3\x82\x93\xe3\x82\x94"));
  // "アイウワヲンヴ"
  EXPECT_TRUE(UserDictionaryUtil::IsValidReading(
                  "\xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xa6\xe3\x83\xaf\xe3\x83\xb2"
                  "\xe3\x83\xb3\xe3\x83\xb4"));
  // "水雲"
  EXPECT_FALSE(UserDictionaryUtil::IsValidReading("\xe6\xb0\xb4\xe9\x9b\xb2"));

  // COMBINING KATAKANA-HIRAGANA VOICED/SEMI-VOICED SOUND MARK (u3099, u309A)
  EXPECT_FALSE(UserDictionaryUtil::IsValidReading("\xE3\x82\x99\xE3\x82\x9A"));

  // KATAKANA-HIRAGANA VOICED/SEMI-VOICED SOUND MARK (u309B, u309C)
  EXPECT_TRUE(UserDictionaryUtil::IsValidReading("\xE3\x82\x9B\xE3\x82\x9C"));
}

TEST(UserDictionaryUtilTest, TestNormalizeReading) {
  // "あいうゔゎ", "アイウヴヮ"
  TestNormalizeReading(
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x82\x94\xe3\x82\x8e",
      "\xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xa6\xe3\x83\xb4\xe3\x83\xae");
  // "あいうゃ", "ｱｲｳｬ"
  TestNormalizeReading(
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x82\x83",
      "\xef\xbd\xb1\xef\xbd\xb2\xef\xbd\xb3\xef\xbd\xac");
  // "ＡＢａｂ０１＠＆＝｜"
  TestNormalizeReading(
      "ABab01@&=|",
      "\xef\xbc\xa1\xef\xbc\xa2\xef\xbd\x81\xef\xbd\x82\xef\xbc\x90\xef\xbc\x91"
      "\xef\xbc\xa0\xef\xbc\x86\xef\xbc\x9d\xef\xbd\x9c");
}

namespace {
struct UserDictionaryEntryData {
  const char *key;
  const char *value;
  const UserDictionary::PosType pos;
  const char *comment;
};

void ConvertUserDictionaryEntry(
    const UserDictionaryEntryData &input, UserDictionary::Entry *output) {
  output->set_key(input.key);
  output->set_value(input.value);
  output->set_pos(input.pos);
  output->set_comment(input.comment);
}
}  // namespace

TEST(UserDictionaryUtilTest, TestSanitizeEntry) {
  const UserDictionaryEntryData golden_data = {
    "abc", "abc", UserDictionary::NOUN, "abc",
  };

  UserDictionary::Entry golden, input;
  ConvertUserDictionaryEntry(golden_data, &golden);

  {
    UserDictionaryEntryData input_data = {
      "abc", "abc", UserDictionary::NOUN, "abc" };
    ConvertUserDictionaryEntry(input_data, &input);
    EXPECT_FALSE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_EQ(golden.DebugString(), input.DebugString());
  }

  {
    UserDictionaryEntryData input_data = {
      "ab\tc", "abc", UserDictionary::NOUN, "abc" };
    ConvertUserDictionaryEntry(input_data, &input);
    EXPECT_TRUE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_EQ(golden.DebugString(), input.DebugString());
  }

  {
    UserDictionaryEntryData input_data = {
      "abc", "ab\tc", UserDictionary::NOUN, "ab\tc" };
    ConvertUserDictionaryEntry(input_data, &input);
    EXPECT_TRUE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_EQ(golden.DebugString(), input.DebugString());
  }

  {
    UserDictionaryEntryData input_data = {
      "ab\tc", "ab\tc", UserDictionary::NOUN, "ab\tc" };
    ConvertUserDictionaryEntry(input_data, &input);
    EXPECT_TRUE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_EQ(golden.DebugString(), input.DebugString());
  }
}

TEST(UserDictionaryUtilTest, TestSanitize) {
  string str('\t', 10);
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 5));
  EXPECT_EQ("", str);

  str = "ab\tc";
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 10));
  EXPECT_EQ("abc", str);

  str = "\xE3\x81\x8B\xE3\x81\x97\xE3\x82\x86\xE3\x81\x8B";  // "かしゆか"
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 3));
  EXPECT_EQ("\xE3\x81\x8B", str);  // "か"

  str = "\xE3\x81\x8B\xE3\x81\x97\xE3\x82\x86\xE3\x81\x8B";  // "かしゆか"
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 4));
  EXPECT_EQ("\xE3\x81\x8B", str);  // "か"

  str = "\xE3\x81\x8B\xE3\x81\x97\xE3\x82\x86\xE3\x81\x8B";  // "かしゆか"
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 5));
  EXPECT_EQ("\xE3\x81\x8B", str);  // "か"

  str = "\xE3\x81\x8B\xE3\x81\x97\xE3\x82\x86\xE3\x81\x8B";  // "かしゆか"
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 6));
  EXPECT_EQ("\xE3\x81\x8B\xE3\x81\x97", str);  // "かし"

  str = "\xE3\x81\x8B\xE3\x81\x97\xE3\x82\x86\xE3\x81\x8B";  // "かしゆか"
  EXPECT_FALSE(UserDictionaryUtil::Sanitize(&str, 100));
  // "かしゆか"
  EXPECT_EQ("\xE3\x81\x8B\xE3\x81\x97\xE3\x82\x86\xE3\x81\x8B", str);
}

TEST(UserDictionaryUtilTest, ValidateEntry) {
  // Create a valid entry.
  UserDictionary::Entry base_entry;
  // "よみ"
  base_entry.set_key("\xE3\x82\x88\xE3\x81\xBF");

  // "単語"
  base_entry.set_value("\xE5\x8D\x98\xE8\xAA\x9E");

  // "名詞"
  base_entry.set_pos(UserDictionary::NOUN);

  // "コメント"
  base_entry.set_comment("\xE3\x82\xB3\xE3\x83\xA1\xE3\x83\xB3\xE3\x83\x88");


  UserDictionary::Entry entry;
  entry.CopyFrom(base_entry);
  EXPECT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            UserDictionaryUtil::ValidateEntry(entry));

  entry.CopyFrom(base_entry);
  entry.clear_key();
  EXPECT_EQ(UserDictionaryCommandStatus::READING_EMPTY,
            UserDictionaryUtil::ValidateEntry(entry));

  entry.CopyFrom(base_entry);
  entry.set_key(string(500, 'a'));
  EXPECT_EQ(UserDictionaryCommandStatus::READING_TOO_LONG,
            UserDictionaryUtil::ValidateEntry(entry));

  entry.CopyFrom(base_entry);
  entry.set_key("a\nb");
  EXPECT_EQ(UserDictionaryCommandStatus::READING_CONTAINS_INVALID_CHARACTER,
            UserDictionaryUtil::ValidateEntry(entry));

  entry.CopyFrom(base_entry);
  entry.clear_value();
  EXPECT_EQ(UserDictionaryCommandStatus::WORD_EMPTY,
            UserDictionaryUtil::ValidateEntry(entry));

  entry.CopyFrom(base_entry);
  entry.set_value(string(500, 'a'));
  EXPECT_EQ(UserDictionaryCommandStatus::WORD_TOO_LONG,
            UserDictionaryUtil::ValidateEntry(entry));

  entry.CopyFrom(base_entry);
  entry.set_value("a\nb");
  EXPECT_EQ(UserDictionaryCommandStatus::WORD_CONTAINS_INVALID_CHARACTER,
            UserDictionaryUtil::ValidateEntry(entry));

  entry.CopyFrom(base_entry);
  entry.clear_comment();
  EXPECT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            UserDictionaryUtil::ValidateEntry(entry));

  entry.CopyFrom(base_entry);
  entry.set_comment(string(500, 'a'));
  EXPECT_EQ(UserDictionaryCommandStatus::COMMENT_TOO_LONG,
            UserDictionaryUtil::ValidateEntry(entry));

  entry.CopyFrom(base_entry);
  entry.set_comment("a\nb");
  EXPECT_EQ(UserDictionaryCommandStatus::COMMENT_CONTAINS_INVALID_CHARACTER,
            UserDictionaryUtil::ValidateEntry(entry));

  entry.CopyFrom(base_entry);
  entry.clear_pos();
  EXPECT_EQ(UserDictionaryCommandStatus::INVALID_POS_TYPE,
            UserDictionaryUtil::ValidateEntry(entry));
}

TEST(UserDictionaryUtilTest, ValidateDictionaryName) {
  EXPECT_EQ(UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY,
            UserDictionaryUtil::ValidateDictionaryName(
                user_dictionary::UserDictionaryStorage::default_instance(),
                ""));

  EXPECT_EQ(UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG,
            UserDictionaryUtil::ValidateDictionaryName(
                user_dictionary::UserDictionaryStorage::default_instance(),
                string(500, 'a')));

  EXPECT_EQ(UserDictionaryCommandStatus
                ::DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER,
            UserDictionaryUtil::ValidateDictionaryName(
                user_dictionary::UserDictionaryStorage::default_instance(),
                "a\nbc"));

  user_dictionary::UserDictionaryStorage storage;
  storage.add_dictionaries()->set_name("abc");
  EXPECT_EQ(UserDictionaryCommandStatus::DICTIONARY_NAME_DUPLICATED,
            UserDictionaryUtil::ValidateDictionaryName(
                storage, "abc"));
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

TEST(UserDictioanryUtilTest, IsSyncDictionaryFull) {
  UserDictionary dictionary;
  dictionary.set_syncable(true);

  for (int i = 0; i < UserDictionaryUtil::max_sync_entry_size(); ++i) {
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
  EXPECT_TRUE(UserDictionaryUtil::GetUserDictionaryById(storage, -1) == NULL);
}

TEST(UserDictionaryUtilTest, GetMutableUserDictionaryById) {
  user_dictionary::UserDictionaryStorage storage;
  storage.add_dictionaries()->set_id(1);
  storage.add_dictionaries()->set_id(2);

  EXPECT_TRUE(UserDictionaryUtil::GetMutableUserDictionaryById(&storage, 1) ==
              storage.mutable_dictionaries(0));
  EXPECT_TRUE(UserDictionaryUtil::GetMutableUserDictionaryById(&storage, 2) ==
              storage.mutable_dictionaries(1));
  EXPECT_TRUE(
      UserDictionaryUtil::GetMutableUserDictionaryById(&storage, -1) == NULL);
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

TEST(UserDictionaryUtilTest, ResolveUnknownFieldSet1) {
  user_dictionary::UserDictionaryStorage storage;
  {
    UserDictionary *dictionary = storage.add_dictionaries();
    UserDictionary::Entry *entry = dictionary->add_entries();
    entry->set_key("key");
    entry->set_value("value");
    entry->set_pos(UserDictionary::NOUN);
    entry->set_comment("comment");
  }

  // Do nothing if any entry doesn't have unknown field set.
  UserDictionaryUtil::ResolveUnknownFieldSet(&storage);
  EXPECT_PROTO_EQ(
      "dictionaries <\n"
      "  entries <\n"
      "    key: \"key\"\n"
      "    value: \"value\"\n"
      "    pos: NOUN\n"
      "    comment: \"comment\"\n"
      "  >\n"
      ">\n",
      storage);
}


TEST(UserDictionaryUtilTest, ResolveUnknownFieldSet2) {
  user_dictionary::UserDictionaryStorage storage;
  {
    UserDictionary *dictionary = storage.add_dictionaries();
    UserDictionary::Entry *entry = dictionary->add_entries();
    entry->set_key("key");
    entry->set_value("value");
    entry->set_comment("comment");

    UnknownFieldSet *unknown_field_set = entry->mutable_unknown_fields();
    unknown_field_set->AddVarint(3, 1);
  }

  // Migrate old enum formatted pos to the actual Entry::pos.
  UserDictionaryUtil::ResolveUnknownFieldSet(&storage);
  EXPECT_PROTO_EQ(
      "dictionaries <\n"
      "  entries <\n"
      "    key: \"key\"\n"
      "    value: \"value\"\n"
      "    pos: NOUN\n"
      "    comment: \"comment\"\n"
      "  >\n"
      ">\n",
      storage);
}

TEST(UserDictionaryUtilTest, ResolveUnknownFieldSet3) {
  user_dictionary::UserDictionaryStorage storage;
  {
    UserDictionary *dictionary = storage.add_dictionaries();
    UserDictionary::Entry *entry = dictionary->add_entries();
    entry->set_key("key");
    entry->set_value("value");
    entry->set_comment("comment");

    UnknownFieldSet *unknown_field_set = entry->mutable_unknown_fields();
    // "名詞"
    unknown_field_set->AddLengthDelimited(3, "\xE5\x90\x8D\xE8\xA9\x9E");
  }

  // Migrate old string formatted pos to the actual Entry::pos.
  UserDictionaryUtil::ResolveUnknownFieldSet(&storage);
  EXPECT_PROTO_EQ(
      "dictionaries <\n"
      "  entries <\n"
      "    key: \"key\"\n"
      "    value: \"value\"\n"
      "    pos: NOUN\n"
      "    comment: \"comment\"\n"
      "  >\n"
      ">\n",
      storage);
}

TEST(UserDictionaryUtilTest, CreateDictionary) {
  user_dictionary::UserDictionaryStorage storage;
  uint64 dictionary_id;

  // Check dictionary validity.
  EXPECT_EQ(UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY,
            UserDictionaryUtil::CreateDictionary(
                &storage, "", &dictionary_id));

  // Check the limit of the number of dictionaries.
  storage.Clear();
  for (int i = 0; i < UserDictionaryUtil::max_dictionary_size(); ++i) {
    storage.add_dictionaries();
  }

  EXPECT_EQ(UserDictionaryCommandStatus::DICTIONARY_SIZE_LIMIT_EXCEEDED,
            UserDictionaryUtil::CreateDictionary(
                &storage, "new dictionary", &dictionary_id));

  storage.Clear();
  EXPECT_EQ(UserDictionaryCommandStatus::UNKNOWN_ERROR,
            UserDictionaryUtil::CreateDictionary(
                &storage, "new dictionary", NULL));

  ASSERT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            UserDictionaryUtil::CreateDictionary(
                &storage, "new dictionary", &dictionary_id));

  EXPECT_PROTO_PEQ("dictionaries <\n"
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
  ASSERT_TRUE(UserDictionaryUtil::DeleteDictionary(
      &storage, 1, &original_index, NULL));
  EXPECT_EQ(0, original_index);
  ASSERT_EQ(1, storage.dictionaries_size());
  EXPECT_EQ(2, storage.dictionaries(0).id());

  // Deletion for unknown dictionary should fail.
  storage.Clear();
  storage.add_dictionaries()->set_id(1);
  storage.add_dictionaries()->set_id(2);
  EXPECT_FALSE(UserDictionaryUtil::DeleteDictionary(
      &storage, 100, NULL, NULL));

  // Deletion for syncable dictionary should fail, too.
  storage.Clear();
  storage.add_dictionaries()->set_id(1);
  storage.add_dictionaries()->set_id(2);
  storage.mutable_dictionaries(0)->set_syncable(true);
  EXPECT_FALSE(UserDictionaryUtil::DeleteDictionary(&storage, 1, NULL, NULL));

  // Keep deleted dictionary.
  storage.Clear();
  storage.add_dictionaries()->set_id(1);
  storage.add_dictionaries()->set_id(2);
  UserDictionary *expected_deleted_dictionary =
      storage.mutable_dictionaries(0);
  UserDictionary *deleted_dictionary;
  EXPECT_TRUE(UserDictionaryUtil::DeleteDictionary(
      &storage, 1, NULL, &deleted_dictionary));
  ASSERT_EQ(1, storage.dictionaries_size());
  EXPECT_EQ(2, storage.dictionaries(0).id());
  EXPECT_TRUE(expected_deleted_dictionary == deleted_dictionary);

  // Delete to avoid memoary leaking.
  delete expected_deleted_dictionary;
}

TEST(UserDictionaryUtilTest, FillDesktopDeprecatedPosField) {
  user_dictionary::UserDictionaryStorage storage;
  {
    UserDictionary *dictionary = storage.add_dictionaries();
    UserDictionary::Entry *entry = dictionary->add_entries();
    entry->set_key("key");
    entry->set_value("value");
    entry->set_pos(UserDictionary::NOUN);
    entry->set_comment("comment");
  }

  UserDictionaryUtil::FillDesktopDeprecatedPosField(&storage);
  ASSERT_EQ(1, storage.dictionaries_size());
  const UserDictionary &dictionary = storage.dictionaries(0);
  ASSERT_EQ(1, dictionary.entries_size());
  const UserDictionary::Entry &entry = dictionary.entries(0);
  EXPECT_EQ("key", entry.key());
  EXPECT_EQ("value", entry.value());
  EXPECT_EQ(UserDictionary::NOUN, entry.pos());
  EXPECT_EQ("comment", entry.comment());
  const UnknownFieldSet &unknown_field_set = entry.unknown_fields();
  ASSERT_EQ(1, unknown_field_set.field_count());
  const UnknownField &unknown_field = unknown_field_set.field(0);
  EXPECT_EQ(3, unknown_field.number());
  EXPECT_EQ(UnknownField::TYPE_LENGTH_DELIMITED, unknown_field.type());
  // "名詞"
  EXPECT_EQ("\xE5\x90\x8D\xE8\xA9\x9E", unknown_field.length_delimited());
}

TEST(UserDictionaryUtilTest, FillDesktopDeprecatedPosFieldEmptyPos) {
  user_dictionary::UserDictionaryStorage storage;
  {
    UserDictionary *dictionary = storage.add_dictionaries();
    UserDictionary::Entry *entry = dictionary->add_entries();
    entry->set_key("key");
    entry->set_value("value");
    // Don't add pos.
    entry->set_comment("comment");
  }

  UserDictionaryUtil::FillDesktopDeprecatedPosField(&storage);
  ASSERT_EQ(1, storage.dictionaries_size());
  const UserDictionary &dictionary = storage.dictionaries(0);
  ASSERT_EQ(1, dictionary.entries_size());
  const UserDictionary::Entry &entry = dictionary.entries(0);
  EXPECT_EQ("key", entry.key());
  EXPECT_EQ("value", entry.value());
  EXPECT_EQ(UserDictionary::NOUN, entry.pos());
  EXPECT_EQ("comment", entry.comment());
  const UnknownFieldSet &unknown_field_set = entry.unknown_fields();
  EXPECT_EQ(0, unknown_field_set.field_count());
}

}  // namespace mozc

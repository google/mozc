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
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "testing/gunit.h"
#include "testing/testing_util.h"

namespace mozc {
namespace {

using ::mozc::user_dictionary::UserDictionary;
using ::mozc::user_dictionary::UserDictionaryCommandStatus;

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
  EXPECT_EQ(UserDictionaryUtil::NormalizeReading("アイウヴヮ"), "あいうゔゎ");
  EXPECT_EQ(UserDictionaryUtil::NormalizeReading("ｱｲｳｬ"), "あいうゃ");
  EXPECT_EQ(UserDictionaryUtil::NormalizeReading("ＡＢａｂ０１＠＆＝｜"),
            "ABab01@&=|");
  EXPECT_EQ(UserDictionaryUtil::NormalizeReading("｡｢｣､･"), "。「」、・");
}

struct UserDictionaryEntryData {
  absl::string_view key;
  absl::string_view value;
  UserDictionary::PosType pos;
  absl::string_view comment;
};

void ConvertUserDictionaryEntry(const UserDictionaryEntryData& input,
                                UserDictionary::Entry* output) {
  output->set_key(input.key);
  output->set_value(input.value);
  output->set_pos(input.pos);
  output->set_comment(input.comment);
}

TEST(UserDictionaryUtilTest, TestSanitizeEntry) {
  constexpr UserDictionaryEntryData kGoldenData = {
      "abc",
      "abc",
      UserDictionary::NOUN,
      "abc",
  };

  UserDictionary::Entry golden, input;
  ConvertUserDictionaryEntry(kGoldenData, &golden);

  {
    constexpr UserDictionaryEntryData kInputData = {
        "abc", "abc", UserDictionary::NOUN, "abc"};
    ConvertUserDictionaryEntry(kInputData, &input);
    EXPECT_FALSE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_PROTO_EQ(golden, input);
  }

  {
    constexpr UserDictionaryEntryData kInputData = {
        "ab\tc", "abc", UserDictionary::NOUN, "abc"};
    ConvertUserDictionaryEntry(kInputData, &input);
    EXPECT_TRUE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_PROTO_EQ(golden, input);
  }

  {
    constexpr UserDictionaryEntryData kInputData = {
        "abc", "ab\tc", UserDictionary::NOUN, "ab\tc"};
    ConvertUserDictionaryEntry(kInputData, &input);
    EXPECT_TRUE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_PROTO_EQ(golden, input);
  }

  {
    constexpr UserDictionaryEntryData kInputData = {
        "ab\tc", "ab\tc", UserDictionary::NOUN, "ab\tc"};
    ConvertUserDictionaryEntry(kInputData, &input);
    EXPECT_TRUE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_PROTO_EQ(golden, input);
  }
}

TEST(UserDictionaryUtilTest, TestSanitize) {
  std::string str(10, '\t');
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 5));
  EXPECT_EQ(str, "");

  str = "ab\tc";
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 10));
  EXPECT_EQ(str, "abc");

  str = "かしゆか";
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 3));
  EXPECT_EQ(str, "か");

  str = "かしゆか";
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 4));
  EXPECT_EQ(str, "か");

  str = "かしゆか";
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 5));
  EXPECT_EQ(str, "か");

  str = "かしゆか";
  EXPECT_TRUE(UserDictionaryUtil::Sanitize(&str, 6));
  EXPECT_EQ(str, "かし");

  str = "かしゆか";
  EXPECT_FALSE(UserDictionaryUtil::Sanitize(&str, 100));
  EXPECT_EQ(str, "かしゆか");
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
  entry.set_key("あ\xE3\x84う");  // Invalid UTF-8. "い" is "E3 81 84".
  EXPECT_EQ(UserDictionaryCommandStatus::READING_CONTAINS_INVALID_CHARACTER,
            UserDictionaryUtil::ValidateEntry(entry));

  entry = base_entry;
  entry.set_key("ふ頭");  // Non-Hiragana chararcters are also acceptable.
  EXPECT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
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
  entry.set_value("あ\x81\x84う");  // Invalid UTF-8. "い" is "E3 81 84".
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
  entry.set_comment("あ\xE3う");  // Invalid UTF-8. "い" is "E3 81 84".
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

  EXPECT_EQ(UserDictionaryUtil::GetUserDictionaryById(storage, 1),
            &storage.dictionaries(0));
  EXPECT_EQ(UserDictionaryUtil::GetUserDictionaryById(storage, 2),
            &storage.dictionaries(1));
  EXPECT_EQ(UserDictionaryUtil::GetUserDictionaryById(storage, -1), nullptr);
}

TEST(UserDictionaryUtilTest, GetMutableUserDictionaryById) {
  user_dictionary::UserDictionaryStorage storage;
  storage.add_dictionaries()->set_id(1);
  storage.add_dictionaries()->set_id(2);

  EXPECT_EQ(UserDictionaryUtil::GetMutableUserDictionaryById(&storage, 1),
            storage.mutable_dictionaries(0));
  EXPECT_EQ(UserDictionaryUtil::GetMutableUserDictionaryById(&storage, 2),
            storage.mutable_dictionaries(1));
  EXPECT_EQ(UserDictionaryUtil::GetMutableUserDictionaryById(&storage, -1),
            nullptr);
}

TEST(UserDictionaryUtilTest, GetUserDictionaryIndexById) {
  user_dictionary::UserDictionaryStorage storage;
  storage.add_dictionaries()->set_id(1);
  storage.add_dictionaries()->set_id(2);

  EXPECT_EQ(UserDictionaryUtil::GetUserDictionaryIndexById(storage, 1), 0);
  EXPECT_EQ(UserDictionaryUtil::GetUserDictionaryIndexById(storage, 2), 1);

  // Return -1 for a failing case.
  EXPECT_EQ(UserDictionaryUtil::GetUserDictionaryIndexById(storage, -1), -1);
}

TEST(UserDictionaryUtilTest, ToPosType) {
  EXPECT_EQ(UserDictionaryUtil::ToPosType("品詞なし"), UserDictionary::NO_POS);
  EXPECT_EQ(UserDictionaryUtil::ToPosType("サジェストのみ"),
            UserDictionary::SUGGESTION_ONLY);
  EXPECT_EQ(UserDictionaryUtil::ToPosType("動詞ワ行五段"),
            UserDictionary::WA_GROUP1_VERB);
  EXPECT_EQ(UserDictionaryUtil::ToPosType("抑制単語"),
            UserDictionary::SUPPRESSION_WORD);
}

TEST(UserDictionaryUtilTest, GetStringPosType) {
  EXPECT_EQ(UserDictionaryUtil::GetStringPosType(UserDictionary::NO_POS),
            "品詞なし");
  EXPECT_EQ(
      UserDictionaryUtil::GetStringPosType(UserDictionary::SUGGESTION_ONLY),
      "サジェストのみ");
  EXPECT_EQ(
      UserDictionaryUtil::GetStringPosType(UserDictionary::WA_GROUP1_VERB),
      "動詞ワ行五段");
  EXPECT_EQ(
      UserDictionaryUtil::GetStringPosType(UserDictionary::SUPPRESSION_WORD),
      "抑制単語");
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

  EXPECT_EQ(UserDictionaryUtil::CreateDictionary(&storage, "new dictionary",
                                                 &dictionary_id),
            UserDictionaryCommandStatus::DICTIONARY_SIZE_LIMIT_EXCEEDED);

  storage.Clear();
  EXPECT_EQ(
      UserDictionaryUtil::CreateDictionary(&storage, "new dictionary", nullptr),
      UserDictionaryCommandStatus::UNKNOWN_ERROR);

  ASSERT_EQ(UserDictionaryUtil::CreateDictionary(&storage, "new dictionary",
                                                 &dictionary_id),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  EXPECT_PROTO_PEQ(
      "dictionaries <\n"
      "  name: \"new dictionary\"\n"
      ">\n",
      storage);
  EXPECT_EQ(storage.dictionaries(0).id(), dictionary_id);
}

TEST(UserDictionaryUtilTest, DeleteDictionary) {
  user_dictionary::UserDictionaryStorage storage;
  storage.add_dictionaries()->set_id(1);
  storage.add_dictionaries()->set_id(2);

  // Simplest deleting case.
  int original_index;
  ASSERT_TRUE(UserDictionaryUtil::DeleteDictionary(&storage, 1, &original_index,
                                                   nullptr));
  EXPECT_EQ(original_index, 0);
  ASSERT_EQ(storage.dictionaries_size(), 1);
  EXPECT_EQ(storage.dictionaries(0).id(), 2);

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
  std::unique_ptr<UserDictionary> deleted_dictionary;
  EXPECT_TRUE(UserDictionaryUtil::DeleteDictionary(&storage, 1, nullptr,
                                                   &deleted_dictionary));
  ASSERT_EQ(storage.dictionaries_size(), 1);
  EXPECT_EQ(storage.dictionaries(0).id(), 2);
  EXPECT_EQ(deleted_dictionary->id(), 1);
}

}  // namespace
}  // namespace mozc

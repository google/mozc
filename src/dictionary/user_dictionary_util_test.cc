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

#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/testing_util.h"

namespace mozc {
namespace user_dictionary {
namespace {

using ::mozc::user_dictionary::UserDictionary;

TEST(UserDictionaryUtilTest, TestIsValidReading) {
  EXPECT_TRUE(IsValidReading("ABYZabyz0189"));
  EXPECT_TRUE(IsValidReading("〜「」"));
  EXPECT_TRUE(IsValidReading("あいうわをんゔ"));
  EXPECT_TRUE(IsValidReading("アイウワヲンヴ"));
  EXPECT_FALSE(IsValidReading("水雲"));

  // COMBINING KATAKANA-HIRAGANA VOICED/SEMI-VOICED SOUND MARK (u3099, u309A)
  EXPECT_FALSE(IsValidReading("゙゚"));

  // KATAKANA-HIRAGANA VOICED/SEMI-VOICED SOUND MARK (u309B, u309C)
  EXPECT_TRUE(IsValidReading("゛゜"));

  EXPECT_FALSE(IsValidReading("𠮷"));
  EXPECT_FALSE(IsValidReading("😁"));
  EXPECT_FALSE(IsValidReading("ヷ"));
  EXPECT_FALSE(IsValidReading("ヺ"));
  EXPECT_TRUE(IsValidReading("。「」、・"));
}

TEST(UserDictionaryUtilTest, TestNormalizeReading) {
  EXPECT_EQ(NormalizeReading("アイウヴヮ"), "あいうゔゎ");
  EXPECT_EQ(NormalizeReading("ｱｲｳｬ"), "あいうゃ");
  EXPECT_EQ(NormalizeReading("ＡＢａｂ０１＠＆＝｜"), "ABab01@&=|");
  EXPECT_EQ(NormalizeReading("｡｢｣､･"), "。「」、・");
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
    EXPECT_FALSE(SanitizeEntry(&input));
    EXPECT_PROTO_EQ(golden, input);
  }

  {
    constexpr UserDictionaryEntryData kInputData = {
        "ab\tc", "abc", UserDictionary::NOUN, "abc"};
    ConvertUserDictionaryEntry(kInputData, &input);
    EXPECT_TRUE(SanitizeEntry(&input));
    EXPECT_PROTO_EQ(golden, input);
  }

  {
    constexpr UserDictionaryEntryData kInputData = {
        "abc", "ab\tc", UserDictionary::NOUN, "ab\tc"};
    ConvertUserDictionaryEntry(kInputData, &input);
    EXPECT_TRUE(SanitizeEntry(&input));
    EXPECT_PROTO_EQ(golden, input);
  }

  {
    constexpr UserDictionaryEntryData kInputData = {
        "ab\tc", "ab\tc", UserDictionary::NOUN, "ab\tc"};
    ConvertUserDictionaryEntry(kInputData, &input);
    EXPECT_TRUE(SanitizeEntry(&input));
    EXPECT_PROTO_EQ(golden, input);
  }
}

TEST(UserDictionaryUtilTest, TestSanitize) {
  std::string str(10, '\t');
  EXPECT_TRUE(Sanitize(&str, 5));
  EXPECT_EQ(str, "");

  str = "ab\tc";
  EXPECT_TRUE(Sanitize(&str, 10));
  EXPECT_EQ(str, "abc");

  str = "かしゆか";
  EXPECT_TRUE(Sanitize(&str, 3));
  EXPECT_EQ(str, "か");

  str = "かしゆか";
  EXPECT_TRUE(Sanitize(&str, 4));
  EXPECT_EQ(str, "か");

  str = "かしゆか";
  EXPECT_TRUE(Sanitize(&str, 5));
  EXPECT_EQ(str, "か");

  str = "かしゆか";
  EXPECT_TRUE(Sanitize(&str, 6));
  EXPECT_EQ(str, "かし");

  str = "かしゆか";
  EXPECT_FALSE(Sanitize(&str, 100));
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
  EXPECT_OK(ValidateEntry(entry));

  entry = base_entry;
  entry.clear_key();
  EXPECT_EQ(ExtendedErrorCode::READING_EMPTY, ValidateEntry(entry).raw_code());

  entry = base_entry;
  entry.set_key(std::string(500, 'a'));
  EXPECT_EQ(ExtendedErrorCode::READING_TOO_LONG,
            ValidateEntry(entry).raw_code());

  entry = base_entry;
  entry.set_key("a\nb");
  EXPECT_EQ(ExtendedErrorCode::READING_CONTAINS_INVALID_CHARACTER,
            ValidateEntry(entry).raw_code());

  entry = base_entry;
  entry.set_key("あ\xE3\x84う");  // Invalid UTF-8. "い" is "E3 81 84".
  EXPECT_EQ(ExtendedErrorCode::READING_CONTAINS_INVALID_CHARACTER,
            ValidateEntry(entry).raw_code());

  entry = base_entry;
  entry.set_key("ふ頭");  // Non-Hiragana characters are also acceptable.
  EXPECT_OK(ValidateEntry(entry));

  entry = base_entry;
  entry.clear_value();
  EXPECT_EQ(ExtendedErrorCode::WORD_EMPTY, ValidateEntry(entry).raw_code());

  entry = base_entry;
  entry.set_value(std::string(500, 'a'));
  EXPECT_EQ(ExtendedErrorCode::WORD_TOO_LONG, ValidateEntry(entry).raw_code());

  entry = base_entry;
  entry.set_value("a\nb");
  EXPECT_EQ(ExtendedErrorCode::WORD_CONTAINS_INVALID_CHARACTER,
            ValidateEntry(entry).raw_code());

  entry = base_entry;
  entry.set_value("あ\x81\x84う");  // Invalid UTF-8. "い" is "E3 81 84".
  EXPECT_EQ(ExtendedErrorCode::WORD_CONTAINS_INVALID_CHARACTER,
            ValidateEntry(entry).raw_code());

  entry = base_entry;
  entry.clear_comment();
  EXPECT_OK(ValidateEntry(entry));

  entry = base_entry;
  entry.set_comment(std::string(500, 'a'));
  EXPECT_EQ(ExtendedErrorCode::COMMENT_TOO_LONG,
            ValidateEntry(entry).raw_code());

  entry = base_entry;
  entry.set_comment("a\nb");
  EXPECT_EQ(ExtendedErrorCode::COMMENT_CONTAINS_INVALID_CHARACTER,
            ValidateEntry(entry).raw_code());

  entry = base_entry;
  entry.set_comment("あ\xE3う");  // Invalid UTF-8. "い" is "E3 81 84".
  EXPECT_EQ(ExtendedErrorCode::COMMENT_CONTAINS_INVALID_CHARACTER,
            ValidateEntry(entry).raw_code());

  entry = base_entry;
  entry.clear_pos();
  EXPECT_EQ(ExtendedErrorCode::INVALID_POS_TYPE,
            ValidateEntry(entry).raw_code());
}

TEST(UserDictionaryUtilTest, ValidateDictionaryName) {
  EXPECT_EQ(ExtendedErrorCode::DICTIONARY_NAME_EMPTY,
            ValidateDictionaryName("").raw_code());

  EXPECT_EQ(ExtendedErrorCode::DICTIONARY_NAME_TOO_LONG,
            ValidateDictionaryName(std::string(500, 'a')).raw_code());

  EXPECT_EQ(ExtendedErrorCode::DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER,
            ValidateDictionaryName("a\nbc").raw_code());
}

}  // namespace
}  // namespace user_dictionary
}  // namespace mozc

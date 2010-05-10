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

#include "base/util.h"
#include "dictionary/user_dictionary_util.h"

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {

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
  const char *pos;
  const char *comment;
};

void ConvertUserDictionaryEntry(
    const UserDictionaryEntryData &input,
    UserDictionaryStorage::UserDictionaryEntry *output) {
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
    "abc",
    "abc",
  };

  UserDictionaryStorage::UserDictionaryEntry golden, input;
  ConvertUserDictionaryEntry(golden_data, &golden);

  {
    UserDictionaryEntryData input_data = { "abc", "abc", "abc", "abc" };
    ConvertUserDictionaryEntry(input_data, &input);
    EXPECT_FALSE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_EQ(golden.DebugString(), input.DebugString());
  }

  {
    UserDictionaryEntryData input_data = { "ab\tc", "abc", "abc", "abc" };
    ConvertUserDictionaryEntry(input_data, &input);
    EXPECT_TRUE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_EQ(golden.DebugString(), input.DebugString());
  }

  {
    UserDictionaryEntryData input_data = { "abc", "ab\tc", "abc", "ab\tc" };
    ConvertUserDictionaryEntry(input_data, &input);
    EXPECT_TRUE(UserDictionaryUtil::SanitizeEntry(&input));
    EXPECT_EQ(golden.DebugString(), input.DebugString());
  }

  {
    UserDictionaryEntryData input_data = { "ab\tc", "ab\tc", "ab\tc", "ab\tc" };
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
}  // namespace mozc

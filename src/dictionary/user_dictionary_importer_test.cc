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

#include "dictionary/user_dictionary_importer.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "dictionary/user_dictionary_storage.h"
#include "testing/gunit.h"

namespace mozc {

namespace {

class TestInputIterator
    : public UserDictionaryImporter::InputIteratorInterface {
 public:
  TestInputIterator() : index_(0), is_available_(false), entries_(nullptr) {}

  bool IsAvailable() const override { return is_available_; }

  bool Next(UserDictionaryImporter::RawEntry *entry) override {
    if (!is_available_) {
      return false;
    }
    if (index_ >= entries_->size()) {
      return false;
    }
    entry->key = (*entries_)[index_].key;
    entry->value = (*entries_)[index_].value;
    entry->pos = (*entries_)[index_].pos;
    entry->comment = (*entries_)[index_].comment;
    ++index_;
    return true;
  }

  void set_entries(
      const std::vector<UserDictionaryImporter::RawEntry> *entries) {
    entries_ = entries;
  }

  void set_available(bool is_available) { is_available_ = is_available; }

 public:
  int index_;
  bool is_available_;
  const std::vector<UserDictionaryImporter::RawEntry> *entries_;
};

}  // namespace

TEST(UserDictionaryImporter, ImportFromNormalTextTest) {
  constexpr char kInput[] =
      "きょうと\t京都\t名詞\n"
      "おおさか\t大阪\t地名\n"
      "とうきょう\t東京\t地名\tコメント\n"
      "すずき\t鈴木\t人名\n"
      "あめりか\tアメリカ\t地名:en";

  UserDictionaryImporter::StringTextLineIterator iter(kInput);
  UserDictionaryStorage::UserDictionary user_dic;

  EXPECT_EQ(UserDictionaryImporter::ImportFromTextLineIterator(
                UserDictionaryImporter::MOZC, &iter, &user_dic),
            UserDictionaryImporter::IMPORT_NO_ERROR);

  ASSERT_EQ(user_dic.entries_size(), 5);

  EXPECT_EQ(user_dic.entries(0).key(), "きょうと");
  EXPECT_EQ(user_dic.entries(0).value(), "京都");
  EXPECT_EQ(user_dic.entries(0).pos(), user_dictionary::UserDictionary::NOUN);
  EXPECT_EQ(user_dic.entries(0).comment(), "");
  EXPECT_EQ(user_dic.entries(0).locale(), "");

  EXPECT_EQ(user_dic.entries(1).key(), "おおさか");
  EXPECT_EQ(user_dic.entries(1).value(), "大阪");
  EXPECT_EQ(user_dic.entries(1).pos(),
            user_dictionary::UserDictionary::PLACE_NAME);
  EXPECT_EQ(user_dic.entries(1).comment(), "");
  EXPECT_EQ(user_dic.entries(1).locale(), "");

  EXPECT_EQ(user_dic.entries(2).key(), "とうきょう");
  EXPECT_EQ(user_dic.entries(2).value(), "東京");
  EXPECT_EQ(user_dic.entries(2).pos(),
            user_dictionary::UserDictionary::PLACE_NAME);
  EXPECT_EQ(user_dic.entries(2).comment(), "コメント");
  EXPECT_EQ(user_dic.entries(2).locale(), "");

  EXPECT_EQ(user_dic.entries(3).key(), "すずき");
  EXPECT_EQ(user_dic.entries(3).value(), "鈴木");
  EXPECT_EQ(user_dic.entries(3).pos(),
            user_dictionary::UserDictionary::PERSONAL_NAME);
  EXPECT_EQ(user_dic.entries(3).comment(), "");
  EXPECT_EQ(user_dic.entries(3).locale(), "");

  EXPECT_EQ(user_dic.entries(4).key(), "あめりか");
  EXPECT_EQ(user_dic.entries(4).value(), "アメリカ");
  EXPECT_EQ(user_dic.entries(4).pos(),
            user_dictionary::UserDictionary::PLACE_NAME);
  EXPECT_EQ(user_dic.entries(4).comment(), "");
  EXPECT_EQ(user_dic.entries(4).locale(), "en");
}

TEST(UserDictionaryImporter, ImportFromKotoeriTextTest) {
  constexpr char kInput[] =
      "\"きょうと\","
      "\"京都\",\"名詞\"\n"
      "\"おおさか\","
      "\"大阪\",\"地名\"\n"
      "// last line";
  {
    UserDictionaryImporter::StringTextLineIterator iter(kInput);
    UserDictionaryStorage::UserDictionary user_dic;

    EXPECT_EQ(UserDictionaryImporter::ImportFromTextLineIterator(
                  UserDictionaryImporter::MOZC, &iter, &user_dic),
              UserDictionaryImporter::IMPORT_NOT_SUPPORTED);

    EXPECT_EQ(user_dic.entries_size(), 0);
  }
  {
    UserDictionaryImporter::StringTextLineIterator iter(kInput);
    UserDictionaryStorage::UserDictionary user_dic;

    EXPECT_EQ(UserDictionaryImporter::ImportFromTextLineIterator(
                  UserDictionaryImporter::KOTOERI, &iter, &user_dic),
              UserDictionaryImporter::IMPORT_NO_ERROR);

    ASSERT_EQ(user_dic.entries_size(), 2);

    EXPECT_EQ(user_dic.entries(0).key(), "きょうと");
    EXPECT_EQ(user_dic.entries(0).value(), "京都");
    EXPECT_EQ(user_dic.entries(0).pos(), user_dictionary::UserDictionary::NOUN);

    EXPECT_EQ(user_dic.entries(1).key(), "おおさか");
    EXPECT_EQ(user_dic.entries(1).value(), "大阪");
    EXPECT_EQ(user_dic.entries(1).pos(),
              user_dictionary::UserDictionary::PLACE_NAME);
  }
}

TEST(UserDictionaryImporter, ImportSpecialPosTagTest) {
  constexpr char kInput[] =
      "きょうと\t京都\tサジェストのみ\n"
      "おおさか\t大阪\t短縮よみ\n"
      "すずき\t鈴木\t品詞なし\n";
  {
    UserDictionaryImporter::StringTextLineIterator iter(kInput);
    UserDictionaryStorage::UserDictionary user_dic;

    EXPECT_EQ(UserDictionaryImporter::ImportFromTextLineIterator(
                  UserDictionaryImporter::MOZC, &iter, &user_dic),
              UserDictionaryImporter::IMPORT_NO_ERROR);

    ASSERT_EQ(user_dic.entries_size(), 3);

    EXPECT_EQ(user_dic.entries(0).key(), "きょうと");
    EXPECT_EQ(user_dic.entries(0).value(), "京都");
    EXPECT_EQ(user_dic.entries(0).pos(),
              user_dictionary::UserDictionary::SUGGESTION_ONLY);

    EXPECT_EQ(user_dic.entries(1).key(), "おおさか");
    EXPECT_EQ(user_dic.entries(1).value(), "大阪");
    EXPECT_EQ(user_dic.entries(1).pos(),
              user_dictionary::UserDictionary::ABBREVIATION);

    EXPECT_EQ(user_dic.entries(2).key(), "すずき");
    EXPECT_EQ(user_dic.entries(2).value(), "鈴木");
    EXPECT_EQ(user_dic.entries(2).pos(),
              user_dictionary::UserDictionary::NO_POS);
  }
}

TEST(UserDictionaryImporter, ImportFromCommentTextTest) {
  constexpr char kInput[] =
      "きょうと\t京都\t名詞\n"
      "!おおさか\t大阪\t地名\n"
      "\n"
      "#とうきょう\t東京\t地名\tコメント\n"
      "すずき\t鈴木\t人名\n";
  {
    const std::string kMsImeInput(std::string("!Microsoft IME\n") + kInput);
    UserDictionaryImporter::StringTextLineIterator iter(kMsImeInput);
    UserDictionaryStorage::UserDictionary user_dic;

    EXPECT_EQ(UserDictionaryImporter::ImportFromTextLineIterator(
                  UserDictionaryImporter::MSIME, &iter, &user_dic),
              UserDictionaryImporter::IMPORT_NO_ERROR);

    ASSERT_EQ(user_dic.entries_size(), 3);

    EXPECT_EQ(user_dic.entries(0).key(), "きょうと");
    EXPECT_EQ(user_dic.entries(0).value(), "京都");
    EXPECT_EQ(user_dic.entries(0).pos(), user_dictionary::UserDictionary::NOUN);

    EXPECT_EQ(user_dic.entries(1).key(), "#とうきょう");
    EXPECT_EQ(user_dic.entries(1).value(), "東京");
    EXPECT_EQ(user_dic.entries(1).pos(),
              user_dictionary::UserDictionary::PLACE_NAME);

    EXPECT_EQ(user_dic.entries(2).key(), "すずき");
    EXPECT_EQ(user_dic.entries(2).value(), "鈴木");
    EXPECT_EQ(user_dic.entries(2).pos(),
              user_dictionary::UserDictionary::PERSONAL_NAME);
  }
  {
    UserDictionaryImporter::StringTextLineIterator iter(kInput);
    UserDictionaryStorage::UserDictionary user_dic;

    EXPECT_EQ(UserDictionaryImporter::ImportFromTextLineIterator(
                  UserDictionaryImporter::MOZC, &iter, &user_dic),
              UserDictionaryImporter::IMPORT_NO_ERROR);

    ASSERT_EQ(user_dic.entries_size(), 3);

    EXPECT_EQ(user_dic.entries(0).key(), "きょうと");
    EXPECT_EQ(user_dic.entries(0).value(), "京都");
    EXPECT_EQ(user_dic.entries(0).pos(), user_dictionary::UserDictionary::NOUN);

    EXPECT_EQ(user_dic.entries(1).key(), "!おおさか");
    EXPECT_EQ(user_dic.entries(1).value(), "大阪");
    EXPECT_EQ(user_dic.entries(1).pos(),
              user_dictionary::UserDictionary::PLACE_NAME);

    EXPECT_EQ(user_dic.entries(2).key(), "すずき");
    EXPECT_EQ(user_dic.entries(2).value(), "鈴木");
    EXPECT_EQ(user_dic.entries(2).pos(),
              user_dictionary::UserDictionary::PERSONAL_NAME);
  }
}

TEST(UserDictionaryImporter, ImportFromInvalidTextTest) {
  constexpr char kInput[] =
      "a"
      "\n"
      "東京\t\t地名\tコメント\n"
      "すずき\t鈴木\t人名\n";

  UserDictionaryImporter::StringTextLineIterator iter(kInput);
  UserDictionaryStorage::UserDictionary user_dic;

  EXPECT_EQ(UserDictionaryImporter::ImportFromTextLineIterator(
                UserDictionaryImporter::MOZC, &iter, &user_dic),
            UserDictionaryImporter::IMPORT_INVALID_ENTRIES);

  ASSERT_EQ(user_dic.entries_size(), 1);

  EXPECT_EQ(user_dic.entries(0).key(), "すずき");
  EXPECT_EQ(user_dic.entries(0).value(), "鈴木");
  EXPECT_EQ(user_dic.entries(0).pos(),
            user_dictionary::UserDictionary::PERSONAL_NAME);
}

TEST(UserDictionaryImporter, ImportFromIteratorInvalidTest) {
  TestInputIterator iter;
  UserDictionaryStorage::UserDictionary user_dic;
  EXPECT_FALSE(iter.IsAvailable());
  EXPECT_EQ(UserDictionaryImporter::ImportFromIterator(&iter, &user_dic),
            UserDictionaryImporter::IMPORT_NO_ERROR);
}

TEST(UserDictionaryImporter, ImportFromIteratorAlreadyFullTest) {
  TestInputIterator iter;
  iter.set_available(true);
  UserDictionaryStorage::UserDictionary user_dic;

  std::vector<UserDictionaryImporter::RawEntry> entries;
  {
    UserDictionaryImporter::RawEntry entry;
    entry.key = "aa";
    entry.value = "aa";
    entry.pos = "名詞";
    entries.push_back(entry);
  }

  for (int i = 0; i < UserDictionaryStorage::max_entry_size(); ++i) {
    user_dic.add_entries();
  }

  iter.set_available(true);
  iter.set_entries(&entries);

  EXPECT_EQ(user_dic.entries_size(), UserDictionaryStorage::max_entry_size());

  EXPECT_TRUE(iter.IsAvailable());
  EXPECT_EQ(UserDictionaryImporter::ImportFromIterator(&iter, &user_dic),
            UserDictionaryImporter::IMPORT_TOO_MANY_WORDS);

  EXPECT_EQ(user_dic.entries_size(), UserDictionaryStorage::max_entry_size());
}

TEST(UserDictionaryImporter, ImportFromIteratorNormalTest) {
  TestInputIterator iter;
  UserDictionaryStorage::UserDictionary user_dic;

  static constexpr size_t kSize[] = {10, 100, 1000, 5000, 12000};
  for (size_t i = 0; i < std::size(kSize); ++i) {
    std::vector<UserDictionaryImporter::RawEntry> entries;
    for (size_t j = 0; j < kSize[i]; ++j) {
      UserDictionaryImporter::RawEntry entry;
      const std::string key("key" + std::to_string(static_cast<uint32_t>(j)));
      const std::string value("value" +
                              std::to_string(static_cast<uint32_t>(j)));
      entry.key = key;
      entry.value = value;
      entry.pos = "名詞";
      entries.push_back(entry);
    }

    iter.set_available(true);
    iter.set_entries(&entries);

    if (kSize[i] <= UserDictionaryStorage::max_entry_size()) {
      EXPECT_EQ(UserDictionaryImporter::ImportFromIterator(&iter, &user_dic),
                UserDictionaryImporter::IMPORT_NO_ERROR);
    } else {
      EXPECT_EQ(UserDictionaryImporter::ImportFromIterator(&iter, &user_dic),
                UserDictionaryImporter::IMPORT_TOO_MANY_WORDS);
    }

    const size_t size =
        std::min(UserDictionaryStorage::max_entry_size(), kSize[i]);
    ASSERT_EQ(user_dic.entries_size(), size);
    for (size_t j = 0; j < size; ++j) {
      EXPECT_EQ(user_dic.entries(j).key(), entries[j].key);
      EXPECT_EQ(user_dic.entries(j).value(), entries[j].value);
      EXPECT_EQ(user_dic.entries(j).pos(),
                user_dictionary::UserDictionary::NOUN);
    }
  }
}

TEST(UserDictionaryImporter, ImportFromIteratorInvalidEntriesTest) {
  TestInputIterator iter;
  UserDictionaryStorage::UserDictionary user_dic;

  static constexpr size_t kSize[] = {10, 100, 1000};
  for (size_t i = 0; i < std::size(kSize); ++i) {
    std::vector<UserDictionaryImporter::RawEntry> entries;
    for (size_t j = 0; j < kSize[i]; ++j) {
      UserDictionaryImporter::RawEntry entry;
      const std::string key("key" + std::to_string(static_cast<uint32_t>(j)));
      const std::string value("value" +
                              std::to_string(static_cast<uint32_t>(j)));
      entry.key = key;
      entry.value = value;
      if (j % 2 == 0) {
        entry.pos = "名詞";
      }
      entries.push_back(entry);
    }

    iter.set_available(true);
    iter.set_entries(&entries);

    EXPECT_EQ(UserDictionaryImporter::ImportFromIterator(&iter, &user_dic),
              UserDictionaryImporter::IMPORT_INVALID_ENTRIES);
    EXPECT_EQ(user_dic.entries_size(), kSize[i] / 2);
  }
}

TEST(UserDictionaryImporter, ImportFromIteratorDupTest) {
  TestInputIterator iter;
  iter.set_available(true);
  UserDictionaryStorage::UserDictionary user_dic;

  {
    UserDictionaryStorage::UserDictionaryEntry *entry = user_dic.add_entries();
    entry->set_key("aa");
    entry->set_value("aa");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
  }

  std::vector<UserDictionaryImporter::RawEntry> entries;

  {
    UserDictionaryImporter::RawEntry entry;
    entry.key = "aa";
    entry.value = "aa";
    entry.pos = "名詞";
    entries.push_back(entry);
  }

  iter.set_entries(&entries);

  EXPECT_EQ(UserDictionaryImporter::ImportFromIterator(&iter, &user_dic),
            UserDictionaryImporter::IMPORT_NO_ERROR);

  EXPECT_EQ(user_dic.entries_size(), 1);

  {
    UserDictionaryImporter::RawEntry entry;
    entry.key = "bb";
    entry.value = "bb";
    entry.pos = "名詞";
    entries.push_back(entry);
  }

  iter.set_entries(&entries);

  EXPECT_EQ(UserDictionaryImporter::ImportFromIterator(&iter, &user_dic),
            UserDictionaryImporter::IMPORT_NO_ERROR);

  EXPECT_EQ(user_dic.entries_size(), 2);

  EXPECT_EQ(UserDictionaryImporter::ImportFromIterator(&iter, &user_dic),
            UserDictionaryImporter::IMPORT_NO_ERROR);

  EXPECT_EQ(user_dic.entries_size(), 2);
}

TEST(UserDictionaryImporter, GuessIMETypeTest) {
  EXPECT_EQ(UserDictionaryImporter::GuessIMEType(""),
            UserDictionaryImporter::NUM_IMES);

  EXPECT_EQ(
      UserDictionaryImporter::GuessIMEType("!Microsoft IME Dictionary Tool"),
      UserDictionaryImporter::MSIME);

  EXPECT_EQ(UserDictionaryImporter::GuessIMEType("!!ATOK_TANGO_TEXT_HEADER_1"),
            UserDictionaryImporter::ATOK);

  EXPECT_EQ(UserDictionaryImporter::GuessIMEType("!!DICUT10"),
            UserDictionaryImporter::NUM_IMES);

  EXPECT_EQ(UserDictionaryImporter::GuessIMEType("!!DICUT"),
            UserDictionaryImporter::NUM_IMES);

  EXPECT_EQ(UserDictionaryImporter::GuessIMEType("!!DICUT11"),
            UserDictionaryImporter::ATOK);

  EXPECT_EQ(UserDictionaryImporter::GuessIMEType("!!DICUT17"),
            UserDictionaryImporter::ATOK);

  EXPECT_EQ(UserDictionaryImporter::GuessIMEType("!!DICUT20"),
            UserDictionaryImporter::ATOK);

  EXPECT_EQ(UserDictionaryImporter::GuessIMEType("\"foo\",\"bar\",\"buz\""),
            UserDictionaryImporter::KOTOERI);

  EXPECT_EQ(UserDictionaryImporter::GuessIMEType("\"comment\""),
            UserDictionaryImporter::KOTOERI);

  EXPECT_EQ(UserDictionaryImporter::GuessIMEType("foo\tbar\tbuz"),
            UserDictionaryImporter::MOZC);

  EXPECT_EQ(UserDictionaryImporter::GuessIMEType("foo\tbar"),
            UserDictionaryImporter::MOZC);

  EXPECT_EQ(UserDictionaryImporter::GuessIMEType("foo"),
            UserDictionaryImporter::NUM_IMES);
}

TEST(UserDictionaryImporter, DetermineFinalIMETypeTest) {
  EXPECT_EQ(UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::IME_AUTO_DETECT,
                UserDictionaryImporter::MSIME),
            UserDictionaryImporter::MSIME);

  EXPECT_EQ(UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::IME_AUTO_DETECT,
                UserDictionaryImporter::ATOK),
            UserDictionaryImporter::ATOK);

  EXPECT_EQ(UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::IME_AUTO_DETECT,
                UserDictionaryImporter::KOTOERI),
            UserDictionaryImporter::KOTOERI);

  EXPECT_EQ(UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::IME_AUTO_DETECT,
                UserDictionaryImporter::NUM_IMES),
            UserDictionaryImporter::NUM_IMES);

  EXPECT_EQ(UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::MOZC, UserDictionaryImporter::MSIME),
            UserDictionaryImporter::MOZC);

  EXPECT_EQ(UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::MOZC, UserDictionaryImporter::ATOK),
            UserDictionaryImporter::MOZC);

  EXPECT_EQ(UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::MOZC, UserDictionaryImporter::KOTOERI),
            UserDictionaryImporter::NUM_IMES);

  EXPECT_EQ(UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::MSIME, UserDictionaryImporter::MSIME),
            UserDictionaryImporter::MSIME);

  EXPECT_EQ(UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::ATOK, UserDictionaryImporter::MSIME),
            UserDictionaryImporter::NUM_IMES);

  EXPECT_EQ(UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::ATOK, UserDictionaryImporter::KOTOERI),
            UserDictionaryImporter::NUM_IMES);
}

TEST(UserDictionaryImporter, GuessEncodingTypeTest) {
  {
    const char str[] = "これはテストです。";
    EXPECT_EQ(UserDictionaryImporter::GuessEncodingType(str),
              UserDictionaryImporter::UTF8);
  }
  {
    const char str[] = "私の名前は中野ですABC";
    EXPECT_EQ(UserDictionaryImporter::GuessEncodingType(str),
              UserDictionaryImporter::UTF8);
  }
  {
    const char str[] = "ABCDEFG abcdefg";
    EXPECT_EQ(UserDictionaryImporter::GuessEncodingType(str),
              UserDictionaryImporter::UTF8);
  }
  {
    const char str[] = "ハロー";
    EXPECT_EQ(UserDictionaryImporter::GuessEncodingType(str),
              UserDictionaryImporter::UTF8);
  }

  {
    // "よろしくお願いします" in Shift-JIS
    const char str[] =
        "\x82\xE6\x82\xEB\x82\xB5\x82\xAD"
        "\x82\xA8\x8A\xE8\x82\xA2\x82\xB5\x82\xDC\x82\xB7";
    EXPECT_EQ(UserDictionaryImporter::GuessEncodingType(str),
              UserDictionaryImporter::SHIFT_JIS);
  }

  {
    // "東京" in Shift-JIS
    const char str[] = "\x93\x8C\x8B\x9E";
    EXPECT_EQ(UserDictionaryImporter::GuessEncodingType(str),
              UserDictionaryImporter::SHIFT_JIS);
  }

  {
    // BOM of UTF-16
    const char str[] = "\xFF\xFE";
    EXPECT_EQ(UserDictionaryImporter::GuessEncodingType(str),
              UserDictionaryImporter::UTF16);
  }

  {
    // BOM of UTF-16
    const char str[] = "\xFE\xFF";
    EXPECT_EQ(UserDictionaryImporter::GuessEncodingType(str),
              UserDictionaryImporter::UTF16);
  }

  {
    // BOM of UTF-8
    const char str[] = "\xEF\xBB\xBF";
    EXPECT_EQ(UserDictionaryImporter::GuessEncodingType(str),
              UserDictionaryImporter::UTF8);
  }
}

TEST(UserDictionaryImporter, StringTextLineIterator) {
  std::string line;
  const char *kTestData[] = {
      // Test for LF.
      "abcde\n"
      "fghij\n"
      "klmno",

      // Test for CR.
      "abcde\r"
      "fghij\r"
      "klmno",

      // Test for CRLF.
      "abcde\r\n"
      "fghij\r\n"
      "klmno",
  };

  for (size_t i = 0; i < std::size(kTestData); ++i) {
    UserDictionaryImporter::StringTextLineIterator iter(kTestData[i]);
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "abcde");
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "fghij");
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "klmno");
    EXPECT_FALSE(iter.IsAvailable());
  }

  // Test empty line with CR.
  {
    constexpr char kInput[] = "\r\rabcde";
    UserDictionaryImporter::StringTextLineIterator iter(kInput);
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "");
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "");
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "abcde");
    EXPECT_FALSE(iter.IsAvailable());
  }

  // Test empty line with LF.
  {
    constexpr char kInput[] = "\n\nabcde";
    UserDictionaryImporter::StringTextLineIterator iter(kInput);
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "");
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "");
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "abcde");
    EXPECT_FALSE(iter.IsAvailable());
  }

  // Test empty line with CRLF.
  {
    constexpr char kInput[] = "\r\n\r\nabcde";
    UserDictionaryImporter::StringTextLineIterator iter(kInput);
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "");
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "");
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "abcde");
    EXPECT_FALSE(iter.IsAvailable());
  }

  // Invalid empty line.
  // At the moment, \n\r is processed as two empty lines.
  {
    constexpr char kInput[] = "\n\rabcde";
    UserDictionaryImporter::StringTextLineIterator iter(kInput);
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "");
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "");
    ASSERT_TRUE(iter.IsAvailable());
    ASSERT_TRUE(iter.Next(&line));
    EXPECT_EQ(line, "abcde");
    EXPECT_FALSE(iter.IsAvailable());
  }
}

}  // namespace mozc

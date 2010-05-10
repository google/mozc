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

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "dictionary/user_dictionary_importer.h"
#include "dictionary/user_dictionary_util.h"
#include "dictionary/user_dictionary_storage.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

class TestInputIterator
    : public UserDictionaryImporter::InputIteratorInterface {
 public:
  TestInputIterator()
      : index_(0), is_available_(false), entries_(NULL) {}

  bool IsAvailable() const {
    return is_available_;
  }

  bool Next(UserDictionaryStorage::UserDictionaryEntry *entry) {
    if (!is_available_) {
      return false;
    }
    if (index_ >= entries_->size()) {
      return false;
    }
    entry->CopyFrom((*entries_)[index_]);
    ++index_;
    return true;
  }

  void set_entries(
      const vector<UserDictionaryStorage::UserDictionaryEntry> *entries) {
    entries_ = entries;
  }

  void set_available(bool is_available) {
    is_available_ = is_available;
  }

 public:
  int index_;
  bool is_available_;
  const vector<UserDictionaryStorage::UserDictionaryEntry> *entries_;
};
}

TEST(UserDictionaryImporter, ImportFromNormalTextTest) {
  // const char kInput[] =
  // "きょうと\t京都\t名詞\n"
  // "おおさか\t大阪\t地名\n"
  // "とうきょう\t東京\t地名\tコメント\n"
  // "すずき\t鈴木\t人名\n";

  const char kInput[] =
      "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8\t"
      "\xE4\xBA\xAC\xE9\x83\xBD\t\xE5\x90\x8D\xE8\xA9\x9E\n"
      "\xE3\x81\x8A\xE3\x81\x8A\xE3\x81\x95\xE3\x81\x8B\t"
      "\xE5\xA4\xA7\xE9\x98\xAA\t\xE5\x9C\xB0\xE5\x90\x8D\n"
      "\xE3\x81\xA8\xE3\x81\x86\xE3\x81\x8D\xE3\x82\x87\xE3"
      "\x81\x86\t\xE6\x9D\xB1\xE4\xBA\xAC\t\xE5\x9C\xB0\xE5"
      "\x90\x8D\t\xE3\x82\xB3\xE3\x83\xA1\xE3\x83\xB3\xE3\x83\x88\n"
      "\xE3\x81\x99\xE3\x81\x9A\xE3\x81\x8D\t\xE9\x88\xB4"
      "\xE6\x9C\xA8\t\xE4\xBA\xBA\xE5\x90\x8D\n";

  istringstream is(kInput);
  UserDictionaryImporter::IStreamTextLineIterator iter(&is);
  UserDictionaryStorage::UserDictionary user_dic;

  EXPECT_EQ(UserDictionaryImporter::IMPORT_NO_ERROR,
            UserDictionaryImporter::ImportFromTextLineIterator(
                UserDictionaryImporter::MOZC,
                &iter,
                &user_dic));

  EXPECT_EQ(4, user_dic.entries_size());

//   EXPECT_EQ("きょうと", user_dic.entries(0).key());
//   EXPECT_EQ("京都", user_dic.entries(0).value());
//   EXPECT_EQ("名詞", user_dic.entries(0).pos());

//   EXPECT_EQ("おおさか", user_dic.entries(1).key());
//   EXPECT_EQ("大阪", user_dic.entries(1).value());
//   EXPECT_EQ("地名", user_dic.entries(1).pos());

//   EXPECT_EQ("とうきょう", user_dic.entries(2).key());
//   EXPECT_EQ("東京", user_dic.entries(2).value());
//   EXPECT_EQ("地名", user_dic.entries(2).pos());
//   EXPECT_EQ("コメント", user_dic.entries(2).comment());

//   EXPECT_EQ("すずき", user_dic.entries(3).key());
//   EXPECT_EQ("鈴木", user_dic.entries(3).value());
//   EXPECT_EQ("人名", user_dic.entries(3).pos());

  EXPECT_EQ("\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8",
            user_dic.entries(0).key());
  EXPECT_EQ("\xE4\xBA\xAC\xE9\x83\xBD", user_dic.entries(0).value());
  EXPECT_EQ("\xE5\x90\x8D\xE8\xA9\x9E", user_dic.entries(0).pos());

  EXPECT_EQ("\xE3\x81\x8A\xE3\x81\x8A\xE3\x81\x95\xE3\x81\x8B",
            user_dic.entries(1).key());
  EXPECT_EQ("\xE5\xA4\xA7\xE9\x98\xAA", user_dic.entries(1).value());
  EXPECT_EQ("\xE5\x9C\xB0\xE5\x90\x8D", user_dic.entries(1).pos());

  EXPECT_EQ("\xE3\x81\xA8\xE3\x81\x86\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
            user_dic.entries(2).key());
  EXPECT_EQ("\xE6\x9D\xB1\xE4\xBA\xAC", user_dic.entries(2).value());
  EXPECT_EQ("\xE5\x9C\xB0\xE5\x90\x8D", user_dic.entries(2).pos());
  EXPECT_EQ("\xE3\x82\xB3\xE3\x83\xA1\xE3\x83\xB3\xE3\x83\x88",
            user_dic.entries(2).comment());

  EXPECT_EQ("\xE3\x81\x99\xE3\x81\x9A\xE3\x81\x8D", user_dic.entries(3).key());
  EXPECT_EQ("\xE9\x88\xB4\xE6\x9C\xA8", user_dic.entries(3).value());
  EXPECT_EQ("\xE4\xBA\xBA\xE5\x90\x8D", user_dic.entries(3).pos());
}

TEST(UserDictionaryImporter, ImportFromKotoeriTextTest) {
  const char kInput[] =
      "\"\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8\","
      "\"\xE4\xBA\xAC\xE9\x83\xBD\",\"\xE5\x90\x8D\xE8\xA9\x9E\"\n"
      "\"\xE3\x81\x8A\xE3\x81\x8A\xE3\x81\x95\xE3\x81\x8B\","
      "\"\xE5\xA4\xA7\xE9\x98\xAA\",\"\xE5\x9C\xB0\xE5\x90\x8D\"\n"
      "// last line";

  {
    istringstream is(kInput);
    UserDictionaryImporter::IStreamTextLineIterator iter(&is);
    UserDictionaryStorage::UserDictionary user_dic;

    EXPECT_EQ(UserDictionaryImporter::IMPORT_NOT_SUPPORTED,
              UserDictionaryImporter::ImportFromTextLineIterator(
                  UserDictionaryImporter::MOZC,
                  &iter,
                  &user_dic));

    EXPECT_EQ(0, user_dic.entries_size());
  }

  {
    istringstream is(kInput);
    UserDictionaryImporter::IStreamTextLineIterator iter(&is);
    UserDictionaryStorage::UserDictionary user_dic;

    EXPECT_EQ(UserDictionaryImporter::IMPORT_NO_ERROR,
              UserDictionaryImporter::ImportFromTextLineIterator(
                  UserDictionaryImporter::KOTOERI,
                  &iter,
                  &user_dic));

    EXPECT_EQ(2, user_dic.entries_size());

//     EXPECT_EQ("きょうと", user_dic.entries(0).key());
//     EXPECT_EQ("京都", user_dic.entries(0).value());
//     EXPECT_EQ("名詞", user_dic.entries(0).pos());

//     EXPECT_EQ("おおさか", user_dic.entries(1).key());
//     EXPECT_EQ("大阪", user_dic.entries(1).value());
//     EXPECT_EQ("地名", user_dic.entries(1).pos());

    EXPECT_EQ("\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8",
              user_dic.entries(0).key());
    EXPECT_EQ("\xE4\xBA\xAC\xE9\x83\xBD", user_dic.entries(0).value());
    EXPECT_EQ("\xE5\x90\x8D\xE8\xA9\x9E", user_dic.entries(0).pos());

    EXPECT_EQ("\xE3\x81\x8A\xE3\x81\x8A\xE3\x81\x95\xE3\x81\x8B",
              user_dic.entries(1).key());
    EXPECT_EQ("\xE5\xA4\xA7\xE9\x98\xAA", user_dic.entries(1).value());
    EXPECT_EQ("\xE5\x9C\xB0\xE5\x90\x8D", user_dic.entries(1).pos());

  }
}

TEST(UserDictionaryImporter, ImportFromCommentTextTest) {
//   const char kInput[] =
//       "きょうと\t京都\t名詞\n"
//       "!おおさか\t大阪\t地名\n"
//       "\n"
//       "#とうきょう\t東京\t地名\tコメント\n"
//       "すずき\t鈴木\t人名\n";

  const char kInput[] =
      "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8\t"
      "\xE4\xBA\xAC\xE9\x83\xBD\t\xE5\x90\x8D\xE8\xA9\x9E\n"
      "!\xE3\x81\x8A\xE3\x81\x8A\xE3\x81\x95\xE3\x81\x8B\t"
      "\xE5\xA4\xA7\xE9\x98\xAA\t\xE5\x9C\xB0\xE5\x90\x8D\n"
      "\n"
      "#\xE3\x81\xA8\xE3\x81\x86\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\t"
      "\xE6\x9D\xB1\xE4\xBA\xAC\t\xE5\x9C\xB0\xE5\x90\x8D\t"
      "\xE3\x82\xB3\xE3\x83\xA1\xE3\x83\xB3\xE3\x83\x88\n"
      "\xE3\x81\x99\xE3\x81\x9A\xE3\x81\x8D\t"
      "\xE9\x88\xB4\xE6\x9C\xA8\t\xE4\xBA\xBA\xE5\x90\x8D\n";

  {

    istringstream is(string("!Microsoft IME\n") + kInput);
    UserDictionaryImporter::IStreamTextLineIterator iter(&is);
    UserDictionaryStorage::UserDictionary user_dic;

    EXPECT_EQ(UserDictionaryImporter::IMPORT_NO_ERROR,
              UserDictionaryImporter::ImportFromTextLineIterator(
                  UserDictionaryImporter::MSIME,
                  &iter,
                  &user_dic));

    EXPECT_EQ(3, user_dic.entries_size());

//     EXPECT_EQ("きょうと", user_dic.entries(0).key());
//     EXPECT_EQ("京都", user_dic.entries(0).value());
//     EXPECT_EQ("名詞", user_dic.entries(0).pos());

//     EXPECT_EQ("#とうきょう", user_dic.entries(1).key());
//     EXPECT_EQ("東京", user_dic.entries(1).value());
//     EXPECT_EQ("地名", user_dic.entries(1).pos());

//     EXPECT_EQ("すずき", user_dic.entries(2).key());
//     EXPECT_EQ("鈴木", user_dic.entries(2).value());
//     EXPECT_EQ("人名", user_dic.entries(2).pos());

    EXPECT_EQ("\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8",
              user_dic.entries(0).key());
    EXPECT_EQ("\xE4\xBA\xAC\xE9\x83\xBD", user_dic.entries(0).value());
    EXPECT_EQ("\xE5\x90\x8D\xE8\xA9\x9E", user_dic.entries(0).pos());

    EXPECT_EQ("#\xE3\x81\xA8\xE3\x81\x86\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
              user_dic.entries(1).key());
    EXPECT_EQ("\xE6\x9D\xB1\xE4\xBA\xAC", user_dic.entries(1).value());
    EXPECT_EQ("\xE5\x9C\xB0\xE5\x90\x8D", user_dic.entries(1).pos());

    EXPECT_EQ("\xE3\x81\x99\xE3\x81\x9A\xE3\x81\x8D",
              user_dic.entries(2).key());
    EXPECT_EQ("\xE9\x88\xB4\xE6\x9C\xA8", user_dic.entries(2).value());
    EXPECT_EQ("\xE4\xBA\xBA\xE5\x90\x8D", user_dic.entries(2).pos());
  }

  {
    istringstream is(kInput);
    UserDictionaryImporter::IStreamTextLineIterator iter(&is);
    UserDictionaryStorage::UserDictionary user_dic;

    EXPECT_EQ(UserDictionaryImporter::IMPORT_NO_ERROR,
              UserDictionaryImporter::ImportFromTextLineIterator(
                  UserDictionaryImporter::MOZC,
                  &iter,
                  &user_dic));

    EXPECT_EQ(3, user_dic.entries_size());

//     EXPECT_EQ("きょうと", user_dic.entries(0).key());
//     EXPECT_EQ("京都", user_dic.entries(0).value());
//     EXPECT_EQ("名詞", user_dic.entries(0).pos());

//     EXPECT_EQ("!おおさか", user_dic.entries(1).key());
//     EXPECT_EQ("大阪", user_dic.entries(1).value());
//     EXPECT_EQ("地名", user_dic.entries(1).pos());

//     EXPECT_EQ("すずき", user_dic.entries(2).key());
//     EXPECT_EQ("鈴木", user_dic.entries(2).value());
//     EXPECT_EQ("人名", user_dic.entries(2).pos());

    EXPECT_EQ("\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8",
              user_dic.entries(0).key());
    EXPECT_EQ("\xE4\xBA\xAC\xE9\x83\xBD", user_dic.entries(0).value());
    EXPECT_EQ("\xE5\x90\x8D\xE8\xA9\x9E", user_dic.entries(0).pos());

    EXPECT_EQ("!\xE3\x81\x8A\xE3\x81\x8A\xE3\x81\x95\xE3\x81\x8B",
              user_dic.entries(1).key());
    EXPECT_EQ("\xE5\xA4\xA7\xE9\x98\xAA", user_dic.entries(1).value());
    EXPECT_EQ("\xE5\x9C\xB0\xE5\x90\x8D", user_dic.entries(1).pos());

    EXPECT_EQ("\xE3\x81\x99\xE3\x81\x9A\xE3\x81\x8D", user_dic.entries(2).key());
    EXPECT_EQ("\xE9\x88\xB4\xE6\x9C\xA8", user_dic.entries(2).value());
    EXPECT_EQ("\xE4\xBA\xBA\xE5\x90\x8D", user_dic.entries(2).pos());
  }
}

TEST(UserDictionaryImporter, ImportFromInvalidTextTest) {
//   const char kInput[] =
//       "a"
//       "\n"
//       "東京\t\t地名\tコメント\n"
//       "すずき\t鈴木\t人名\n";

  const char kInput[] =
      "a"
      "\n"
      "\xE6\x9D\xB1\xE4\xBA\xAC\t\t\xE5\x9C\xB0\xE5\x90\x8D\t"
      "\xE3\x82\xB3\xE3\x83\xA1\xE3\x83\xB3\xE3\x83\x88\n"
      "\xE3\x81\x99\xE3\x81\x9A\xE3\x81\x8D\t"
      "\xE9\x88\xB4\xE6\x9C\xA8\t\xE4\xBA\xBA\xE5\x90\x8D\n";

  istringstream is(kInput);
  UserDictionaryImporter::IStreamTextLineIterator iter(&is);
  UserDictionaryStorage::UserDictionary user_dic;

  EXPECT_EQ(UserDictionaryImporter::IMPORT_INVALID_ENTRIES,
            UserDictionaryImporter::ImportFromTextLineIterator(
                UserDictionaryImporter::MOZC,
                &iter,
                &user_dic));

  EXPECT_EQ(1, user_dic.entries_size());

//   EXPECT_EQ("すずき", user_dic.entries(0).key());
//   EXPECT_EQ("鈴木", user_dic.entries(0).value());
//   EXPECT_EQ("人名", user_dic.entries(0).pos());

  EXPECT_EQ("\xE3\x81\x99\xE3\x81\x9A\xE3\x81\x8D", user_dic.entries(0).key());
  EXPECT_EQ("\xE9\x88\xB4\xE6\x9C\xA8", user_dic.entries(0).value());
  EXPECT_EQ("\xE4\xBA\xBA\xE5\x90\x8D", user_dic.entries(0).pos());

}

TEST(UserDictionaryImporter, ImportFromIteratorInvalidTest) {
  TestInputIterator iter;
  UserDictionaryStorage::UserDictionary user_dic;
  EXPECT_FALSE(iter.IsAvailable());
  EXPECT_EQ(UserDictionaryImporter::IMPORT_NO_ERROR,
            UserDictionaryImporter::ImportFromIterator(
                &iter, &user_dic));
}

TEST(UserDictionaryImporter, ImportFromIteratorAlreadyFullTest) {
  TestInputIterator iter;
  iter.set_available(true);
  UserDictionaryStorage::UserDictionary user_dic;

  vector<UserDictionaryStorage::UserDictionaryEntry> entries;
  {
    UserDictionaryStorage::UserDictionaryEntry entry;
    entry.set_key("aa");
    entry.set_value("aa");
    // entry.set_pos("名詞");
    entry.set_pos("\xE5\x90\x8D\xE8\xA9\x9E");
    entries.push_back(entry);
  }

  for (int i = 0; i < UserDictionaryStorage::max_entry_size(); ++i) {
    user_dic.add_entries();
  }

  iter.set_available(true);
  iter.set_entries(&entries);

  EXPECT_EQ(UserDictionaryStorage::max_entry_size(),
            user_dic.entries_size());

  EXPECT_TRUE(iter.IsAvailable());
  EXPECT_EQ(UserDictionaryImporter::IMPORT_TOO_MANY_WORDS,
            UserDictionaryImporter::ImportFromIterator(
                &iter, &user_dic));

  EXPECT_EQ(UserDictionaryStorage::max_entry_size(),
            user_dic.entries_size());
}

TEST(UserDictionaryImporter, ImportFromIteratorNormalTest) {
  TestInputIterator iter;
  UserDictionaryStorage::UserDictionary user_dic;

  static const size_t kSize[] = { 10, 100, 1000, 5000, 12000 };
  for (size_t i = 0; i < arraysize(kSize); ++i) {
    vector<UserDictionaryStorage::UserDictionaryEntry> entries;
    for (size_t j = 0; j < kSize[i]; ++j) {
      UserDictionaryStorage::UserDictionaryEntry entry;
      const string key = "key" + Util::SimpleItoa(j);
      const string value = "value" + Util::SimpleItoa(j);
      entry.set_key(key);
      entry.set_value(value);
      // entry.set_pos("名詞");
      entry.set_pos("\xE5\x90\x8D\xE8\xA9\x9E");
      entries.push_back(entry);
    }

    iter.set_available(true);
    iter.set_entries(&entries);

    if (kSize[i] <= UserDictionaryStorage::max_entry_size()) {
      EXPECT_EQ(UserDictionaryImporter::IMPORT_NO_ERROR,
                UserDictionaryImporter::ImportFromIterator(
                    &iter, &user_dic));
    } else {
      EXPECT_EQ(UserDictionaryImporter::IMPORT_TOO_MANY_WORDS,
                UserDictionaryImporter::ImportFromIterator(
                    &iter, &user_dic));
    }

    const size_t size = min(UserDictionaryStorage::max_entry_size(),
                            kSize[i]);
    EXPECT_EQ(size, user_dic.entries_size());
    for (size_t j = 0; j < size; ++j) {
      EXPECT_EQ(entries[j].DebugString(),
                user_dic.entries(j).DebugString());
    }
  }
}

TEST(UserDictionaryImporter, ImportFromIteratorInvalidEntriesTest) {
  TestInputIterator iter;
  UserDictionaryStorage::UserDictionary user_dic;

  static const size_t kSize[] = { 10, 100, 1000 };
  for (size_t i = 0; i < arraysize(kSize); ++i) {
    vector<UserDictionaryStorage::UserDictionaryEntry> entries;
    for (size_t j = 0; j < kSize[i]; ++j) {
      UserDictionaryStorage::UserDictionaryEntry entry;
      const string key = "key" + Util::SimpleItoa(j);
      const string value = "value" + Util::SimpleItoa(j);
      entry.set_key(key);
      entry.set_value(value);
      if (j % 2 == 0) {
        // entry.set_pos("名詞");
        entry.set_pos("\xE5\x90\x8D\xE8\xA9\x9E");
      }
      entries.push_back(entry);
    }

    iter.set_available(true);
    iter.set_entries(&entries);

    EXPECT_EQ(UserDictionaryImporter::IMPORT_INVALID_ENTRIES,
              UserDictionaryImporter::ImportFromIterator(
                  &iter, &user_dic));
    EXPECT_EQ(kSize[i] / 2, user_dic.entries_size());
  }
}

TEST(UserDictionaryImporter, ImportFromIteratorDupTest) {
  TestInputIterator iter;
  iter.set_available(true);
  UserDictionaryStorage::UserDictionary user_dic;

  {
    UserDictionaryStorage::UserDictionaryEntry *entry
        = user_dic.add_entries();
    entry->set_key("aa");
    entry->set_value("aa");
    // entry->set_pos("名詞");
    entry->set_pos("\xE5\x90\x8D\xE8\xA9\x9E");
  }

  vector<UserDictionaryStorage::UserDictionaryEntry> entries;

  {
    UserDictionaryStorage::UserDictionaryEntry entry;
    entry.set_key("aa");
    entry.set_value("aa");
    // entry.set_pos("名詞");
    entry.set_pos("\xE5\x90\x8D\xE8\xA9\x9E");
    entries.push_back(entry);
  }

  iter.set_entries(&entries);

  EXPECT_EQ(UserDictionaryImporter::IMPORT_NO_ERROR,
            UserDictionaryImporter::ImportFromIterator(
                &iter, &user_dic));

  EXPECT_EQ(1, user_dic.entries_size());

  {
    UserDictionaryStorage::UserDictionaryEntry entry;
    entry.set_key("bb");
    entry.set_value("bb");
    // entry.set_pos("名詞");
    entry.set_pos("\xE5\x90\x8D\xE8\xA9\x9E");
    entries.push_back(entry);
  }

  iter.set_entries(&entries);

  EXPECT_EQ(UserDictionaryImporter::IMPORT_NO_ERROR,
            UserDictionaryImporter::ImportFromIterator(
                &iter, &user_dic));

  EXPECT_EQ(2, user_dic.entries_size());

  EXPECT_EQ(UserDictionaryImporter::IMPORT_NO_ERROR,
            UserDictionaryImporter::ImportFromIterator(
                &iter, &user_dic));

  EXPECT_EQ(2, user_dic.entries_size());
}

TEST(UserDictionaryImporter, GuessIMETypeTest) {
  EXPECT_EQ(UserDictionaryImporter::NUM_IMES,
            UserDictionaryImporter::GuessIMEType(""));

  EXPECT_EQ(UserDictionaryImporter::MSIME,
            UserDictionaryImporter::GuessIMEType(
                "!Microsoft IME Dictionary Tool"));

  EXPECT_EQ(UserDictionaryImporter::ATOK,
            UserDictionaryImporter::GuessIMEType(
                "!!ATOK_TANGO_TEXT_HEADER_1"));

  EXPECT_EQ(UserDictionaryImporter::NUM_IMES,
            UserDictionaryImporter::GuessIMEType(
                "!!DICUT10"));

  EXPECT_EQ(UserDictionaryImporter::NUM_IMES,
            UserDictionaryImporter::GuessIMEType(
                "!!DICUT"));

  EXPECT_EQ(UserDictionaryImporter::ATOK,
            UserDictionaryImporter::GuessIMEType(
                "!!DICUT11"));

  EXPECT_EQ(UserDictionaryImporter::ATOK,
            UserDictionaryImporter::GuessIMEType(
                "!!DICUT17"));

  EXPECT_EQ(UserDictionaryImporter::ATOK,
            UserDictionaryImporter::GuessIMEType(
                "!!DICUT20"));

  EXPECT_EQ(UserDictionaryImporter::KOTOERI,
            UserDictionaryImporter::GuessIMEType(
                "\"foo\",\"bar\",\"buz\""));

  EXPECT_EQ(UserDictionaryImporter::KOTOERI,
            UserDictionaryImporter::GuessIMEType(
                "\"comment\""));

  EXPECT_EQ(UserDictionaryImporter::MOZC,
            UserDictionaryImporter::GuessIMEType(
                "foo\tbar\tbuz"));

  EXPECT_EQ(UserDictionaryImporter::MOZC,
            UserDictionaryImporter::GuessIMEType(
                "foo\tbar"));

  EXPECT_EQ(UserDictionaryImporter::NUM_IMES,
            UserDictionaryImporter::GuessIMEType(
                "foo"));
}

TEST(UserDictionaryImporter, DetermineFinalIMETypeTest) {
  EXPECT_EQ(UserDictionaryImporter::MSIME,
            UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::IME_AUTO_DETECT,
                UserDictionaryImporter::MSIME));

  EXPECT_EQ(UserDictionaryImporter::ATOK,
            UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::IME_AUTO_DETECT,
                UserDictionaryImporter::ATOK));

  EXPECT_EQ(UserDictionaryImporter::KOTOERI,
            UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::IME_AUTO_DETECT,
                UserDictionaryImporter::KOTOERI));

  EXPECT_EQ(UserDictionaryImporter::NUM_IMES,
            UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::IME_AUTO_DETECT,
                UserDictionaryImporter::NUM_IMES));

  EXPECT_EQ(UserDictionaryImporter::MOZC,
            UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::MOZC,
                UserDictionaryImporter::MSIME));

  EXPECT_EQ(UserDictionaryImporter::MOZC,
            UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::MOZC,
                UserDictionaryImporter::ATOK));

  EXPECT_EQ(UserDictionaryImporter::NUM_IMES,
            UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::MOZC,
                UserDictionaryImporter::KOTOERI));

  EXPECT_EQ(UserDictionaryImporter::MSIME,
            UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::MSIME,
                UserDictionaryImporter::MSIME));

  EXPECT_EQ(UserDictionaryImporter::NUM_IMES,
            UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::ATOK,
                UserDictionaryImporter::MSIME));

  EXPECT_EQ(UserDictionaryImporter::NUM_IMES,
            UserDictionaryImporter::DetermineFinalIMEType(
                UserDictionaryImporter::ATOK,
                UserDictionaryImporter::KOTOERI));
}

TEST(UserDictionaryImporter, GuessEncodingTypeTest) {
  {
    // const string str = "これはテストです。";
    const string str = "\xE3\x81\x93\xE3\x82\x8C\xE3\x81\xAF\xE3\x83\x86"
        "\xE3\x82\xB9\xE3\x83\x88\xE3\x81\xA7\xE3\x81\x99\xE3\x80\x82";
    EXPECT_EQ(UserDictionaryImporter::UTF8,
              UserDictionaryImporter::GuessEncodingType(
                  str.c_str(), str.size()));
  }

  {
    // const string str = "私の名前は中野ですABC";
    const string str = "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
        "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99"
        "ABC";
    EXPECT_EQ(UserDictionaryImporter::UTF8,
              UserDictionaryImporter::GuessEncodingType(
                  str.c_str(), str.size()));
  }

  {
    const string str = "ABCDEFG abcdefg";
    EXPECT_EQ(UserDictionaryImporter::UTF8,
              UserDictionaryImporter::GuessEncodingType(
                  str.c_str(), str.size()));
  }

  {
    //     const string str = "ハロー";
    const string str = "\xE3\x83\x8F\xE3\x83\xAD\xE3\x83\xBC";
    EXPECT_EQ(UserDictionaryImporter::UTF8,
              UserDictionaryImporter::GuessEncodingType(
                  str.c_str(), str.size()));
  }

  {
    const string str = "\x82\xE6\x82\xEB\x82\xB5\x82\xAD"
        "\x82\xA8\x8A\xE8\x82\xA2\x82\xB5\x82\xDC\x82\xB7";
    EXPECT_EQ(UserDictionaryImporter::SHIFT_JIS,
              UserDictionaryImporter::GuessEncodingType(
                  str.c_str(), str.size()));
  }

  {
    const string str = "\x93\x8C\x8B\x9E";
    EXPECT_EQ(UserDictionaryImporter::SHIFT_JIS,
              UserDictionaryImporter::GuessEncodingType(
                  str.c_str(), str.size()));
  }

  {
    const string str = "\xFF\xFE";
    EXPECT_EQ(UserDictionaryImporter::UTF16,
              UserDictionaryImporter::GuessEncodingType(
                  str.c_str(), str.size()));
  }

  {
    const string str = "\xFE\xFF";
    EXPECT_EQ(UserDictionaryImporter::UTF16,
              UserDictionaryImporter::GuessEncodingType(
                  str.c_str(), str.size()));
  }

  {
    const string str = "\xEF\xBB\xBF";
    EXPECT_EQ(UserDictionaryImporter::UTF8,
              UserDictionaryImporter::GuessEncodingType(
                  str.c_str(), str.size()));
  }
}

TEST(UserDictionaryImporter, ImportFromMSIMETest) {
  UserDictionaryStorage::UserDictionary dic;

  UserDictionaryImporter::ErrorType result =
      UserDictionaryImporter::ImportFromMSIME(&dic);

#ifdef OS_WINDOWS
  // Currently the following tests are disabled since necessary components
  // are not available on Punlse VM. See http://b/237578 for details.
  // TODO(yukawa): Arrange some automated tests instead of these tests.
  //               http://b/2375839
  // EXPECT_NE(UserDictionaryImporter::IMPORT_CANNOT_OPEN_DICTIONARY, result);
  // EXPECT_NE(UserDictionaryImporter::IMPORT_FATAL, result);
  // EXPECT_NE(UserDictionaryImporter::IMPORT_UNKNOWN_ERROR, result);
#else
  EXPECT_EQ(UserDictionaryImporter::IMPORT_NOT_SUPPORTED, result);
#endif
}
}  // mozc

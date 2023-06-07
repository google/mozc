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

#include "dictionary/user_dictionary.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/random.h"
#include "base/singleton.h"
#include "config/config_handler.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_test_util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_pos.h"
#include "dictionary/user_pos_interface.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace dictionary {
namespace {

constexpr char kUserDictionary0[] =
    "start\tstart\tverb\n"
    "star\tstar\tnoun\n"
    "starting\tstarting\tnoun\n"
    "stamp\tstamp\tnoun\n"
    "stand\tstand\tverb\n"
    "smile\tsmile\tverb\n"
    "smog\tsmog\tnoun\n"
    // invalid characters "あ\x81\x84う" in reading. "い" is "E3 81 84".
    "あ\x81\x84う\tvalue\tnoun\n"

    // This is also a valid entry. Key can contain any valid characters.
    "水雲\tvalue\tnoun\n"

    // Empty key
    "\tvalue\tnoun\n"
    // Empty value
    "start\t\tnoun\n"
    // Invalid POS
    "star\tvalue\tpos\n"
    // Empty POS
    "star\tvalue\t\n"
    // Duplicate entry
    "start\tstart\tverb\n"
    // The following are for tests for LookupComment
    // No comment
    "comment_key1\tcomment_value1\tnoun\n"
    // Has comment
    "comment_key2\tcomment_value2\tnoun\tcomment\n"
    // Different POS
    "comment_key3\tcomment_value3\tnoun\tcomment1\n"
    "comment_key3\tcomment_value3\tverb\tcomment2\n"
    // White spaces comment
    "comment_key4\tcomment_value4\tverb\t     \n";

constexpr char kUserDictionary1[] = "end\tend\tverb\n";

void PushBackToken(absl::string_view key, absl::string_view value, uint16_t id,
                   std::vector<UserPos::Token> *tokens) {
  tokens->resize(tokens->size() + 1);
  UserPos::Token *t = &tokens->back();
  t->key = std::string(key);
  t->value = std::string(value);
  t->id = id;
}

// This class is a mock class for writing unit tests of a class that
// depends on POS. It accepts only two values for part-of-speech:
// "noun" as words without inflection and "verb" as words with
// inflection.
class UserPosMock : public UserPosInterface {
 public:
  UserPosMock() = default;

  UserPosMock(const UserPosMock &) = delete;
  UserPosMock &operator=(const UserPosMock &) = delete;

  ~UserPosMock() override = default;

  // This method returns true if the given pos is "noun" or "verb".
  bool IsValidPos(absl::string_view pos) const override { return true; }

  static constexpr absl::string_view kNoun = "名詞";
  static constexpr absl::string_view kVerb = "動詞ワ行五段";

  // Given a verb, this method expands it to three different forms,
  // i.e. base form (the word itself), "-ed" form and "-ing" form. For
  // example, if the given word is "play", the method returns "play",
  // "played" and "playing". When a noun is passed, it returns only
  // base form. The method set lid and rid of the word as following:
  //
  //  POS              | lid | rid
  // ------------------+-----+-----
  //  noun             | 100 | 100
  //  verb (base form) | 200 | 200
  //  verb (-ed form)  | 210 | 210
  //  verb (-ing form) | 220 | 220
  bool GetTokens(absl::string_view key, absl::string_view value,
                 absl::string_view pos, absl::string_view locale,
                 std::vector<UserPos::Token> *tokens) const override {
    if (key.empty() || value.empty() || pos.empty() || tokens == nullptr) {
      return false;
    }

    tokens->clear();
    if (pos == kNoun) {
      PushBackToken(key, value, 100, tokens);
      return true;
    } else if (pos == kVerb) {
      PushBackToken(key, value, 200, tokens);
      PushBackToken(absl::StrCat(key, "ed"), absl::StrCat(value, "ed"), 210,
                    tokens);
      PushBackToken(absl::StrCat(key, "ing"), absl::StrCat(value, "ing"), 220,
                    tokens);
      return true;
    } else {
      return false;
    }
  }

  void GetPosList(std::vector<std::string> *pos_list) const override {}

  bool GetPosIds(absl::string_view pos, uint16_t *id) const override {
    return false;
  }
};

class UserDictionaryTest : public ::testing::Test {
 protected:
  UserDictionaryTest() { convreq_.set_config(&config_); }

  void SetUp() override {
    suppression_dictionary_ = std::make_unique<SuppressionDictionary>();

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
    config::ConfigHandler::GetDefaultConfig(&config_);
  }

  void TearDown() override {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();

    // This config initialization will be removed once ConversionRequest can
    // take config as an injected argument.
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  // Workaround for the constructor of UserDictionary being protected.
  // Creates a user dictionary with mock pos data.
  UserDictionary *CreateDictionaryWithMockPos() {
    return new UserDictionary(
        std::make_unique<UserPosMock>(),
        dictionary::PosMatcher(mock_data_manager_.GetPosMatcherData()),
        suppression_dictionary_.get());
  }

  // Creates a user dictionary with actual pos data.
  UserDictionary *CreateDictionary() {
    return new UserDictionary(
        UserPos::CreateFromDataManager(mock_data_manager_),
        dictionary::PosMatcher(mock_data_manager_.GetPosMatcherData()),
        Singleton<SuppressionDictionary>::get());
  }

  struct Entry {
    std::string key;
    std::string value;
    uint16_t lid;
    uint16_t rid;
  };

  class EntryCollector : public DictionaryInterface::Callback {
   public:
    ResultType OnToken(absl::string_view,  // key
                       absl::string_view,  // actual_key
                       const Token &token) override {
      // Collect only user dictionary entries.
      if (token.attributes & Token::USER_DICTIONARY) {
        entries_.push_back(Entry());
        entries_.back().key = token.key;
        entries_.back().value = token.value;
        entries_.back().lid = token.lid;
        entries_.back().rid = token.rid;
      }
      return TRAVERSE_CONTINUE;
    }

    const std::vector<Entry> &entries() const { return entries_; }

   private:
    std::vector<Entry> entries_;
  };

  bool TestLookupPredictiveHelper(const Entry *expected, size_t expected_size,
                                  absl::string_view key,
                                  const UserDictionary &dic) {
    EntryCollector collector;
    dic.LookupPredictive(key, convreq_, &collector);

    if (expected == nullptr || expected_size == 0) {
      return collector.entries().empty();
    }
    return (!collector.entries().empty() &&
            CompareEntries(expected, expected_size, collector.entries()));
  }

  void TestLookupPrefixHelper(const Entry *expected, size_t expected_size,
                              const char *key, size_t key_size,
                              const UserDictionary &dic) {
    EntryCollector collector;
    dic.LookupPrefix(absl::string_view(key, key_size), convreq_, &collector);

    if (expected == nullptr || expected_size == 0) {
      EXPECT_TRUE(collector.entries().empty());
    } else {
      ASSERT_FALSE(collector.entries().empty());
      CompareEntries(expected, expected_size, collector.entries());
    }
  }

  void TestLookupExactHelper(const Entry *expected, size_t expected_size,
                             const char *key, size_t key_size,
                             const UserDictionary &dic) {
    EntryCollector collector;
    dic.LookupExact(absl::string_view(key, key_size), convreq_, &collector);

    if (expected == nullptr || expected_size == 0) {
      EXPECT_TRUE(collector.entries().empty());
    } else {
      ASSERT_FALSE(collector.entries().empty());
      CompareEntries(expected, expected_size, collector.entries());
    }
  }

  static std::string EncodeEntry(const Entry &entry) {
    return entry.key + "\t" + entry.value + "\t" + std::to_string(entry.lid) +
           "\t" + std::to_string(entry.rid) + "\n";
  }

  static std::string EncodeEntries(const Entry *array, size_t size) {
    std::vector<std::string> encoded_items;
    for (size_t i = 0; i < size; ++i) {
      encoded_items.push_back(EncodeEntry(array[i]));
    }
    std::sort(encoded_items.begin(), encoded_items.end());
    return absl::StrJoin(encoded_items, "");
  }

  static bool CompareEntries(const Entry *expected, size_t expected_size,
                             const std::vector<Entry> &actual) {
    const std::string expected_encoded = EncodeEntries(expected, expected_size);
    const std::string actual_encoded = EncodeEntries(&actual[0], actual.size());
    return expected_encoded == actual_encoded;
  }

  static void LoadFromString(const std::string &contents,
                             UserDictionaryStorage *storage) {
    std::istringstream is(contents);
    CHECK(is.good());

    storage->GetProto().Clear();
    UserDictionaryStorage::UserDictionary *dic =
        storage->GetProto().add_dictionaries();
    CHECK(dic);

    std::string line;
    while (!std::getline(is, line).fail()) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      std::vector<std::string> fields =
          absl::StrSplit(line, '\t', absl::AllowEmpty());
      EXPECT_GE(fields.size(), 3) << line;
      UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
      CHECK(entry);
      entry->set_key(std::move(fields[0]));
      entry->set_value(std::move(fields[1]));
      if (fields[2] == "verb") {
        entry->set_pos(user_dictionary::UserDictionary::WA_GROUP1_VERB);
      } else if (fields[2] == "noun") {
        entry->set_pos(user_dictionary::UserDictionary::NOUN);
      }
      if (fields.size() >= 4 && !fields[3].empty()) {
        entry->set_comment(std::move(fields[3]));
      }
    }
  }

  // Helper function to lookup comment string from |dic|.
  std::string LookupComment(const UserDictionary &dic, absl::string_view key,
                            absl::string_view value) {
    std::string comment;
    dic.LookupComment(key, value, convreq_, &comment);
    return comment;
  }

  std::unique_ptr<SuppressionDictionary> suppression_dictionary_;
  ConversionRequest convreq_;
  config::Config config_;

 private:
  const testing::ScopedTempUserProfileDirectory scoped_profile_dir_;
  const testing::MockDataManager mock_data_manager_;
  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(UserDictionaryTest, TestLookupPredictive) {
  std::unique_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    UserDictionaryTest::LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage.GetProto());
  }

  // A normal lookup operation.
  const Entry kExpected0[] = {
      {"start", "start", 200, 200},
      {"started", "started", 210, 210},
      {"starting", "starting", 100, 100},
      {"starting", "starting", 220, 220},
  };
  EXPECT_TRUE(TestLookupPredictiveHelper(kExpected0, std::size(kExpected0),
                                         "start", *dic));

  // Another normal lookup operation.
  const Entry kExpected1[] = {
      {"stamp", "stamp", 100, 100},       {"stand", "stand", 200, 200},
      {"standed", "standed", 210, 210},   {"standing", "standing", 220, 220},
      {"star", "star", 100, 100},         {"start", "start", 200, 200},
      {"started", "started", 210, 210},   {"starting", "starting", 100, 100},
      {"starting", "starting", 220, 220},
  };
  EXPECT_TRUE(TestLookupPredictiveHelper(kExpected1, std::size(kExpected1),
                                         "st", *dic));

  // Invalid input values should be just ignored.
  TestLookupPredictiveHelper(nullptr, 0, "", *dic);
  TestLookupPredictiveHelper(nullptr, 0, "あ\x81\x84う", *dic);

  // Kanji is also a valid key chararcter.
  const Entry kExpected2[] = {
      {"水雲", "value", 100, 100},
  };
  TestLookupPredictiveHelper(kExpected2, std::size(kExpected2), "水雲", *dic);

  // Make a change to the dictionary file and load it again.
  {
    UserDictionaryStorage storage("");
    LoadFromString(kUserDictionary1, &storage);
    dic->Load(storage.GetProto());
  }

  // A normal lookup again.
  const Entry kExpected3[] = {
      {"end", "end", 200, 200},
      {"ended", "ended", 210, 210},
      {"ending", "ending", 220, 220},
  };
  TestLookupPredictiveHelper(kExpected3, std::size(kExpected3), "end", *dic);

  // Entries in the dictionary before reloading cannot be looked up.
  EXPECT_TRUE(TestLookupPredictiveHelper(nullptr, 0, "start", *dic));
  EXPECT_TRUE(TestLookupPredictiveHelper(nullptr, 0, "st", *dic));
}

TEST_F(UserDictionaryTest, TestLookupPrefix) {
  std::unique_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage.GetProto());
  }

  // A normal lookup operation.
  const Entry kExpected0[] = {
      {"star", "star", 100, 100},
      {"start", "start", 200, 200},
      {"started", "started", 210, 210},
  };
  TestLookupPrefixHelper(kExpected0, std::size(kExpected0), "started", 7, *dic);

  // Another normal lookup operation.
  const Entry kExpected1[] = {
      {"star", "star", 100, 100},
      {"start", "start", 200, 200},
      {"starting", "starting", 100, 100},
      {"starting", "starting", 220, 220},
  };
  TestLookupPrefixHelper(kExpected1, std::size(kExpected1), "starting", 8,
                         *dic);

  // Invalid input values should be just ignored.
  TestLookupPrefixHelper(nullptr, 0, "", 0, *dic);
  TestLookupPrefixHelper(nullptr, 0, "あ\x81\x84う", 8, *dic);

  // Kanji is also a valid key chararcter.
  const Entry kExpected2[] = {
      {"水雲", "value", 100, 100},
  };
  TestLookupPrefixHelper(kExpected2, std::size(kExpected2), "水雲", 6, *dic);

  // Make a change to the dictionary file and load it again.
  {
    UserDictionaryStorage storage("");
    LoadFromString(kUserDictionary1, &storage);
    dic->Load(storage.GetProto());
  }

  // A normal lookup.
  const Entry kExpected3[] = {
      {"end", "end", 200, 200},
      {"ending", "ending", 220, 220},
  };
  TestLookupPrefixHelper(kExpected3, std::size(kExpected3), "ending", 6, *dic);

  // Lookup for entries which are gone should returns empty result.
  TestLookupPrefixHelper(nullptr, 0, "started", 7, *dic);
  TestLookupPrefixHelper(nullptr, 0, "starting", 8, *dic);
}

TEST_F(UserDictionaryTest, TestLookupExact) {
  std::unique_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage.GetProto());
  }

  // A normal lookup operation.
  const Entry kExpected0[] = {
      {"start", "start", 200, 200},
  };
  TestLookupExactHelper(kExpected0, std::size(kExpected0), "start", 5, *dic);

  // Another normal lookup operation.
  const Entry kExpected1[] = {
      {"starting", "starting", 100, 100},
      {"starting", "starting", 220, 220},
  };
  TestLookupExactHelper(kExpected1, std::size(kExpected1), "starting", 8, *dic);

  // Invalid input values should be just ignored.
  TestLookupExactHelper(nullptr, 0, "", 0, *dic);
  TestLookupExactHelper(nullptr, 0, "あ\x81\x84う", 8, *dic);

  // Kanji is also a valid key chararcter.
  const Entry kExpected2[] = {
      {"水雲", "value", 100, 100},
  };
  TestLookupExactHelper(kExpected2, std::size(kExpected2), "水雲", 6, *dic);
}

TEST_F(UserDictionaryTest, TestLookupExactWithSuggestionOnlyWords) {
  std::unique_ptr<UserDictionary> user_dic(CreateDictionary());
  user_dic->WaitForReloader();

  // Create dictionary
  const std::string filename = FileUtil::JoinPath(
      absl::GetFlag(FLAGS_test_tmpdir), "suggestion_only_test.db");
  EXPECT_OK(FileUtil::UnlinkIfExists(filename));
  UserDictionaryStorage storage(filename);
  {
    uint64_t id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.GetProto().mutable_dictionaries(0);

    // "名詞"
    UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
    entry->set_key("key");
    entry->set_value("noun");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);

    // "サジェストのみ"
    entry = dic->add_entries();
    entry->set_key("key");
    entry->set_value("suggest_only");
    entry->set_pos(user_dictionary::UserDictionary::SUGGESTION_ONLY);

    user_dic->Load(storage.GetProto());
  }

  // "suggestion_only" should not be looked up.
  const testing::MockDataManager mock_data_manager;
  const dictionary::PosMatcher pos_matcher(
      mock_data_manager.GetPosMatcherData());
  const uint16_t kNounId = pos_matcher.GetGeneralNounId();
  const Entry kExpected1[] = {{"key", "noun", kNounId, kNounId}};
  TestLookupExactHelper(kExpected1, std::size(kExpected1), "key", 3, *user_dic);
}

TEST_F(UserDictionaryTest, TestLookupWighShortCut) {
  std::unique_ptr<UserDictionary> user_dic(CreateDictionary());
  user_dic->WaitForReloader();

  // Create dictionary
  const std::string filename =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "shortcut_test.db");
  EXPECT_OK(FileUtil::UnlinkIfExists(filename));
  UserDictionaryStorage storage(filename);
  {
    uint64_t id = 0;
    // Creates the shortcut dictionary.
    EXPECT_TRUE(storage.CreateDictionary(
        "__auto_imported_android_shortcuts_dictionary", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.GetProto().mutable_dictionaries(0);

    // "名詞"
    UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
    entry->set_key("key");
    entry->set_value("noun");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);

    // SUGGESTION ONLY word is handled as SHORTCUT word.
    entry = dic->add_entries();
    entry->set_key("key");
    entry->set_value("suggest_only");
    entry->set_pos(user_dictionary::UserDictionary::SUGGESTION_ONLY);

    user_dic->Load(storage.GetProto());
  }

  // shortcut words are looked up.
  const testing::MockDataManager mock_data_manager;
  const dictionary::PosMatcher pos_matcher(
      mock_data_manager.GetPosMatcherData());
  const uint16_t kNounId = pos_matcher.GetGeneralNounId();
  const uint16_t kUnknownId = pos_matcher.GetUnknownId();
  const Entry kExpected2[] = {
      {"key", "noun", kNounId, kNounId},
      {"key", "suggest_only", kUnknownId, kUnknownId},
  };
  TestLookupExactHelper(kExpected2, std::size(kExpected2), "key", 3, *user_dic);
  TestLookupPredictiveHelper(kExpected2, std::size(kExpected2), "ke",
                             *user_dic);
  TestLookupPrefixHelper(kExpected2, std::size(kExpected2), "keykey", 3,
                         *user_dic);
}

TEST_F(UserDictionaryTest, IncognitoModeTest) {
  config_.set_incognito_mode(true);
  std::unique_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    UserDictionaryTest::LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage.GetProto());
  }

  TestLookupPrefixHelper(nullptr, 0, "start", 4, *dic);
  TestLookupPredictiveHelper(nullptr, 0, "s", *dic);

  config_.set_incognito_mode(false);
  {
    EntryCollector collector;
    dic->LookupPrefix("start", convreq_, &collector);
    EXPECT_FALSE(collector.entries().empty());
  }
  {
    EntryCollector collector;
    dic->LookupPredictive("s", convreq_, &collector);
    EXPECT_FALSE(collector.entries().empty());
  }
}

TEST_F(UserDictionaryTest, AsyncLoadTest) {
  const std::string filename = FileUtil::JoinPath(
      absl::GetFlag(FLAGS_test_tmpdir), "async_load_test.db");
  EXPECT_OK(FileUtil::UnlinkIfExists(filename));
  Random random;

  // Create dictionary
  std::vector<std::string> keys;
  {
    UserDictionaryStorage storage(filename);

    EXPECT_FALSE(storage.Load().ok());
    EXPECT_TRUE(storage.Lock());

    uint64_t id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.GetProto().mutable_dictionaries(0);
    for (size_t j = 0; j < 10000; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
      entry->set_key(random.Utf8StringRandomLen(10, 'a', 'z'));
      entry->set_value(random.Utf8StringRandomLen(10, 'a', 'z'));
      entry->set_pos(user_dictionary::UserDictionary::NOUN);
      entry->set_comment(random.Utf8StringRandomLen(10, 'a', 'z'));
      keys.push_back(entry->key());
    }
    EXPECT_OK(storage.Save());
    EXPECT_TRUE(storage.UnLock());
  }

  {
    std::unique_ptr<UserDictionary> dic(CreateDictionary());
    // Wait for async reload called from the constructor.
    dic->WaitForReloader();
    dic->SetUserDictionaryName(filename);

    for (int i = 0; i < 32; ++i) {
      std::shuffle(keys.begin(), keys.end(), random);
      dic->Reload();
      for (int i = 0; i < 1000; ++i) {
        CollectTokenCallback callback;
        dic->LookupPrefix(keys[i], convreq_, &callback);
      }
    }
    dic->WaitForReloader();
  }
  EXPECT_OK(FileUtil::UnlinkIfExists(filename));
}

TEST_F(UserDictionaryTest, TestSuppressionDictionary) {
  std::unique_ptr<UserDictionary> user_dic(CreateDictionaryWithMockPos());
  user_dic->WaitForReloader();

  const std::string filename = FileUtil::JoinPath(
      absl::GetFlag(FLAGS_test_tmpdir), "suppression_test.db");
  ASSERT_OK(FileUtil::UnlinkIfExists(filename));

  UserDictionaryStorage storage(filename);

  // Create dictionary
  {
    uint64_t id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.GetProto().mutable_dictionaries(0);
    for (size_t j = 0; j < 10000; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
      entry->set_key("no_suppress_key" +
                     std::to_string(static_cast<uint32_t>(j)));
      entry->set_value("no_suppress_value" +
                       std::to_string(static_cast<uint32_t>(j)));
      entry->set_pos(user_dictionary::UserDictionary::NOUN);
    }

    for (size_t j = 0; j < 10; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
      entry->set_key("suppress_key" + std::to_string(static_cast<uint32_t>(j)));
      entry->set_value("suppress_value" +
                       std::to_string(static_cast<uint32_t>(j)));
      // entry->set_pos("抑制単語");
      entry->set_pos(user_dictionary::UserDictionary::SUPPRESSION_WORD);
    }

    user_dic->Load(storage.GetProto());

    for (size_t j = 0; j < 10; ++j) {
      EXPECT_TRUE(suppression_dictionary_->SuppressEntry(
          "suppress_key" + std::to_string(static_cast<uint32_t>(j)),
          "suppress_value" + std::to_string(static_cast<uint32_t>(j))));
    }
  }

  // Remove suppression entry
  {
    storage.GetProto().Clear();
    uint64_t id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.GetProto().mutable_dictionaries(0);
    for (size_t j = 0; j < 10000; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
      entry->set_key("no_suppress_key" +
                     std::to_string(static_cast<uint32_t>(j)));
      entry->set_value("no_suppress_value" +
                       std::to_string(static_cast<uint32_t>(j)));
      entry->set_pos(user_dictionary::UserDictionary::NOUN);
    }

    user_dic->Load(storage.GetProto());

    for (size_t j = 0; j < 10; ++j) {
      EXPECT_FALSE(suppression_dictionary_->SuppressEntry(
          "suppress_key" + std::to_string(static_cast<uint32_t>(j)),
          "suppress_value" + std::to_string(static_cast<uint32_t>(j))));
    }
  }
  EXPECT_OK(FileUtil::UnlinkIfExists(filename));
}

TEST_F(UserDictionaryTest, TestSuggestionOnlyWord) {
  std::unique_ptr<UserDictionary> user_dic(CreateDictionary());
  user_dic->WaitForReloader();

  const std::string filename = FileUtil::JoinPath(
      absl::GetFlag(FLAGS_test_tmpdir), "suggestion_only_test.db");
  ASSERT_OK(FileUtil::UnlinkIfExists(filename));

  UserDictionaryStorage storage(filename);

  // Create dictionary
  {
    uint64_t id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.GetProto().mutable_dictionaries(0);

    for (size_t j = 0; j < 10; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
      entry->set_key("key" + std::to_string(static_cast<uint32_t>(j)));
      entry->set_value("default");
      // "名詞"
      entry->set_pos(user_dictionary::UserDictionary::NOUN);
    }

    for (size_t j = 0; j < 10; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry = dic->add_entries();
      entry->set_key("key" + std::to_string(static_cast<uint32_t>(j)));
      entry->set_value("suggest_only");
      // "サジェストのみ"
      entry->set_pos(user_dictionary::UserDictionary::SUGGESTION_ONLY);
    }

    user_dic->Load(storage.GetProto());
  }

  {
    constexpr char kKey[] = "key0123";
    CollectTokenCallback callback;
    user_dic->LookupPrefix(kKey, convreq_, &callback);
    const std::vector<Token> &tokens = callback.tokens();
    for (size_t i = 0; i < tokens.size(); ++i) {
      EXPECT_EQ(tokens[i].value, "default");
    }
  }
  {
    constexpr char kKey[] = "key";
    CollectTokenCallback callback;
    user_dic->LookupPredictive(kKey, convreq_, &callback);
    const std::vector<Token> &tokens = callback.tokens();
    for (size_t i = 0; i < tokens.size(); ++i) {
      EXPECT_TRUE(tokens[i].value == "suggest_only" ||
                  tokens[i].value == "default");
    }
  }

  EXPECT_OK(FileUtil::UnlinkIfExists(filename));
}

TEST_F(UserDictionaryTest, TestUsageStats) {
  std::unique_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();
  UserDictionaryStorage storage("");

  {
    UserDictionaryStorage::UserDictionary *dic1 =
        storage.GetProto().add_dictionaries();
    CHECK(dic1);
    UserDictionaryStorage::UserDictionaryEntry *entry;
    entry = dic1->add_entries();
    CHECK(entry);
    entry->set_key("key1");
    entry->set_value("value1");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
    entry = dic1->add_entries();
    CHECK(entry);
    entry->set_key("key2");
    entry->set_value("value2");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
  }
  {
    UserDictionaryStorage::UserDictionary *dic2 =
        storage.GetProto().add_dictionaries();
    CHECK(dic2);
    UserDictionaryStorage::UserDictionaryEntry *entry;
    entry = dic2->add_entries();
    CHECK(entry);
    entry->set_key("key3");
    entry->set_value("value3");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
    entry = dic2->add_entries();
    CHECK(entry);
    entry->set_key("key4");
    entry->set_value("value4");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
    entry = dic2->add_entries();
    CHECK(entry);
    entry->set_key("key5");
    entry->set_value("value5");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
  }
  dic->Load(storage.GetProto());

  EXPECT_INTEGER_STATS("UserRegisteredWord", 5);
}

TEST_F(UserDictionaryTest, LookupComment) {
  std::unique_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    UserDictionaryTest::LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage.GetProto());
  }

  // Entry is in user dictionary but has no comment.
  std::string comment;
  comment = "prev comment";
  EXPECT_FALSE(
      dic->LookupComment("comment_key1", "comment_value2", convreq_, &comment));
  EXPECT_EQ(comment, "prev comment");

  // Usual case: single key-value pair with comment.
  EXPECT_TRUE(
      dic->LookupComment("comment_key2", "comment_value2", convreq_, &comment));
  EXPECT_EQ(comment, "comment");

  // There exist two entries having the same key, value and POS.  Since POS is
  // irrelevant to comment lookup, the first nonempty comment should be found.
  EXPECT_TRUE(
      dic->LookupComment("comment_key3", "comment_value3", convreq_, &comment));
  EXPECT_EQ(comment, "comment1");

  // White-space only comments should be cleared.
  EXPECT_FALSE(
      dic->LookupComment("comment_key4", "comment_value4", convreq_, &comment));
  // The previous comment should remain.
  EXPECT_EQ(comment, "comment1");

  // Comment should be found iff key and value match.
  EXPECT_TRUE(LookupComment(*dic, "comment_key", "mismatching_value").empty());
  EXPECT_TRUE(LookupComment(*dic, "comment_key1", "mismatching_value").empty());
  EXPECT_TRUE(LookupComment(*dic, "comment_key2", "mismatching_value").empty());
  EXPECT_TRUE(LookupComment(*dic, "comment_key3", "mismatching_value").empty());
  EXPECT_TRUE(LookupComment(*dic, "comment_key4", "mismatching_value").empty());
  EXPECT_TRUE(LookupComment(*dic, "mismatching_key", "comment_value").empty());
  EXPECT_TRUE(LookupComment(*dic, "mismatching_key", "comment_value1").empty());
  EXPECT_TRUE(LookupComment(*dic, "mismatching_key", "comment_value2").empty());
  EXPECT_TRUE(LookupComment(*dic, "mismatching_key", "comment_value3").empty());
  EXPECT_TRUE(LookupComment(*dic, "mismatching_key", "comment_value4").empty());
}

TEST_F(UserDictionaryTest, TestPopulateTokenFromUserPosToken) {
  std::unique_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  dic->WaitForReloader();

  const testing::MockDataManager mock_data_manager;
  const dictionary::PosMatcher pos_matcher(
      mock_data_manager.GetPosMatcherData());

  UserPosInterface::Token user_token;
  user_token.key = "key";
  user_token.value = "value";
  user_token.id = 10;

  Token token;

  dic->PopulateTokenFromUserPosToken(user_token, UserDictionary::PREFIX,
                                     &token);
  EXPECT_EQ(token.key, "key");
  EXPECT_EQ(token.value, "value");
  EXPECT_EQ(token.lid, 10);
  EXPECT_EQ(token.rid, 10);
  EXPECT_EQ(token.cost, 5000);
  EXPECT_EQ(token.attributes, Token::USER_DICTIONARY);

  user_token.add_attribute(UserPos::Token::NON_JA_LOCALE);
  dic->PopulateTokenFromUserPosToken(user_token, UserDictionary::PREFIX,
                                     &token);
  EXPECT_EQ(token.cost, 10000);

  user_token.attributes = 0;
  user_token.add_attribute(UserPos::Token::ISOLATED_WORD);
  dic->PopulateTokenFromUserPosToken(user_token, UserDictionary::PREFIX,
                                     &token);
  EXPECT_EQ(token.cost, 200);

  user_token.attributes = 0;
  user_token.add_attribute(UserPos::Token::SUGGESTION_ONLY);
  dic->PopulateTokenFromUserPosToken(
      user_token, UserDictionary::UserDictionary::PREFIX, &token);
  EXPECT_EQ(token.lid, pos_matcher.GetUnknownId());
  EXPECT_EQ(token.rid, pos_matcher.GetUnknownId());
  EXPECT_EQ(token.cost, 5000);

  user_token.attributes = 0;
  user_token.add_attribute(UserPos::Token::SHORTCUT);
  dic->PopulateTokenFromUserPosToken(user_token, UserDictionary::PREDICTIVE,
                                     &token);
  EXPECT_EQ(token.lid, pos_matcher.GetUnknownId());
  EXPECT_EQ(token.rid, pos_matcher.GetUnknownId());
  EXPECT_EQ(token.cost, 5000);

  user_token.attributes = 0;

  user_token.key = "a";  // one char
  user_token.add_attribute(UserPos::Token::SHORTCUT);
  dic->PopulateTokenFromUserPosToken(user_token, UserDictionary::PREFIX,
                                     &token);
  EXPECT_EQ(token.cost, 5000 + 2000 * 3);

  user_token.key = "aa";
  user_token.add_attribute(UserPos::Token::SHORTCUT);
  dic->PopulateTokenFromUserPosToken(user_token, UserDictionary::PREFIX,
                                     &token);
  EXPECT_EQ(token.cost, 5000 + 2000 * 2);

  user_token.key = "aaa";
  user_token.add_attribute(UserPos::Token::SHORTCUT);
  dic->PopulateTokenFromUserPosToken(user_token, UserDictionary::PREFIX,
                                     &token);
  EXPECT_EQ(token.cost, 5000 + 2000);

  user_token.key = "aaaa";
  user_token.add_attribute(UserPos::Token::SHORTCUT);
  dic->PopulateTokenFromUserPosToken(user_token, UserDictionary::PREFIX,
                                     &token);
  EXPECT_EQ(token.cost, 5000);

  user_token.key = "aaaaaaa";
  user_token.add_attribute(UserPos::Token::SHORTCUT);
  dic->PopulateTokenFromUserPosToken(user_token, UserDictionary::PREFIX,
                                     &token);
  EXPECT_EQ(token.cost, 5000);
}
}  // namespace
}  // namespace dictionary
}  // namespace mozc

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
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "base/random.h"
#include "base/system_util.h"
#include "base/thread.h"
#include "config/config_handler.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/dictionary_test_util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_pos.h"
#include "protocol/config.pb.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace dictionary {
namespace {

using ::mozc::dictionary::DictionaryInterface;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AnyOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Return;
using ::testing::UnorderedElementsAreArray;

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
                   std::vector<UserPos::Token>* tokens) {
  tokens->push_back(UserPos::Token{
      .key = std::string(key),
      .value = std::string(value),
      .id = id,
  });
}

// This class is a mock class for writing unit tests of a class that
// depends on POS. It accepts only two values for part-of-speech:
// "noun" as words without inflection and "verb" as words with
// inflection.
class UserPosMock : public UserPos {
 public:
  // This method returns true if the given pos is "noun" or "verb".
  bool IsValidPos(absl::string_view pos) const override { return true; }

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
                 std::vector<UserPos::Token>* tokens) const override {
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

  std::vector<std::string> GetPosList() const override {
    return {{kNoun.data(), kNoun.size()}};
  }
  int GetPosListDefaultIndex() const override { return 0; }

  bool GetPosIds(absl::string_view pos, uint16_t* id) const override {
    return false;
  }

  static constexpr absl::string_view kNoun = "名詞";
  static constexpr absl::string_view kVerb = "動詞ワ行五段";
};

class UserDictionaryTest : public testing::TestWithTempUserProfile {
 protected:
  UserDictionaryTest() { config::ConfigHandler::GetDefaultConfig(&config_); }

  ~UserDictionaryTest() override {
    // This config initialization will be removed once ConversionRequest can
    // take config as an injected argument.
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  // Workaround for the constructor of UserDictionary being protected.
  // Creates a user dictionary with mock pos data.
  std::unique_ptr<UserDictionary> CreateDictionaryWithMockPos() {
    return std::make_unique<UserDictionary>(
        std::make_unique<UserPosMock>(),
        dictionary::PosMatcher(mock_data_manager_.GetPosMatcherData()));
  }

  // Creates a user dictionary with actual pos data.
  std::unique_ptr<UserDictionary> CreateDictionary() {
    return std::make_unique<UserDictionary>(
        UserPos::CreateFromDataManager(mock_data_manager_),
        dictionary::PosMatcher(mock_data_manager_.GetPosMatcherData()));
  }

  std::unique_ptr<UserDictionary> CreateDictionaryWithFilename(
      std::string filename) {
    return std::make_unique<UserDictionary>(
        UserPos::CreateFromDataManager(mock_data_manager_),
        dictionary::PosMatcher(mock_data_manager_.GetPosMatcherData()),
        std::move(filename));
  }

  ConversionRequest ConvReq(const config::Config& config) {
    return ConversionRequestBuilder().SetConfig(config).Build();
  }

  struct Entry {
    std::string key;
    std::string value;
    uint16_t lid;
    uint16_t rid;

    bool operator==(const Entry& rhs) const {
      return std::tie(key, value, lid, rid) ==
             std::tie(rhs.key, rhs.value, rhs.lid, rhs.rid);
    }

    template <typename Sink>
    friend void AbslStringify(Sink& sink, const Entry& entry) {
      absl::Format(&sink, "key = %s, value = %s, lid = %d, rid = %d", entry.key,
                   entry.value, entry.lid, entry.rid);
    }
  };

  class EntryCollector : public DictionaryInterface::Callback {
   public:
    ResultType OnToken(absl::string_view,  // key
                       absl::string_view,  // actual_key
                       const Token& token) override {
      // Collect only user dictionary entries.
      if (token.attributes & Token::USER_DICTIONARY) {
        entries_.push_back(Entry({.key = token.key,
                                  .value = token.value,
                                  .lid = static_cast<uint16_t>(token.lid),
                                  .rid = static_cast<uint16_t>(token.rid)}));
      }
      return TRAVERSE_CONTINUE;
    }

    std::vector<Entry>&& entries() && { return std::move(entries_); }

   private:
    std::vector<Entry> entries_;
  };

  std::vector<Entry> LookupPredictive(absl::string_view key,
                                      const UserDictionary& dic) {
    EntryCollector collector;
    const ConversionRequest convreq = ConvReq(config_);
    dic.LookupPredictive(key, convreq, &collector);
    return std::move(collector).entries();
  }

  std::vector<Entry> LookupPrefix(absl::string_view key,
                                  const UserDictionary& dic) {
    EntryCollector collector;
    const ConversionRequest convreq = ConvReq(config_);
    dic.LookupPrefix(key, convreq, &collector);
    return std::move(collector).entries();
  }

  std::vector<Entry> LookupExact(absl::string_view key,
                                 const UserDictionary& dic) {
    EntryCollector collector;
    const ConversionRequest convreq = ConvReq(config_);
    dic.LookupExact(key, convreq, &collector);
    return std::move(collector).entries();
  }

  static void LoadFromString(const std::string& contents,
                             UserDictionaryStorage* storage) {
    std::istringstream is(contents);
    CHECK(is.good());

    storage->GetProto().Clear();
    UserDictionaryStorage::UserDictionary* dic =
        storage->GetProto().add_dictionaries();
    CHECK(dic);

    std::string line;
    while (!std::getline(is, line).fail()) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      std::vector<absl::string_view> fields =
          absl::StrSplit(line, '\t', absl::AllowEmpty());
      EXPECT_GE(fields.size(), 3) << line;
      UserDictionaryStorage::UserDictionaryEntry* entry = dic->add_entries();
      CHECK(entry);
      entry->set_key(fields[0]);
      entry->set_value(fields[1]);
      if (fields[2] == "verb") {
        entry->set_pos(user_dictionary::UserDictionary::WA_GROUP1_VERB);
      } else if (fields[2] == "noun") {
        entry->set_pos(user_dictionary::UserDictionary::NOUN);
      }
      if (fields.size() >= 4 && !fields[3].empty()) {
        entry->set_comment(fields[3]);
      }
    }
  }

  // Helper function to lookup comment string from |dic|.
  std::string LookupComment(const UserDictionary& dic, absl::string_view key,
                            absl::string_view value) {
    std::string comment;
    const ConversionRequest convreq = ConvReq(config_);
    dic.LookupComment(key, value, convreq, &comment);
    return comment;
  }

  config::Config config_;

 private:
  const testing::MockDataManager mock_data_manager_;
};

TEST_F(UserDictionaryTest, TestLookupPredictiveCallback) {
  std::unique_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    UserDictionaryTest::LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage.GetProto());
  }

  MockCallback mock_callback;
  EXPECT_CALL(mock_callback, OnKey(_))
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
  EXPECT_CALL(mock_callback, OnActualKey(_, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
  EXPECT_CALL(mock_callback, OnToken(_, _, _))
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));

  EXPECT_CALL(mock_callback, OnKey(Eq("start")))
      .Times(1)
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
  EXPECT_CALL(mock_callback, OnActualKey(Eq("start"), Eq("start"), Eq(0)))
      .Times(1)
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
  EXPECT_CALL(mock_callback, OnToken(Eq("start"), Eq("start"), _))
      .Times(1)
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));

  const ConversionRequest convreq = ConvReq(config_);
  dic->LookupPredictive("start", convreq, &mock_callback);
}

TEST_F(UserDictionaryTest, TestLookupExactCallback) {
  std::unique_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    UserDictionaryTest::LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage.GetProto());
  }

  MockCallback mock_callback;
  EXPECT_CALL(mock_callback, OnKey(Eq("start")))
      .Times(1)
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
  EXPECT_CALL(mock_callback, OnActualKey(Eq("start"), Eq("start"), Eq(0)))
      .Times(1)
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
  EXPECT_CALL(mock_callback, OnToken(Eq("start"), Eq("start"), _))
      .Times(1)
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));

  const ConversionRequest convreq = ConvReq(config_);
  dic->LookupExact("start", convreq, &mock_callback);
}

TEST_F(UserDictionaryTest, TestLookupPrefixCallback) {
  std::unique_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    UserDictionaryTest::LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage.GetProto());
  }

  MockCallback mock_callback;
  EXPECT_CALL(mock_callback, OnKey(_))
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
  EXPECT_CALL(mock_callback, OnActualKey(_, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
  EXPECT_CALL(mock_callback, OnToken(_, _, _))
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));

  EXPECT_CALL(mock_callback, OnKey(Eq("start")))
      .Times(1)
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
  EXPECT_CALL(mock_callback, OnActualKey(Eq("start"), Eq("start"), Eq(0)))
      .Times(1)
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
  EXPECT_CALL(mock_callback, OnToken(Eq("start"), Eq("start"), _))
      .Times(1)
      .WillRepeatedly(Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));

  const ConversionRequest convreq = ConvReq(config_);
  dic->LookupPrefix("start", convreq, &mock_callback);
}

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
  EXPECT_THAT(LookupPredictive("start", *dic),
              UnorderedElementsAreArray(kExpected0));

  // Another normal lookup operation.
  const Entry kExpected1[] = {
      {"stamp", "stamp", 100, 100},       {"stand", "stand", 200, 200},
      {"standed", "standed", 210, 210},   {"standing", "standing", 220, 220},
      {"star", "star", 100, 100},         {"start", "start", 200, 200},
      {"started", "started", 210, 210},   {"starting", "starting", 100, 100},
      {"starting", "starting", 220, 220},
  };
  EXPECT_THAT(LookupPredictive("st", *dic),
              UnorderedElementsAreArray(kExpected1));

  // Invalid input values should be just ignored.
  EXPECT_THAT(LookupPredictive("", *dic), IsEmpty());
  EXPECT_THAT(LookupPredictive("あ\x81\x84う", *dic), IsEmpty());

  // Kanji is also a valid key chararcter.
  EXPECT_THAT(LookupPredictive("水雲", *dic),
              ElementsAre(Entry{"水雲", "value", 100, 100}));

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
  EXPECT_THAT(LookupPredictive("end", *dic),
              UnorderedElementsAreArray(kExpected3));

  // Entries in the dictionary before reloading cannot be looked up.
  EXPECT_THAT(LookupPredictive("start", *dic), IsEmpty());
  EXPECT_THAT(LookupPredictive("st", *dic), IsEmpty());
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
  EXPECT_THAT(LookupPrefix("started", *dic),
              UnorderedElementsAreArray(kExpected0));

  // Another normal lookup operation.
  const Entry kExpected1[] = {
      {"star", "star", 100, 100},
      {"start", "start", 200, 200},
      {"starting", "starting", 100, 100},
      {"starting", "starting", 220, 220},
  };
  EXPECT_THAT(LookupPrefix("starting", *dic),
              UnorderedElementsAreArray(kExpected1));

  // Invalid input values should be just ignored.
  EXPECT_THAT(LookupPrefix("", *dic), IsEmpty());
  EXPECT_THAT(LookupPrefix("あ\x81\x84う", *dic), IsEmpty());

  // Kanji is also a valid key chararcter.
  EXPECT_THAT(LookupPrefix("水雲", *dic),
              ElementsAre(Entry{"水雲", "value", 100, 100}));

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
  EXPECT_THAT(LookupPrefix("ending", *dic),
              UnorderedElementsAreArray(kExpected3));

  // Lookup for entries which are gone should returns empty result.
  EXPECT_THAT(LookupPrefix("started", *dic), IsEmpty());
  EXPECT_THAT(LookupPrefix("starting", *dic), IsEmpty());
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
  EXPECT_THAT(LookupExact("start", *dic),
              ElementsAre(Entry{"start", "start", 200, 200}));

  // Another normal lookup operation.
  const Entry kExpected1[] = {
      {"starting", "starting", 100, 100},
      {"starting", "starting", 220, 220},
  };
  EXPECT_THAT(LookupExact("starting", *dic),
              UnorderedElementsAreArray(kExpected1));

  // Invalid input values should be just ignored.
  EXPECT_THAT(LookupExact("", *dic), IsEmpty());
  EXPECT_THAT(LookupExact("あ\x81\x84う", *dic), IsEmpty());

  // Kanji is also a valid key chararcter.
  EXPECT_THAT(LookupExact("水雲", *dic),
              ElementsAre(Entry{"水雲", "value", 100, 100}));
}

TEST_F(UserDictionaryTest, TestLookupExactWithSuggestionOnlyWords) {
  std::unique_ptr<UserDictionary> user_dic(CreateDictionary());
  user_dic->WaitForReloader();

  // Create dictionary
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string filename =
      FileUtil::JoinPath(temp_dir.path(), "suggestion_only_test.db");
  UserDictionaryStorage storage(filename);
  {
    EXPECT_OK(storage.CreateDictionary("test"));
    UserDictionaryStorage::UserDictionary* dic =
        storage.GetProto().mutable_dictionaries(0);

    // "名詞"
    UserDictionaryStorage::UserDictionaryEntry* entry = dic->add_entries();
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
  EXPECT_THAT(LookupExact("key", *user_dic),
              ElementsAre(Entry{"key", "noun", kNounId, kNounId}));
}

TEST_F(UserDictionaryTest, TestLookupWithShortCut) {
  std::unique_ptr<UserDictionary> user_dic(CreateDictionary());
  user_dic->WaitForReloader();

  // Create dictionary
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string filename =
      FileUtil::JoinPath(temp_dir.path(), "shortcut_test.db");
  UserDictionaryStorage storage(filename);
  {
    // Creates the shortcut dictionary (from Gboard Android).
    EXPECT_OK(storage.CreateDictionary(
        "__auto_imported_android_shortcuts_dictionary"));
    UserDictionaryStorage::UserDictionary* dic =
        storage.GetProto().mutable_dictionaries(0);

    // "名詞"
    UserDictionaryStorage::UserDictionaryEntry* entry = dic->add_entries();
    entry->set_key("key");
    entry->set_value("noun");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);

    // SUGGESTION ONLY word is handled as SHORTCUT word.
    entry = dic->add_entries();
    entry->set_key("key");
    entry->set_value("suggest_only");
    entry->set_pos(user_dictionary::UserDictionary::SUGGESTION_ONLY);

    // NO POS word is handled as SHORTCUT word.
    entry = dic->add_entries();
    entry->set_key("key");
    entry->set_value("no_pos");
    entry->set_pos(user_dictionary::UserDictionary::NO_POS);

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
      {"key", "no_pos", kUnknownId, kUnknownId},
      {"key", "suggest_only", kUnknownId, kUnknownId},
  };
  const Entry kExpectedPrediction[] = {
      {"key", "noun", kNounId, kNounId},
      {"key", "no_pos", kUnknownId, kUnknownId},
      {"key", "suggest_only", kUnknownId, kUnknownId},
  };

  EXPECT_THAT(LookupExact("key", *user_dic),
              UnorderedElementsAreArray(kExpected2));
  EXPECT_THAT(LookupPredictive("ke", *user_dic),
              UnorderedElementsAreArray(kExpectedPrediction));
  EXPECT_THAT(LookupPrefix("key", *user_dic),
              UnorderedElementsAreArray(kExpected2));
}

TEST_F(UserDictionaryTest, TestKeyNormalization) {
  std::unique_ptr<UserDictionary> user_dic(CreateDictionary());
  user_dic->WaitForReloader();

  // Create dictionary
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string filename =
      FileUtil::JoinPath(temp_dir.path(), "normalization_test.db");
  UserDictionaryStorage storage(filename);
  {
    EXPECT_OK(storage.CreateDictionary("normalization_test"));
    UserDictionaryStorage::UserDictionary* dic =
        storage.GetProto().mutable_dictionaries(0);

    // "名詞"(NOUN)
    // Full width will be normalized to half width.
    UserDictionaryStorage::UserDictionaryEntry* entry = dic->add_entries();
    entry->set_key("１％");
    entry->set_value("１％");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);

    // Half width katakana will be normalized to full width hiragana.
    entry = dic->add_entries();
    entry->set_key("ｸﾞｰｸﾞﾙ");
    entry->set_value("Google");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);

    // NO POS word is handled as SHORTCUT word.
    // voiced sound mark in「う゛」 will be normalized to be 「ゔ」.
    entry = dic->add_entries();
    entry->set_key("う゛ぃくとりー");
    entry->set_value("ヴィクトリー");
    entry->set_pos(user_dictionary::UserDictionary::NO_POS);

    // Katakana will be normalized to hiragana.
    entry = dic->add_entries();
    entry->set_key("ジーボード");
    entry->set_value("Gboard");
    entry->set_pos(user_dictionary::UserDictionary::NO_POS);

    user_dic->Load(storage.GetProto());
  }

  EXPECT_EQ(LookupExact("1%", *user_dic).size(), 1);
  EXPECT_EQ(LookupExact("１％", *user_dic).size(), 0);

  EXPECT_EQ(LookupExact("ぐーぐる", *user_dic).size(), 1);
  EXPECT_EQ(LookupExact("グーグル", *user_dic).size(), 0);
  EXPECT_EQ(LookupExact("ｸﾞｰｸﾞﾙ", *user_dic).size(), 0);

  EXPECT_EQ(LookupExact("ゔぃくとりー", *user_dic).size(), 1);
  EXPECT_EQ(LookupExact("う゛ぃくとりー", *user_dic).size(), 0);
  EXPECT_EQ(LookupExact("ヴィクトリー", *user_dic).size(), 0);

  EXPECT_EQ(LookupExact("じーぼーど", *user_dic).size(), 1);
  EXPECT_EQ(LookupExact("ジーボード", *user_dic).size(), 0);
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

  EXPECT_THAT(LookupPrefix("star", *dic), IsEmpty());
  EXPECT_THAT(LookupPredictive("s", *dic), IsEmpty());

  config_.set_incognito_mode(false);
  EXPECT_THAT(LookupPrefix("start", *dic), Not(IsEmpty()));
  EXPECT_THAT(LookupPredictive("s", *dic), Not(IsEmpty()));
}

TEST_F(UserDictionaryTest, AsyncLoadTest) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string filename =
      FileUtil::JoinPath(temp_dir.path(), "async_load_test.db");
  Random random;

  // Create dictionary
  std::vector<std::string> keys;
  {
    UserDictionaryStorage storage(filename);

    EXPECT_FALSE(storage.Load().ok());
    EXPECT_TRUE(storage.Lock());

    EXPECT_OK(storage.CreateDictionary("test"));
    UserDictionaryStorage::UserDictionary* dic =
        storage.GetProto().mutable_dictionaries(0);
    for (size_t j = 0; j < 10000; ++j) {
      UserDictionaryStorage::UserDictionaryEntry* entry = dic->add_entries();
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
    std::unique_ptr<UserDictionary> dic(CreateDictionaryWithFilename(filename));
    // Wait for async reload called from the constructor.
    dic->WaitForReloader();

    for (int i = 0; i < 32; ++i) {
      std::shuffle(keys.begin(), keys.end(), random);
      dic->Reload();
      for (int i = 0; i < 1000; ++i) {
        CollectTokenCallback callback;
        const ConversionRequest convreq = ConvReq(config_);
        dic->LookupPrefix(keys[i], convreq, &callback);
        EXPECT_FALSE(callback.tokens().empty());
      }
    }
    dic->WaitForReloader();
  }

  // Fix b//341758719. Waits the reload inside the destructor.
  {
    std::unique_ptr<UserDictionary> dic(CreateDictionaryWithFilename(filename));
    dic->Reload();
  }
}

TEST_F(UserDictionaryTest, TestSuppressionDictionary) {
  std::unique_ptr<UserDictionary> user_dic(CreateDictionaryWithMockPos());
  user_dic->WaitForReloader();

  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string filename =
      FileUtil::JoinPath(temp_dir.path(), "suppression_test.db");

  UserDictionaryStorage storage(filename);

  // Create dictionary
  {
    EXPECT_OK(storage.CreateDictionary("test"));
    UserDictionaryStorage::UserDictionary* dic =
        storage.GetProto().mutable_dictionaries(0);
    for (size_t j = 0; j < 10000; ++j) {
      UserDictionaryStorage::UserDictionaryEntry* entry = dic->add_entries();
      entry->set_key(absl::StrCat("no_suppress_key", j));
      entry->set_value(absl::StrCat("no_suppress_value", j));
      entry->set_pos(user_dictionary::UserDictionary::NOUN);
    }

    for (size_t j = 0; j < 10; ++j) {
      UserDictionaryStorage::UserDictionaryEntry* entry = dic->add_entries();
      entry->set_key(absl::StrCat("suppress_key", j));
      entry->set_value(absl::StrCat("suppress_value", j));
      // entry->set_pos("抑制単語");
      entry->set_pos(user_dictionary::UserDictionary::SUPPRESSION_WORD);
    }

    user_dic->Load(storage.GetProto());

    for (size_t j = 0; j < 10; ++j) {
      EXPECT_TRUE(user_dic->IsSuppressedEntry(
          absl::StrCat("suppress_key", j), absl::StrCat("suppress_value", j)));
    }
  }

  // Remove suppression entry
  {
    storage.GetProto().Clear();
    EXPECT_OK(storage.CreateDictionary("test"));
    UserDictionaryStorage::UserDictionary* dic =
        storage.GetProto().mutable_dictionaries(0);
    for (size_t j = 0; j < 10000; ++j) {
      UserDictionaryStorage::UserDictionaryEntry* entry = dic->add_entries();
      entry->set_key(absl::StrCat("no_suppress_key", j));
      entry->set_value(absl::StrCat("no_suppress_value", j));
      entry->set_pos(user_dictionary::UserDictionary::NOUN);
    }

    user_dic->Load(storage.GetProto());

    for (size_t j = 0; j < 10; ++j) {
      EXPECT_FALSE(user_dic->IsSuppressedEntry(
          absl::StrCat("suppress_key", j), absl::StrCat("suppress_value", j)));
    }
  }
}

TEST_F(UserDictionaryTest, TestSuggestionOnlyWord) {
  std::unique_ptr<UserDictionary> user_dic(CreateDictionary());
  user_dic->WaitForReloader();

  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string filename =
      FileUtil::JoinPath(temp_dir.path(), "suggestion_only_test.db");

  UserDictionaryStorage storage(filename);

  // Create dictionary
  {
    EXPECT_OK(storage.CreateDictionary("test"));
    UserDictionaryStorage::UserDictionary* dic =
        storage.GetProto().mutable_dictionaries(0);

    for (size_t j = 0; j < 10; ++j) {
      UserDictionaryStorage::UserDictionaryEntry* entry = dic->add_entries();
      entry->set_key(absl::StrCat("key", j));
      entry->set_value("default");
      // "名詞"
      entry->set_pos(user_dictionary::UserDictionary::NOUN);
    }

    for (size_t j = 0; j < 10; ++j) {
      UserDictionaryStorage::UserDictionaryEntry* entry = dic->add_entries();
      entry->set_key(absl::StrCat("key", j));
      entry->set_value("suggest_only");
      // "サジェストのみ"
      entry->set_pos(user_dictionary::UserDictionary::SUGGESTION_ONLY);
    }

    user_dic->Load(storage.GetProto());
  }

  {
    constexpr char kKey[] = "key0123";
    CollectTokenCallback callback;
    const ConversionRequest convreq = ConvReq(config_);
    user_dic->LookupPrefix(kKey, convreq, &callback);
    EXPECT_THAT(callback.tokens(), Each(Field(&Token::value, Eq("default"))));
  }
  {
    constexpr char kKey[] = "key";
    CollectTokenCallback callback;
    const ConversionRequest convreq = ConvReq(config_);
    user_dic->LookupPredictive(kKey, convreq, &callback);
    EXPECT_THAT(
        callback.tokens(),
        Each(Field(&Token::value, AnyOf(Eq("suggest_only"), Eq("default")))));
  }
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
  const ConversionRequest convreq = ConvReq(config_);
  EXPECT_FALSE(
      dic->LookupComment("comment_key1", "comment_value2", convreq, &comment));
  EXPECT_EQ(comment, "prev comment");

  // Usual case: single key-value pair with comment.
  EXPECT_TRUE(
      dic->LookupComment("comment_key2", "comment_value2", convreq, &comment));
  EXPECT_EQ(comment, "comment");

  // There exist two entries having the same key, value and POS.  Since POS is
  // irrelevant to comment lookup, the first nonempty comment should be found.
  EXPECT_TRUE(
      dic->LookupComment("comment_key3", "comment_value3", convreq, &comment));
  EXPECT_EQ(comment, "comment1");

  // White-space only comments should be cleared.
  EXPECT_FALSE(
      dic->LookupComment("comment_key4", "comment_value4", convreq, &comment));
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

  UserPos::Token user_token{.key = "key", .value = "value", .id = 10};

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

TEST_F(UserDictionaryTest, AsyncImportTest) {
  const std::string filename = FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(), "async_import_test.db");

  const testing::MockDataManager mock_data_manager;

  auto make_dic = [&]() {
    return UserDictionary(UserPos::CreateFromDataManager(mock_data_manager),
                          PosMatcher(mock_data_manager.GetPosMatcherData()),
                          filename);
  };

  auto dic = make_dic();

  constexpr int kThreadsSize = 100;
  constexpr int kEntrySize = 1000;

  auto make_tsv_dic = [](int i) {
    std::string tsv;
    for (int n = 0; n < kEntrySize; ++n) {
      absl::StrAppendFormat(&tsv, "key_%2.2d%4.4d\tvalue_%2.2d%4.4d\t名詞\n", i,
                            n, i, n);
    }
    // Adds one invalid line.
    absl::StrAppendFormat(&tsv, "__INVALID__");
    return tsv;
  };

  {
    // Import TSV dictionary asynchronously.
    user_dictionary::AsyncUserDictionaryImporter importer(dic);

    std::vector<Thread> threads;
    for (int i = 0; i < kThreadsSize; ++i) {
      threads.emplace_back([&, i]() {
        importer.Import(absl::StrCat("dic", i), make_tsv_dic(i));
      });
    }

    for (auto& thread : threads) {
      thread.Join();
    }

    // The importing process is canceled in destructor so we need to call
    // Wait() explicitly.
    importer.Wait();
  }

  EXPECT_OK(FileUtil::FileExists(filename));

  auto check_entries = [](const UserDictionary& dic) {
    const ConversionRequest convreq = ConversionRequestBuilder().Build();
    for (int i = 0; i < kThreadsSize; ++i) {
      for (int n = 0; n < kEntrySize; ++n) {
        const std::string key = absl::StrFormat("key_%2.2d%4.4d", i, n);
        MockCallback mock_callback;
        EXPECT_CALL(mock_callback, OnKey(Eq(key)))
            .Times(1)
            .WillRepeatedly(
                Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
        EXPECT_CALL(mock_callback, OnActualKey(_, _, _))
            .Times(1)
            .WillRepeatedly(
                Return(DictionaryInterface::Callback::TRAVERSE_CONTINUE));
        dic.LookupExact(key, convreq, &mock_callback);
      }
    }
  };

  check_entries(dic);

  auto dic_reloaded = make_dic();

  // reload data.
  {
    dic_reloaded.Reload();
    dic_reloaded.WaitForReloader();
    check_entries(dic_reloaded);
  }

  // Clear dictionary with empty TSV.
  {
    user_dictionary::AsyncUserDictionaryImporter importer(dic_reloaded);

    importer.Import("dic0", "");
    importer.Wait();

    mozc::UserDictionaryStorage storage(filename);
    EXPECT_OK(storage.Load());
    EXPECT_FALSE(storage.GetUserDictionaryId("dic0").ok());
    EXPECT_TRUE(storage.GetUserDictionaryId("dic1").ok());
  }
}

}  // namespace
}  // namespace dictionary
}  // namespace mozc

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

#include "dictionary/user_dictionary_session.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "dictionary/user_dictionary_storage.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "testing/testing_util.h"

namespace mozc::user_dictionary {
namespace {

constexpr char kDictionaryData[] =
    "きょうと\t京都\t名詞\n"
    "おおさか\t大阪\t地名\n"
    "とうきょう\t東京\t地名\tコメント\n"
    "すずき\t鈴木\t人名\n";

class UserDictionarySessionTest : public testing::TestWithTempUserProfile {
 protected:
  std::string GetUserDictionaryFile() {
    return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), "test.db");
  }

  void ResetEntry(const absl::string_view key, const absl::string_view value,
                  const UserDictionary::PosType pos,
                  UserDictionary::Entry* entry) {
    entry->Clear();
    entry->set_key(std::string(key));
    entry->set_value(std::string(value));
    entry->set_pos(pos);
  }
};

TEST_F(UserDictionarySessionTest, SaveAndLoad) {
  UserDictionarySession session(GetUserDictionaryFile());

  ASSERT_EQ(session.Load(), UserDictionaryCommandStatus::FILE_NOT_FOUND);

  session.mutable_storage()->GetProto().set_version(10);
  ASSERT_EQ(session.Save(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  // Clear once, in order to make sure that Load is actually working.
  session.mutable_storage()->GetProto().Clear();
  ASSERT_EQ(session.Load(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  // Make sure that the data is actually loaded.
  EXPECT_EQ(session.storage().version(), 10);
}

TEST_F(UserDictionarySessionTest, LoadWithEnsuringNonEmptyStorage) {
  UserDictionarySession session(GetUserDictionaryFile());
  session.SetDefaultDictionaryName("abcde");

  ASSERT_EQ(session.LoadWithEnsuringNonEmptyStorage(),
            UserDictionaryCommandStatus::FILE_NOT_FOUND);

  EXPECT_PROTO_PEQ("dictionaries: < name: \"abcde\" >", session.storage());
}

// Unfortunately the limit size of the stored file is hard coded in
// user_dictionary_storage.cc, so it is not reallistic to test it on various
// environment for now, as it requires (unreasonablly) huge diskspace.
// TODO(hidehiko): enable the following test, after moving the save logic to
// UserDictionarySession.
TEST_F(UserDictionarySessionTest, DISABLED_HugeFileSave) {
  UserDictionarySession session(GetUserDictionaryFile());

  // Create huge dummy data.
  {
    UserDictionaryStorage* storage = &session.mutable_storage()->GetProto();
    for (int i = 0; i < 100; ++i) {
      UserDictionary* dictionary = storage->add_dictionaries();
      for (int j = 0; j < 1000; ++j) {
        UserDictionary::Entry* entry = dictionary->add_entries();
        entry->set_key("dummy_key_data");
        entry->set_value("dummy_value_data");
        entry->set_pos(UserDictionary::NOUN);  // Set dummy data.
        entry->set_comment(
            "dummy_long_long_long_long_long_long_long_long_long_comment");
      }
    }
  }

  ASSERT_EQ(session.Save(),
            UserDictionaryCommandStatus::FILE_SIZE_LIMIT_EXCEEDED);

  session.mutable_storage()->GetProto().Clear();
  ASSERT_EQ(session.Load(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  EXPECT_GT(session.storage().dictionaries_size(), 0);
}

TEST_F(UserDictionarySessionTest, UndoWithoutHistory) {
  UserDictionarySession session(GetUserDictionaryFile());
  EXPECT_EQ(session.Undo(), UserDictionaryCommandStatus::NO_UNDO_HISTORY);
}

TEST_F(UserDictionarySessionTest, CreateDictionary) {
  UserDictionarySession session(GetUserDictionaryFile());

  uint64_t dictionary_id;
  ASSERT_EQ(session.CreateDictionary("user dictionary", &dictionary_id),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  {
    const UserDictionaryStorage& storage = session.storage();
    EXPECT_EQ(storage.dictionaries_size(), 1);
    EXPECT_EQ(storage.dictionaries(0).name(), "user dictionary");
    EXPECT_EQ(storage.dictionaries(0).id(), dictionary_id);
  }

  uint64_t dummy_dictionary_id;
  EXPECT_EQ(session.CreateDictionary("", &dummy_dictionary_id),
            UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY);
  EXPECT_EQ(
      session.CreateDictionary(std::string(500, 'a'), &dummy_dictionary_id),
      UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG);
  EXPECT_EQ(
      session.CreateDictionary("a\nb", &dummy_dictionary_id),
      UserDictionaryCommandStatus ::DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER);
  EXPECT_EQ(session.CreateDictionary("user dictionary", &dummy_dictionary_id),
            UserDictionaryCommandStatus::DICTIONARY_NAME_DUPLICATED);

  // Test undo for CreateDictionary.
  EXPECT_EQ(session.Undo(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  EXPECT_EQ(session.storage().dictionaries_size(), 0);

  while (session.storage().dictionaries_size() <
         ::mozc::UserDictionaryStorage::max_dictionary_size()) {
    session.mutable_storage()->GetProto().add_dictionaries();
  }
  EXPECT_EQ(session.CreateDictionary("dictionary 2", &dummy_dictionary_id),
            UserDictionaryCommandStatus::DICTIONARY_SIZE_LIMIT_EXCEEDED);
}

TEST_F(UserDictionarySessionTest, DeleteDictionary) {
  UserDictionarySession session(GetUserDictionaryFile());

  // Add dummy dictionary.
  constexpr uint64_t kDummyId = 10;
  {
    UserDictionary* user_dictionary =
        session.mutable_storage()->GetProto().add_dictionaries();
    user_dictionary->set_id(kDummyId);
  }

  EXPECT_EQ(session.DeleteDictionary(kDummyId),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  EXPECT_EQ(session.DeleteDictionary(100000),
            UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID);

  // Test undo for DeleteDictionary.
  EXPECT_EQ(session.Undo(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  ASSERT_EQ(session.storage().dictionaries_size(), 1);
  EXPECT_EQ(session.storage().dictionaries(0).id(), kDummyId);
}

TEST_F(UserDictionarySessionTest, DeleteDictionaryWithEnsuringNonEmptyStorage) {
  UserDictionarySession session(GetUserDictionaryFile());
  session.SetDefaultDictionaryName("abcde");

  // Add dummy dictionary.
  constexpr uint64_t kDummyId = 10;
  {
    UserDictionary* user_dictionary =
        session.mutable_storage()->GetProto().add_dictionaries();
    user_dictionary->set_id(kDummyId);
  }

  EXPECT_EQ(session.DeleteDictionaryWithEnsuringNonEmptyStorage(kDummyId),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  EXPECT_PROTO_PEQ(
      "dictionaries: <\n"
      "  name: \"abcde\"\n"
      ">\n",
      session.storage());

  // Test undo for DeleteDictionaryWithEnsuringNonEmptyStorage.
  EXPECT_EQ(session.Undo(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  ASSERT_EQ(session.storage().dictionaries_size(), 1);
  EXPECT_EQ(session.storage().dictionaries(0).id(), kDummyId);
}

TEST_F(UserDictionarySessionTest, RenameDictionary) {
  UserDictionarySession session(GetUserDictionaryFile());

  // Prepare the targeet dictionary.
  uint64_t dictionary_id;
  ASSERT_EQ(session.CreateDictionary("user dictionary", &dictionary_id),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  ASSERT_EQ(session.RenameDictionary(dictionary_id, "new name"),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  EXPECT_EQ(session.storage().dictionaries(0).name(), "new name");

  EXPECT_EQ(session.RenameDictionary(dictionary_id, ""),
            UserDictionaryCommandStatus::DICTIONARY_NAME_EMPTY);
  EXPECT_EQ(session.RenameDictionary(dictionary_id, std::string(500, 'a')),
            UserDictionaryCommandStatus::DICTIONARY_NAME_TOO_LONG);
  EXPECT_EQ(
      session.RenameDictionary(dictionary_id, "a\nb"),
      UserDictionaryCommandStatus ::DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER);

  // OK to rename to the same name.
  EXPECT_EQ(session.RenameDictionary(dictionary_id, "new name"),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  uint64_t dummy_dictionary_id;
  ASSERT_EQ(session.CreateDictionary("another name", &dummy_dictionary_id),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  // NG to rename to the name of another dictionary.
  EXPECT_EQ(session.RenameDictionary(dictionary_id, "another name"),
            UserDictionaryCommandStatus::DICTIONARY_NAME_DUPLICATED);

  EXPECT_EQ(session.RenameDictionary(10000000, "new name 2"),
            UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID);

  // Test undo for RenameDictionary.
  // Before the test, undo for create dictionary.
  EXPECT_EQ(session.Undo(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  ASSERT_EQ(1, session.storage().dictionaries_size());
  EXPECT_EQ(session.Undo(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  EXPECT_EQ(session.storage().dictionaries(0).name(), "new name");
  EXPECT_EQ(session.Undo(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  EXPECT_EQ(session.storage().dictionaries(0).name(), "user dictionary");
}

TEST_F(UserDictionarySessionTest, AddEntry) {
  UserDictionarySession session(GetUserDictionaryFile());
  UserDictionary::Entry entry;

  uint64_t dictionary_id;
  ASSERT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            session.CreateDictionary("user dictionary", &dictionary_id));

  ResetEntry("reading", "word", UserDictionary::NOUN, &entry);
  ASSERT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            session.AddEntry(dictionary_id, entry));
  EXPECT_PROTO_PEQ(
      "dictionaries: <\n"
      "  entries: <\n"
      "    key: \"reading\"\n"
      "    value: \"word\"\n"
      "    pos: NOUN\n"
      "  >\n"
      ">\n",
      session.storage());

  ResetEntry("reading2", "word2", UserDictionary::PREFIX, &entry);
  ASSERT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            session.AddEntry(dictionary_id, entry));
  EXPECT_PROTO_PEQ(
      "dictionaries: <\n"
      "  entries: <\n"
      "    key: \"reading\"\n"
      "    value: \"word\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading2\"\n"
      "    value: \"word2\"\n"
      "    pos: PREFIX\n"
      "  >\n"
      ">\n",
      session.storage());

  ResetEntry("", "word3", UserDictionary::NOUN, &entry);
  EXPECT_EQ(session.AddEntry(dictionary_id, entry),
            UserDictionaryCommandStatus::READING_EMPTY);

  // 0 is always invalid dictionary id.
  ResetEntry("reading4", "word4", UserDictionary::NOUN, &entry);
  EXPECT_EQ(session.AddEntry(0, entry),
            UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID);

  // Test undo for AddEntry.
  EXPECT_EQ(session.Undo(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  EXPECT_PROTO_PEQ(
      "dictionaries: <\n"
      "  entries: <\n"
      "    key: \"reading\"\n"
      "    value: \"word\"\n"
      "    pos: NOUN\n"
      "  >\n"
      ">\n",
      session.storage());
}

TEST_F(UserDictionarySessionTest, AddEntryLimitExceeded) {
  UserDictionarySession session(GetUserDictionaryFile());
  UserDictionary::Entry entry;

  uint64_t dictionary_id;
  ASSERT_EQ(session.CreateDictionary("user dictionary", &dictionary_id),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  ResetEntry("reading", "word", UserDictionary::NOUN, &entry);

  for (int i = 0; i < mozc::UserDictionaryStorage::max_entry_size(); ++i) {
    ASSERT_EQ(session.AddEntry(dictionary_id, entry),
              UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  }

  EXPECT_EQ(session.AddEntry(dictionary_id, entry),
            UserDictionaryCommandStatus::ENTRY_SIZE_LIMIT_EXCEEDED);
}

TEST_F(UserDictionarySessionTest, EditEntry) {
  UserDictionarySession session(GetUserDictionaryFile());
  UserDictionary::Entry entry;

  uint64_t dictionary_id;
  ASSERT_EQ(session.CreateDictionary("user dictionary", &dictionary_id),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  ResetEntry("reading", "word", UserDictionary::NOUN, &entry);
  ASSERT_EQ(session.AddEntry(dictionary_id, entry),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  ResetEntry("reading2", "word2", UserDictionary::PREFIX, &entry);
  ASSERT_EQ(session.AddEntry(dictionary_id, entry),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  EXPECT_PROTO_PEQ(
      "dictionaries: <\n"
      "  entries: <\n"
      "    key: \"reading\"\n"
      "    value: \"word\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading2\"\n"
      "    value: \"word2\"\n"
      "    pos: PREFIX\n"
      "  >\n"
      ">\n",
      session.storage());

  ResetEntry("reading3", "word3", UserDictionary::ADVERB, &entry);
  ASSERT_EQ(session.EditEntry(dictionary_id, 0, entry),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  EXPECT_PROTO_PEQ(
      "dictionaries: <\n"
      "  entries: <\n"
      "    key: \"reading3\"\n"
      "    value: \"word3\"\n"
      "    pos: ADVERB\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading2\"\n"
      "    value: \"word2\"\n"
      "    pos: PREFIX\n"
      "  >\n"
      ">\n",
      session.storage());

  // Test for index out of bounds.
  ResetEntry("reading4", "word4", UserDictionary::NOUN, &entry);
  EXPECT_EQ(session.EditEntry(dictionary_id, -1, entry),
            UserDictionaryCommandStatus::ENTRY_INDEX_OUT_OF_RANGE);

  // Test for invalid entry.
  ResetEntry("", "word4", UserDictionary::NOUN, &entry);
  EXPECT_EQ(session.EditEntry(dictionary_id, 0, entry),
            UserDictionaryCommandStatus::READING_EMPTY);

  // Test for invalid dictionary id. 0 is always invalid dictionary id.
  ResetEntry("reading4", "word4", UserDictionary::NOUN, &entry);
  EXPECT_EQ(session.EditEntry(0, 0, entry),
            UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID);

  // Test undo for EditEntry.
  EXPECT_EQ(session.Undo(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  EXPECT_PROTO_PEQ(
      "dictionaries: <\n"
      "  entries: <\n"
      "    key: \"reading\"\n"
      "    value: \"word\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading2\"\n"
      "    value: \"word2\"\n"
      "    pos: PREFIX\n"
      "  >\n"
      ">\n",
      session.storage());
}

TEST_F(UserDictionarySessionTest, DeleteEntry) {
  UserDictionarySession session(GetUserDictionaryFile());
  UserDictionary::Entry entry;

  uint64_t dictionary_id;
  ASSERT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            session.CreateDictionary("user dictionary", &dictionary_id));

  ResetEntry("reading", "word", UserDictionary::NOUN, &entry);
  ASSERT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            session.AddEntry(dictionary_id, entry));
  ResetEntry("reading2", "word2", UserDictionary::NOUN, &entry);
  ASSERT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            session.AddEntry(dictionary_id, entry));
  ResetEntry("reading3", "word3", UserDictionary::NOUN, &entry);
  ASSERT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            session.AddEntry(dictionary_id, entry));
  ResetEntry("reading4", "word4", UserDictionary::NOUN, &entry);
  ASSERT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            session.AddEntry(dictionary_id, entry));
  ResetEntry("reading5", "word5", UserDictionary::NOUN, &entry);
  ASSERT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            session.AddEntry(dictionary_id, entry));

  EXPECT_PROTO_PEQ(
      "dictionaries: <\n"
      "  entries: <\n"
      "    key: \"reading\"\n"
      "    value: \"word\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading2\"\n"
      "    value: \"word2\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading3\"\n"
      "    value: \"word3\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading4\"\n"
      "    value: \"word4\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading5\"\n"
      "    value: \"word5\"\n"
      "    pos: NOUN\n"
      "  >\n"
      ">\n",
      session.storage());

  std::vector<int> index_list;
  index_list.push_back(1);
  index_list.push_back(3);
  EXPECT_EQ(session.DeleteEntry(dictionary_id, std::move(index_list)),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  EXPECT_PROTO_PEQ(
      "dictionaries: <\n"
      "  entries: <\n"
      "    key: \"reading\"\n"
      "    value: \"word\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading3\"\n"
      "    value: \"word3\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading5\"\n"
      "    value: \"word5\"\n"
      "    pos: NOUN\n"
      "  >\n"
      ">\n",
      session.storage());

  // Test for index out of bounds.
  index_list.clear();
  index_list.push_back(0);
  index_list.push_back(100);
  EXPECT_EQ(session.DeleteEntry(dictionary_id, std::move(index_list)),
            UserDictionaryCommandStatus::ENTRY_INDEX_OUT_OF_RANGE);

  // The contents shouldn't be changed.
  EXPECT_PROTO_PEQ(
      "dictionaries: <\n"
      "  entries: <\n"
      "    key: \"reading\"\n"
      "    value: \"word\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading3\"\n"
      "    value: \"word3\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading5\"\n"
      "    value: \"word5\"\n"
      "    pos: NOUN\n"
      "  >\n"
      ">\n",
      session.storage());

  // Test for invalid dictionary id.
  index_list.clear();
  index_list.push_back(0);
  EXPECT_EQ(session.DeleteEntry(0, std::move(index_list)),
            UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID);

  // Test undo for delete entries.
  EXPECT_EQ(session.Undo(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  EXPECT_PROTO_PEQ(
      "dictionaries: <\n"
      "  entries: <\n"
      "    key: \"reading\"\n"
      "    value: \"word\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading2\"\n"
      "    value: \"word2\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading3\"\n"
      "    value: \"word3\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading4\"\n"
      "    value: \"word4\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"reading5\"\n"
      "    value: \"word5\"\n"
      "    pos: NOUN\n"
      "  >\n"
      ">\n",
      session.storage());
}

TEST_F(UserDictionarySessionTest, ImportFromString) {
  UserDictionarySession session(GetUserDictionaryFile());

  uint64_t dictionary_id;
  ASSERT_EQ(session.CreateDictionary("user dictionary", &dictionary_id),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  ASSERT_EQ(session.ImportFromString(dictionary_id, kDictionaryData),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  EXPECT_PROTO_PEQ(
      "dictionaries: <\n"
      "  entries: <\n"
      "    key: \"きょうと\"\n"
      "    value: \"京都\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"おおさか\"\n"
      "    value: \"大阪\"\n"
      "    pos: PLACE_NAME\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"とうきょう\"\n"
      "    value: \"東京\"\n"
      "    pos: PLACE_NAME\n"
      "    comment: \"コメント\"\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"すずき\"\n"
      "    value: \"鈴木\"\n"
      "    pos: PERSONAL_NAME\n"
      "  >\n"
      ">",
      session.storage());

  ASSERT_EQ(session.ImportFromString(0, kDictionaryData),
            UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID);

  // Test for Undo.
  ASSERT_EQ(session.Undo(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  // The dictionary instance should be kept, but imported contents should be
  // cleared.
  ASSERT_EQ(session.storage().dictionaries_size(), 1);
  EXPECT_EQ(session.storage().dictionaries(0).entries_size(), 0);
}

TEST_F(UserDictionarySessionTest, ImportToNewDictionaryFromString) {
  UserDictionarySession session(GetUserDictionaryFile());

  uint64_t dictionary_id;
  ASSERT_EQ(session.ImportToNewDictionaryFromString(
                "user dictionary", kDictionaryData, &dictionary_id),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  EXPECT_PROTO_PEQ(
      "dictionaries: <\n"
      "  name: \"user dictionary\"\n"
      "  entries: <\n"
      "    key: \"きょうと\"\n"
      "    value: \"京都\"\n"
      "    pos: NOUN\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"おおさか\"\n"
      "    value: \"大阪\"\n"
      "    pos: PLACE_NAME\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"とうきょう\"\n"
      "    value: \"東京\"\n"
      "    pos: PLACE_NAME\n"
      "    comment: \"コメント\"\n"
      "  >\n"
      "  entries: <\n"
      "    key: \"すずき\"\n"
      "    value: \"鈴木\"\n"
      "    pos: PERSONAL_NAME\n"
      "  >\n"
      ">",
      session.storage());

  // Test for UNDO.
  ASSERT_EQ(session.Undo(),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);
  // The dictionary instance should be removed.
  EXPECT_EQ(session.storage().dictionaries_size(), 0);
}

TEST_F(UserDictionarySessionTest, ImportToNewDictionaryFromStringFailure) {
  UserDictionarySession session(GetUserDictionaryFile());

  // Try to create a new dictionary with a name containing
  // an invalid character.
  uint64_t dictionary_id;
  ASSERT_EQ(

      session.ImportToNewDictionaryFromString("a\nb", kDictionaryData,
                                              &dictionary_id),
      UserDictionaryCommandStatus ::DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER);

  EXPECT_EQ(session.storage().dictionaries_size(), 0);
}

TEST_F(UserDictionarySessionTest, ClearDictionariesAndUndoHistory) {
  UserDictionarySession session(GetUserDictionaryFile());

  uint64_t dic_id;
  ASSERT_EQ(session.CreateDictionary("dic1", &dic_id),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  UserDictionary::Entry entry;
  ResetEntry("reading", "word", UserDictionary::NOUN, &entry);
  ASSERT_EQ(session.AddEntry(dic_id, entry),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  ResetEntry("reading", "word2", UserDictionary::NOUN, &entry);
  ASSERT_EQ(session.AddEntry(dic_id, entry),
            UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS);

  ASSERT_EQ(session.storage().dictionaries_size(), 1);
  ASSERT_TRUE(session.has_undo_history());

  session.ClearDictionariesAndUndoHistory();

  EXPECT_EQ(session.storage().dictionaries_size(), 0);
  EXPECT_FALSE(session.has_undo_history());
}

}  // namespace
}  // namespace mozc::user_dictionary

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

#include "dictionary/user_dictionary_session_handler.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/file_util.h"
#include "base/protobuf/repeated_ptr_field.h"
#include "base/system_util.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "testing/testing_util.h"

namespace mozc {
namespace {

using ::mozc::protobuf::RepeatedPtrField;
using ::mozc::user_dictionary::UserDictionary;
using ::mozc::user_dictionary::UserDictionaryCommand;
using ::mozc::user_dictionary::UserDictionaryCommandStatus;
using ::mozc::user_dictionary::UserDictionarySessionHandler;

constexpr char kDictionaryData[] =
    "きょうと\t京都\t名詞\n"
    "おおさか\t大阪\t地名\n"
    "とうきょう\t東京\t地名\tコメント\n"
    "すずき\t鈴木\t人名\n";

// 0 means invalid dictionary id.
// c.f., UserDictionaryUtil::CreateNewDictionaryId()
constexpr uint64_t kInvalidDictionaryId = 0;

class UserDictionarySessionHandlerTest
    : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    handler_ = std::make_unique<UserDictionarySessionHandler>();
    command_ = std::make_unique<UserDictionaryCommand>();
    status_ = std::make_unique<UserDictionaryCommandStatus>();

    handler_->set_dictionary_path(GetUserDictionaryFile());
  }

  void TearDown() override {
    EXPECT_OK(FileUtil::UnlinkIfExists(GetUserDictionaryFile()));
  }

  void Clear() {
    command_->Clear();
    status_->Clear();
  }

  static std::string GetUserDictionaryFile() {
    return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), "test.db");
  }

  uint64_t CreateSession() {
    Clear();
    command_->set_type(UserDictionaryCommand::CREATE_SESSION);
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(status_->status(),
              (UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS));
    EXPECT_TRUE(status_->has_session_id());
    EXPECT_NE(0, status_->session_id());
    return status_->session_id();
  }

  void DeleteSession(uint64_t session_id) {
    Clear();
    command_->set_type(UserDictionaryCommand::DELETE_SESSION);
    command_->set_session_id(session_id);
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(status_->status(),
              (UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS));
  }

  uint64_t CreateUserDictionary(const uint64_t session_id,
                                const absl::string_view name) {
    Clear();
    command_->set_type(UserDictionaryCommand::CREATE_DICTIONARY);
    command_->set_session_id(session_id);
    command_->set_dictionary_name(name);
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(status_->status(),
              (UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS));
    EXPECT_TRUE(status_->has_dictionary_id());
    return status_->dictionary_id();
  }

  void AddUserDictionaryEntry(const uint64_t session_id,
                              const uint64_t dictionary_id,
                              const absl::string_view key,
                              const absl::string_view value,
                              const UserDictionary::PosType pos,
                              const absl::string_view comment) {
    Clear();
    command_->set_type(UserDictionaryCommand::ADD_ENTRY);
    command_->set_session_id(session_id);
    command_->set_dictionary_id(dictionary_id);
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key(std::string(key));
    entry->set_value(std::string(value));
    entry->set_pos(pos);
    entry->set_comment(std::string(comment));
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(status_->status(),
              (UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS));
  }

  RepeatedPtrField<UserDictionary::Entry> GetAllUserDictionaryEntries(
      uint64_t session_id, uint64_t dictionary_id) {
    std::vector<int> indices;
    const uint32_t entries_size =
        GetUserDictionaryEntrySize(session_id, dictionary_id);
    for (uint32_t i = 0; i < entries_size; ++i) {
      indices.push_back(i);
    }
    return GetUserDictionaryEntries(session_id, dictionary_id, indices);
  }

  RepeatedPtrField<UserDictionary::Entry> GetUserDictionaryEntries(
      uint64_t session_id, uint64_t dictionary_id,
      absl::Span<const int> indices) {
    Clear();
    command_->set_type(UserDictionaryCommand::GET_ENTRIES);
    command_->set_session_id(session_id);
    command_->set_dictionary_id(dictionary_id);
    for (size_t i = 0; i < indices.size(); ++i) {
      command_->add_entry_index(indices[i]);
    }
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(status_->status(),
              (UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS));
    EXPECT_EQ(status_->entries_size(), indices.size());
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(status_->status(),
              (UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS));
    return status_->entries();
  }

  uint32_t GetUserDictionaryEntrySize(uint64_t session_id,
                                      uint64_t dictionary_id) {
    Clear();
    command_->set_type(UserDictionaryCommand::GET_ENTRY_SIZE);
    command_->set_session_id(session_id);
    command_->set_dictionary_id(dictionary_id);
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(status_->status(),
              (UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS));
    EXPECT_TRUE(status_->has_entry_size());
    return status_->entry_size();
  }

  std::unique_ptr<UserDictionarySessionHandler> handler_;
  std::unique_ptr<UserDictionaryCommand> command_;
  std::unique_ptr<UserDictionaryCommandStatus> status_;
};

TEST_F(UserDictionarySessionHandlerTest, InvalidCommand) {
  ASSERT_FALSE(handler_->Evaluate(*command_, status_.get()));

  // We cannot test setting invalid id, because it'll just fail to cast
  // (i.e. assertion error) in debug build.
}

TEST_F(UserDictionarySessionHandlerTest, NoOperation) {
  const uint64_t session_id = CreateSession();

  Clear();
  command_->set_type(UserDictionaryCommand::NO_OPERATION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::NO_OPERATION);
  command_->set_session_id(0);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: UNKNOWN_SESSION_ID", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::NO_OPERATION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  // Delete the session.
  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, ClearStorage) {
  // Set up a user dictionary.
  {
    Clear();
    const uint64_t session_id = CreateSession();
    const uint64_t dictionary_id =
        CreateUserDictionary(session_id, "dictionary");
    AddUserDictionaryEntry(session_id, dictionary_id, "reading", "word",
                           UserDictionary::NOUN, "");
    AddUserDictionaryEntry(session_id, dictionary_id, "reading", "word2",
                           UserDictionary::NOUN, "");
    ASSERT_EQ(GetUserDictionaryEntrySize(session_id, dictionary_id), 2);
    DeleteSession(session_id);
  }
  // Test CLEAR_STORAGE command.
  {
    Clear();
    command_->set_type(UserDictionaryCommand::CLEAR_STORAGE);
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(status_->status(),
              (UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS));
  }
  // After the command invocation, storage becomes empty.
  {
    Clear();
    const uint64_t session_id = CreateSession();
    command_->set_type(UserDictionaryCommand::GET_STORAGE);
    command_->set_session_id(session_id);
    ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_PROTO_PEQ(
        "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
        "storage <\n"
        ">\n",
        *status_);
  }
}

TEST_F(UserDictionarySessionHandlerTest, CreateDeleteSession) {
  const uint64_t session_id = CreateSession();

  // Without session_id, the command should fail.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(status_->status(), UserDictionaryCommandStatus::INVALID_ARGUMENT);

  // Test for invalid session id.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(0);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(status_->status(), UserDictionaryCommandStatus::UNKNOWN_SESSION_ID);

  // Test for valid session.
  DeleteSession(session_id);

  // Test deletion twice should fail.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(status_->status(), UserDictionaryCommandStatus::UNKNOWN_SESSION_ID);
}

TEST_F(UserDictionarySessionHandlerTest, CreateTwice) {
  const uint64_t session_id1 = CreateSession();
  const uint64_t session_id2 = CreateSession();
  ASSERT_NE(session_id1, session_id2);

  // Here, the first session is lost, so trying to delete it should fail
  // with unknown id error, and deletion of the second session should
  // success.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id1);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(status_->status(), UserDictionaryCommandStatus::UNKNOWN_SESSION_ID);

  DeleteSession(session_id2);
}

TEST_F(UserDictionarySessionHandlerTest, LoadAndSave) {
  const uint64_t session_id = CreateSession();

  // First of all, create a dictionary named "dictionary".
  CreateUserDictionary(session_id, "dictionary");

  // Save the current storage.
  Clear();
  command_->set_type(UserDictionaryCommand::SAVE);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  // Create another dictionary.
  CreateUserDictionary(session_id, "dictionary2");

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"dictionary\" >\n"
      "  dictionaries: < name: \"dictionary2\" >\n"
      ">",
      *status_);

  // Load the data to the storage. So the storage content should be reverted
  // to the saved one.
  Clear();
  command_->set_type(UserDictionaryCommand::LOAD);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"dictionary\" >\n"
      ">",
      *status_);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, LoadWithEnsuringNonEmptyStorage) {
  const uint64_t session_id = CreateSession();

  Clear();
  command_->set_type(UserDictionaryCommand::SET_DEFAULT_DICTIONARY_NAME);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("abcde");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  // Load the data to the storage. It should be failed as there should be no
  // file yet. Regardless of the failure, a new dictionary should be
  // created.
  Clear();
  command_->set_type(UserDictionaryCommand::LOAD);
  command_->set_session_id(session_id);
  command_->set_ensure_non_empty_storage(true);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: FILE_NOT_FOUND", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"abcde\" >\n"
      ">",
      *status_);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, Undo) {
  const uint64_t session_id = CreateSession();

  // At first, the session shouldn't be undoable.
  Clear();
  command_->set_type(UserDictionaryCommand::CHECK_UNDOABILITY);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: NO_UNDO_HISTORY", *status_);

  // The first undo without any preceding operation should fail.
  Clear();
  command_->set_type(UserDictionaryCommand::UNDO);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: NO_UNDO_HISTORY", *status_);

  // Create a dictionary.
  CreateUserDictionary(session_id, "dictionary");

  // Now the session should be undoable.
  Clear();
  command_->set_type(UserDictionaryCommand::CHECK_UNDOABILITY);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  // And then undo. This time, the command should succeed.
  Clear();
  command_->set_type(UserDictionaryCommand::UNDO);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, GetEntries) {
  const uint64_t session_id = CreateSession();
  const uint64_t dictionary_id = CreateUserDictionary(session_id, "dictionary");

  AddUserDictionaryEntry(session_id, dictionary_id, "key1", "value1",
                         UserDictionary::NOUN, "comment1");
  AddUserDictionaryEntry(session_id, dictionary_id, "key2", "value2",
                         UserDictionary::NOUN, "comment2");
  AddUserDictionaryEntry(session_id, dictionary_id, "key3", "value3",
                         UserDictionary::SYMBOL, "comment3");
  ASSERT_EQ(3, GetUserDictionaryEntrySize(session_id, dictionary_id));

  std::vector<int> indices;
  indices.push_back(0);
  indices.push_back(2);
  GetUserDictionaryEntries(session_id, dictionary_id, indices);
  EXPECT_PROTO_PEQ(
      "entries: <\n"
      "  key: \"key1\"\n"
      "  value: \"value1\"\n"
      "  pos: NOUN\n"
      "  comment: \"comment1\"\n"
      ">"
      "entries: <\n"
      "  key: \"key3\"\n"
      "  value: \"value3\"\n"
      "  pos: SYMBOL\n"
      "  comment: \"comment3\"\n"
      ">",
      *status_);

  // Invalid dictionary ID
  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRIES);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(kInvalidDictionaryId);
  command_->add_entry_index(0);
  EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(status_->status(),
            (UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID));

  // Invalid entry index
  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRIES);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(0);
  command_->add_entry_index(3);
  EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(status_->status(),
            (UserDictionaryCommandStatus::ENTRY_INDEX_OUT_OF_RANGE));

  // Invalid entry index
  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRIES);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(0);
  command_->add_entry_index(-1);
  EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(status_->status(),
            (UserDictionaryCommandStatus::ENTRY_INDEX_OUT_OF_RANGE));

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, DictionaryEdit) {
  const uint64_t session_id = CreateSession();

  // Create a dictionary named "dictionary".
  CreateUserDictionary(session_id, "dictionary");

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"dictionary\" >\n"
      ">",
      *status_);

  // Create another dictionary named "dictionary2".
  CreateUserDictionary(session_id, "dictionary2");

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"dictionary\" >\n"
      "  dictionaries: < name: \"dictionary2\" >\n"
      ">",
      *status_);
  const uint64_t dictionary_id1 = status_->storage().dictionaries(0).id();
  const uint64_t dictionary_id2 = status_->storage().dictionaries(1).id();

  // Dictionary creation without name should be failed.
  Clear();
  command_->set_type(UserDictionaryCommand::CREATE_DICTIONARY);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  // Then rename the second dictionary to "dictionary3".
  Clear();
  command_->set_type(UserDictionaryCommand::RENAME_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id2);
  command_->set_dictionary_name("dictionary3");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"dictionary\" >\n"
      "  dictionaries: < name: \"dictionary3\" >\n"
      ">",
      *status_);
  EXPECT_EQ(status_->storage().dictionaries(0).id(), dictionary_id1);
  EXPECT_EQ(status_->storage().dictionaries(1).id(), dictionary_id2);

  // Dictionary renaming without dictionary_id or new name should be failed.
  Clear();
  command_->set_type(UserDictionaryCommand::RENAME_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id2);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::RENAME_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("new dictionary name");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  // Then delete the first dictionary.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id1);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"dictionary3\" >\n"
      ">",
      *status_);
  EXPECT_EQ(status_->storage().dictionaries(0).id(), dictionary_id2);

  // Dictionary deletion without dictionary id should be failed.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_DICTIONARY);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  // Then delete the dictionary again with the ensure_non_empty_dictionary.
  Clear();
  command_->set_type(UserDictionaryCommand::SET_DEFAULT_DICTIONARY_NAME);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("abcde");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id2);
  command_->set_ensure_non_empty_storage(true);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"abcde\" >\n"
      ">",
      *status_);
  EXPECT_NE(status_->storage().dictionaries(0).id(), dictionary_id2);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, AddEntry) {
  const uint64_t session_id = CreateSession();
  const uint64_t dictionary_id = CreateUserDictionary(session_id, "dictionary");
  ASSERT_EQ(0, GetUserDictionaryEntrySize(session_id, dictionary_id));

  // Add an entry.
  AddUserDictionaryEntry(session_id, dictionary_id, "reading", "word",
                         UserDictionary::NOUN, "");
  ASSERT_EQ(GetUserDictionaryEntrySize(session_id, dictionary_id), 1);
  GetAllUserDictionaryEntries(session_id, dictionary_id);
  EXPECT_PROTO_PEQ(
      "entries: <\n"
      "  key: \"reading\"\n"
      "  value: \"word\"\n"
      "  pos: NOUN\n"
      ">\n",
      *status_);

  // AddEntry without dictionary_id or entry should be failed.
  Clear();
  command_->set_type(UserDictionaryCommand::ADD_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::ADD_ENTRY);
  command_->set_session_id(session_id);
  {
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key("reading");
    entry->set_value("word");
    entry->set_pos(UserDictionary::NOUN);
  }
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, EditEntry) {
  const uint64_t session_id = CreateSession();
  const uint64_t dictionary_id = CreateUserDictionary(session_id, "dictionary");
  ASSERT_EQ(GetUserDictionaryEntrySize(session_id, dictionary_id), 0);

  // Add an entry.
  AddUserDictionaryEntry(session_id, dictionary_id, "reading", "word",
                         UserDictionary::NOUN, "");
  ASSERT_EQ(GetUserDictionaryEntrySize(session_id, dictionary_id), 1);

  // Add another entry.
  AddUserDictionaryEntry(session_id, dictionary_id, "reading2", "word2",
                         UserDictionary::NOUN, "");
  ASSERT_EQ(GetUserDictionaryEntrySize(session_id, dictionary_id), 2);
  GetAllUserDictionaryEntries(session_id, dictionary_id);
  EXPECT_PROTO_PEQ(
      "entries: <\n"
      "  key: \"reading\"\n"
      "  value: \"word\"\n"
      "  pos: NOUN\n"
      ">\n"
      "entries: <\n"
      "  key: \"reading2\"\n"
      "  value: \"word2\"\n"
      "  pos: NOUN\n"
      ">",
      *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::EDIT_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(1);
  {
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key("reading3");
    entry->set_value("word3");
    entry->set_pos(UserDictionary::PREFIX);
  }
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  ASSERT_EQ(GetUserDictionaryEntrySize(session_id, dictionary_id), 2);
  GetAllUserDictionaryEntries(session_id, dictionary_id);
  EXPECT_PROTO_PEQ(
      "entries: <\n"
      "  key: \"reading\"\n"
      "  value: \"word\"\n"
      "  pos: NOUN\n"
      ">"
      "entries: <\n"
      "  key: \"reading3\"\n"
      "  value: \"word3\"\n"
      "  pos: PREFIX\n"
      ">",
      *status_);

  // EditEntry without dictionary_id or entry should be failed.
  // Also, the number of entry_index should exactly equals to '1'.
  Clear();
  command_->set_type(UserDictionaryCommand::EDIT_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(1);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::EDIT_ENTRY);
  command_->set_session_id(session_id);
  command_->add_entry_index(1);
  {
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key("reading3");
    entry->set_value("word3");
    entry->set_pos(UserDictionary::PREFIX);
  }
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::EDIT_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  {
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key("reading3");
    entry->set_value("word3");
    entry->set_pos(UserDictionary::PREFIX);
  }
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::EDIT_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(0);
  command_->add_entry_index(1);
  {
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key("reading3");
    entry->set_value("word3");
    entry->set_pos(UserDictionary::PREFIX);
  }
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, DeleteEntry) {
  const uint64_t session_id = CreateSession();
  const uint64_t dictionary_id = CreateUserDictionary(session_id, "dictionary");
  ASSERT_EQ(GetUserDictionaryEntrySize(session_id, dictionary_id), 0);

  // Add entries.
  AddUserDictionaryEntry(session_id, dictionary_id, "reading", "word",
                         UserDictionary::NOUN, "");
  AddUserDictionaryEntry(session_id, dictionary_id, "reading2", "word2",
                         UserDictionary::NOUN, "");
  AddUserDictionaryEntry(session_id, dictionary_id, "reading3", "word3",
                         UserDictionary::NOUN, "");
  AddUserDictionaryEntry(session_id, dictionary_id, "reading4", "word4",
                         UserDictionary::NOUN, "");
  AddUserDictionaryEntry(session_id, dictionary_id, "reading5", "word5",
                         UserDictionary::NOUN, "");
  ASSERT_EQ(GetUserDictionaryEntrySize(session_id, dictionary_id), 5);

  // Delete the second and fourth entries.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(1);
  command_->add_entry_index(3);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(status_->status(),
            (UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS));
  ASSERT_EQ(3, GetUserDictionaryEntrySize(session_id, dictionary_id));
  GetAllUserDictionaryEntries(session_id, dictionary_id);
  EXPECT_PROTO_PEQ(
      "entries: <\n"
      "  key: \"reading\"\n"
      "  value: \"word\"\n"
      "  pos: NOUN\n"
      ">"
      "entries: <\n"
      "  key: \"reading3\"\n"
      "  value: \"word3\"\n"
      "  pos: NOUN\n"
      ">"
      "entries: <\n"
      "  key: \"reading5\"\n"
      "  value: \"word5\"\n"
      "  pos: NOUN\n"
      ">",
      *status_);

  // Entry deletion without dictionary_id or entry_index should be failed.
  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRIES);
  command_->set_session_id(session_id);
  command_->add_entry_index(0);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);
  ASSERT_EQ(GetUserDictionaryEntrySize(session_id, dictionary_id), 3);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRIES);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);
  ASSERT_EQ(GetUserDictionaryEntrySize(session_id, dictionary_id), 3);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, ImportData1) {
  const uint64_t session_id = CreateSession();

  // First of all, create a dictionary named "dictionary".
  const uint64_t dictionary_id = CreateUserDictionary(session_id, "dictionary");

  // Import data to the dictionary.
  Clear();
  command_->set_type(UserDictionaryCommand::IMPORT_DATA);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->set_data(kDictionaryData);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);
  ASSERT_TRUE(status_->has_dictionary_id());
  EXPECT_EQ(status_->dictionary_id(), dictionary_id);

  // Make sure the size of the data.
  ASSERT_EQ(GetUserDictionaryEntrySize(session_id, dictionary_id), 4);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, ImportData2) {
  const uint64_t session_id = CreateSession();

  // Import data to a new dictionary.
  Clear();
  command_->set_type(UserDictionaryCommand::IMPORT_DATA);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("user dictionary");
  command_->set_data(kDictionaryData);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);
  ASSERT_TRUE(status_->has_dictionary_id());
  const uint64_t dictionary_id = status_->dictionary_id();

  // Make sure the size of the data.
  ASSERT_EQ(GetUserDictionaryEntrySize(session_id, dictionary_id), 4);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, ImportDataFailure) {
  const uint64_t session_id = CreateSession();
  const uint64_t dictionary_id = CreateUserDictionary(session_id, "dictionary");

  // Fail if the data is missing.
  Clear();
  command_->set_type(UserDictionaryCommand::IMPORT_DATA);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("user dictionary");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::IMPORT_DATA);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  // Fail if neither dictionary_name nor dictionary_id is set.
  Clear();
  command_->set_type(UserDictionaryCommand::IMPORT_DATA);
  command_->set_session_id(session_id);
  command_->set_data(kDictionaryData);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, ImportDataIgnoringInvalidEntries) {
  const uint64_t session_id = CreateSession();

  // kDictionaryData contains 4 entries.
  std::string data = kDictionaryData;
  // Adding 3 entries, but the last one is invalid. So the total should be 6.
  data.append("☻\tEMOTICON\t名詞\n");            // Symbol reading (valid).
  data.append("読み\tYOMI\t名詞\n");             // Kanji reading (valid).
  data.append("あ\x81\x84う\tINVALID\t名詞\n");  // Invalid UTF-8 string.

  // Import data to a new dictionary.
  Clear();
  command_->set_type(UserDictionaryCommand::IMPORT_DATA);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("user dictionary");
  command_->set_data(data);
  command_->set_ignore_invalid_entries(true);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);
  ASSERT_TRUE(status_->has_dictionary_id());
  const uint64_t dictionary_id = status_->dictionary_id();

  // Make sure the size of the data.
  ASSERT_EQ(GetUserDictionaryEntrySize(session_id, dictionary_id), 6);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, GetStorage) {
  const uint64_t session_id = CreateSession();
  const uint64_t dictionary_id1 =
      CreateUserDictionary(session_id, "dictionary1");

  AddUserDictionaryEntry(session_id, dictionary_id1, "reading1_1", "word1_1",
                         UserDictionary::NOUN, "");
  AddUserDictionaryEntry(session_id, dictionary_id1, "reading1_2", "word1_2",
                         UserDictionary::NOUN, "");

  // Create a dictionary named "dictionary2".
  const uint64_t dictionary_id2 =
      CreateUserDictionary(session_id, "dictionary2");

  AddUserDictionaryEntry(session_id, dictionary_id2, "reading2_1", "word2_1",
                         UserDictionary::NOUN, "");

  Clear();
  command_->set_type(UserDictionaryCommand::GET_STORAGE);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage <\n"
      "  dictionaries <\n"
      "    name: \"dictionary1\"\n"
      "    entries <\n"
      "      key: \"reading1_1\"\n"
      "      value: \"word1_1\"\n"
      "      comment: \"\"\n"
      "      pos: NOUN\n"
      "    >\n"
      "    entries <\n"
      "      key: \"reading1_2\"\n"
      "      value: \"word1_2\"\n"
      "      comment: \"\"\n"
      "      pos: NOUN\n"
      "    >\n"
      "  >\n"
      "  dictionaries <\n"
      "    name: \"dictionary2\"\n"
      "    entries <\n"
      "      key: \"reading2_1\"\n"
      "      value: \"word2_1\"\n"
      "      comment: \"\"\n"
      "      pos: NOUN\n"
      "    >\n"
      "  >\n"
      ">\n",
      *status_);

  DeleteSession(session_id);
}

}  // namespace
}  // namespace mozc

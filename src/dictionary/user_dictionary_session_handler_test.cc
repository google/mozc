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

#include "dictionary/user_dictionary_session_handler.h"

#ifndef OS_WINDOWS
#include <sys/stat.h>
#endif

#include <string>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/testing_util.h"
#include "base/util.h"
#include "dictionary/user_dictionary_storage.pb.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

using ::mozc::user_dictionary::UserDictionary;
using ::mozc::user_dictionary::UserDictionaryCommand;
using ::mozc::user_dictionary::UserDictionaryCommandStatus;
using ::mozc::user_dictionary::UserDictionarySessionHandler;

namespace mozc {

namespace {
// "きょうと\t京都\t名詞\n"
// "!おおさか\t大阪\t地名\n"
// "\n"
// "#とうきょう\t東京\t地名\tコメント\n"
// "すずき\t鈴木\t人名\n";
const char kDictionaryData[] =
    "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8\t"
    "\xE4\xBA\xAC\xE9\x83\xBD\t\xE5\x90\x8D\xE8\xA9\x9E\n"
    "\xE3\x81\x8A\xE3\x81\x8A\xE3\x81\x95\xE3\x81\x8B\t"
    "\xE5\xA4\xA7\xE9\x98\xAA\t\xE5\x9C\xB0\xE5\x90\x8D\n"
    "\xE3\x81\xA8\xE3\x81\x86\xE3\x81\x8D\xE3\x82\x87\xE3"
    "\x81\x86\t\xE6\x9D\xB1\xE4\xBA\xAC\t\xE5\x9C\xB0\xE5"
    "\x90\x8D\t\xE3\x82\xB3\xE3\x83\xA1\xE3\x83\xB3\xE3\x83\x88\n"
    "\xE3\x81\x99\xE3\x81\x9A\xE3\x81\x8D\t\xE9\x88\xB4"
    "\xE6\x9C\xA8\t\xE4\xBA\xBA\xE5\x90\x8D\n";
}  // namespace

class UserDictionarySessionHandlerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    original_user_profile_directory_ = Util::GetUserProfileDirectory();
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    Util::Unlink(GetUserDictionaryFile());

    handler_.reset(new UserDictionarySessionHandler);
    command_.reset(new UserDictionaryCommand);
    status_.reset(new UserDictionaryCommandStatus);

    handler_->set_dictionary_path(GetUserDictionaryFile());
  }

  virtual void TearDown() {
    Util::Unlink(GetUserDictionaryFile());
    Util::SetUserProfileDirectory(original_user_profile_directory_);
  }

  void Clear() {
    command_->Clear();
    status_->Clear();
  }

  static string GetUserDictionaryFile() {
#ifndef OS_WINDOWS
    chmod(FLAGS_test_tmpdir.c_str(), 0777);
#endif
    return Util::JoinPath(FLAGS_test_tmpdir, "test.db");
  }

  scoped_ptr<UserDictionarySessionHandler> handler_;
  scoped_ptr<UserDictionaryCommand> command_;
  scoped_ptr<UserDictionaryCommandStatus> status_;

 private:
  string original_user_profile_directory_;
};

TEST_F(UserDictionarySessionHandlerTest, InvalidCommand) {
  ASSERT_FALSE(handler_->Evaluate(*command_, status_.get()));

  // We cannot test setting invalid id, because it'll just fail to cast
  // (i.e. assertion error) in debug build.
}

TEST_F(UserDictionarySessionHandlerTest, NoOperation) {
  // Create a session.
  Clear();
  command_->set_type(UserDictionaryCommand::CREATE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  const uint64 session_id = status_->session_id();

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
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
}

TEST_F(UserDictionarySessionHandlerTest, ClearStorage) {
#ifdef __native_client__
  EXPECT_PROTO_EQ("status: UNKNOWN_ERROR", *status_);
#else
  const string &user_dictionary_file = GetUserDictionaryFile();
  // Touch the file.
  {
    OutputFileStream output(user_dictionary_file.c_str());
  }
  ASSERT_TRUE(Util::FileExists(user_dictionary_file));

  command_->set_type(UserDictionaryCommand::CLEAR_STORAGE);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));

  // Should never fail.
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  // The file should be removed.
  EXPECT_FALSE(Util::FileExists(user_dictionary_file));
#endif
}

TEST_F(UserDictionarySessionHandlerTest, CreateDeleteSession) {
  command_->set_type(UserDictionaryCommand::CREATE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));

  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);
  ASSERT_TRUE(status_->has_session_id());

  uint64 session_id = status_->session_id();
  ASSERT_NE(0, session_id);

  // Without session_id, the command should fail.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  // Test for invalid session id.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(0);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: UNKNOWN_SESSION_ID", *status_);

  // Test for valid session.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  // Test deletion twice should fail.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: UNKNOWN_SESSION_ID", *status_);
}

TEST_F(UserDictionarySessionHandlerTest, CreateTwice) {
  command_->set_type(UserDictionaryCommand::CREATE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));

  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);
  ASSERT_TRUE(status_->has_session_id());

  const uint64 session_id1 = status_->session_id();
  ASSERT_NE(0, session_id1);

  // Try to create another session.
  Clear();
  command_->set_type(UserDictionaryCommand::CREATE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));

  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);
  ASSERT_TRUE(status_->has_session_id());

  const uint64 session_id2 = status_->session_id();
  ASSERT_NE(0, session_id2);
  ASSERT_NE(session_id1, session_id2);

  // Here, the first session is lost, so trying to delete it should fail
  // with unknown id error, and deletion of the second session should
  // success.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id1);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: UNKNOWN_SESSION_ID", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id2);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);
}

TEST_F(UserDictionarySessionHandlerTest, LoadAndSave) {
  // Create a session.
  command_->set_type(UserDictionaryCommand::CREATE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  const uint64 session_id = status_->session_id();

  // First of all, create a dictionary named "dictionary".
  Clear();
  command_->set_type(UserDictionaryCommand::CREATE_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("dictionary");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));

  // Save the current storage.
  Clear();
  command_->set_type(UserDictionaryCommand::SAVE);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  // Create another dictionary.
  Clear();
  command_->set_type(UserDictionaryCommand::CREATE_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("dictionary2");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
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
#ifdef ENABLE_CLOUD_SYNC
  // If CLOUD_SYNC is enabled, Load automatically creates a Sync dictionary.
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"dictionary\" >\n"
      "  dictionaries: < name: \"Sync Dictionary\" >\n"
      ">",
      *status_);
#else
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"dictionary\" >\n"
      ">",
      *status_);
#endif

  // Delete the session.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
}

TEST_F(UserDictionarySessionHandlerTest, LoadWithEnsuringNonEmptyStorage) {
  // Create a session.
  command_->set_type(UserDictionaryCommand::CREATE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  const uint64 session_id = status_->session_id();

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
#ifdef ENABLE_CLOUD_SYNC
  // If CLOUD_SYNC is enabled, Load automatically creates a Sync dictionary.
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"Sync Dictionary\" >\n"
      "  dictionaries: < name: \"abcde\" >\n"
      ">",
      *status_);
#else
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"abcde\" >\n"
      ">",
      *status_);
#endif

  // Delete the session.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
}

TEST_F(UserDictionarySessionHandlerTest, Undo) {
  // Create a session.
  command_->set_type(UserDictionaryCommand::CREATE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  const uint64 session_id = status_->session_id();

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
  Clear();
  command_->set_type(UserDictionaryCommand::CREATE_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("dictionary");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

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

  // Delete the session.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
}

TEST_F(UserDictionarySessionHandlerTest, DictionaryEdit) {
  // Create a session.
  command_->set_type(UserDictionaryCommand::CREATE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  const uint64 session_id = status_->session_id();

  // Create a dictionary named "dictionary".
  Clear();
  command_->set_type(UserDictionaryCommand::CREATE_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("dictionary");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "storage: <\n"
                   "  dictionaries: < name: \"dictionary\" >\n"
                   ">",
                   *status_);

  // Create another dictionary named "dictionary2".
  Clear();
  command_->set_type(UserDictionaryCommand::CREATE_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("dictionary2");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "storage: <\n"
                   "  dictionaries: < name: \"dictionary\" >\n"
                   "  dictionaries: < name: \"dictionary2\" >\n"
                   ">",
                   *status_);
  const uint64 dictionary_id1 = status_->storage().dictionaries(0).id();
  const uint64 dictionary_id2 = status_->storage().dictionaries(1).id();

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
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "storage: <\n"
                   "  dictionaries: < name: \"dictionary\" >\n"
                   "  dictionaries: < name: \"dictionary3\" >\n"
                   ">",
                   *status_);
  EXPECT_EQ(dictionary_id1, status_->storage().dictionaries(0).id());
  EXPECT_EQ(dictionary_id2, status_->storage().dictionaries(1).id());

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
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "storage: <\n"
                   "  dictionaries: < name: \"dictionary3\" >\n"
                   ">",
                   *status_);
  EXPECT_EQ(dictionary_id2, status_->storage().dictionaries(0).id());

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
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "storage: <\n"
                   "  dictionaries: < name: \"abcde\" >\n"
                   ">",
                   *status_);
  EXPECT_NE(dictionary_id2, status_->storage().dictionaries(0).id());

  // Delete the session.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
}

TEST_F(UserDictionarySessionHandlerTest, EntryEdit) {
  // Create a session.
  command_->set_type(UserDictionaryCommand::CREATE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  const uint64 session_id = status_->session_id();

  // Create a dictionary in which entries are kept.
  Clear();
  command_->set_type(UserDictionaryCommand::CREATE_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("dictionary");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);
  const uint64 dictionary_id = status_->dictionary_id();

  // At first, the dictionary should be empty (i.e. the size should be 0).
  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY_SIZE);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                  "entry_size: 0",
                  *status_);

  // Add an entry.
  Clear();
  command_->set_type(UserDictionaryCommand::ADD_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  {
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key("reading");
    entry->set_value("word");
    entry->set_pos(UserDictionary::NOUN);
  }
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY_SIZE);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                  "entry_size: 1",
                  *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(0);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "entry: <\n"
                   "  key: \"reading\"\n"
                   "  value: \"word\"\n"
                   "  pos: NOUN\n"
                   ">",
                   *status_);

  // Add another entry.
  Clear();
  command_->set_type(UserDictionaryCommand::ADD_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  {
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key("reading2");
    entry->set_value("word2");
    entry->set_pos(UserDictionary::NOUN);
  }
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY_SIZE);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                  "entry_size: 2",
                  *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(0);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "entry: <\n"
                   "  key: \"reading\"\n"
                   "  value: \"word\"\n"
                   "  pos: NOUN\n"
                   ">",
                   *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(1);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "entry: <\n"
                   "  key: \"reading2\"\n"
                   "  value: \"word2\"\n"
                   "  pos: NOUN\n"
                   ">",
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

  // Edit the second entry.
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

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY_SIZE);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                  "entry_size: 2",
                  *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(0);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "entry: <\n"
                   "  key: \"reading\"\n"
                   "  value: \"word\"\n"
                   "  pos: NOUN\n"
                   ">",
                   *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(1);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "entry: <\n"
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

  // As deleting preparation add three more entries.
  static const char *kReadings[] = { "reading4", "reading5", "reading6" };
  static const char *kWords[] = { "word4", "word5", "word6" };
  for (int i = 0; i < 3; ++i) {
    Clear();
    command_->set_type(UserDictionaryCommand::ADD_ENTRY);
    command_->set_session_id(session_id);
    command_->set_dictionary_id(dictionary_id);
    {
      UserDictionary::Entry *entry = command_->mutable_entry();
      entry->set_key(kReadings[i]);
      entry->set_value(kWords[i]);
      entry->set_pos(UserDictionary::NOUN);
    }
    ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);
  }

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY_SIZE);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                  "entry_size: 5",
                  *status_);

  // Delete the second and fourth entries.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(1);
  command_->add_entry_index(3);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY_SIZE);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                  "entry_size: 3",
                  *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(0);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "entry: <\n"
                   "  key: \"reading\"\n"
                   "  value: \"word\"\n"
                   "  pos: NOUN\n"
                   ">",
                   *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(1);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "entry: <\n"
                   "  key: \"reading4\"\n"
                   "  value: \"word4\"\n"
                   "  pos: NOUN\n"
                   ">",
                   *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(2);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "entry: <\n"
                   "  key: \"reading6\"\n"
                   "  value: \"word6\"\n"
                   "  pos: NOUN\n"
                   ">",
                   *status_);

  // Entry deletion without dictionary_id or entry_index should be failed.
  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY);
  command_->set_session_id(session_id);
  command_->add_entry_index(0);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  // Delete the session.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
}

TEST_F(UserDictionarySessionHandlerTest, ImportData1) {
  // Create a session.
  command_->set_type(UserDictionaryCommand::CREATE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  const uint64 session_id = status_->session_id();

  // First of all, create a dictionary named "dictionary".
  Clear();
  command_->set_type(UserDictionaryCommand::CREATE_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("dictionary");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  const uint64 dictionary_id = status_->dictionary_id();

  // Import data to the dictionary.
  Clear();
  command_->set_type(UserDictionaryCommand::IMPORT_DATA);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->set_data(kDictionaryData);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);
  ASSERT_TRUE(status_->has_dictionary_id());
  EXPECT_EQ(dictionary_id, status_->dictionary_id());

  // Make sure the size of the data.
  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY_SIZE);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                  "entry_size: 4",
                  *status_);

  // Delete the session.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
}

TEST_F(UserDictionarySessionHandlerTest, ImportData2) {
  // Create a session.
  command_->set_type(UserDictionaryCommand::CREATE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  const uint64 session_id = status_->session_id();

  // Import data to a new dictionary.
  Clear();
  command_->set_type(UserDictionaryCommand::IMPORT_DATA);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("user dictionary");
  command_->set_data(kDictionaryData);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);
  ASSERT_TRUE(status_->has_dictionary_id());
  const uint64 dictionary_id = status_->dictionary_id();

  // Make sure the size of the data.
  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRY_SIZE);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                  "entry_size: 4",
                  *status_);

  // Delete the session.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
}

TEST_F(UserDictionarySessionHandlerTest, ImportDataFailure) {
  // Create a session.
  command_->set_type(UserDictionaryCommand::CREATE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  const uint64 session_id = status_->session_id();

  // First of all, create a dictionary named "dictionary".
  Clear();
  command_->set_type(UserDictionaryCommand::CREATE_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("dictionary");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  const uint64 dictionary_id = status_->dictionary_id();

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

  // Delete the session.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
}

}  // namespace mozc

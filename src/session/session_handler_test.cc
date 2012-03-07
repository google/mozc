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

#include <algorithm>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/converter_mock.h"
#include "converter/user_data_manager_mock.h"
#include "session/commands.pb.h"
#include "session/generic_storage_manager.h"
#include "session/japanese_session_factory.h"
#include "session/session_handler.h"
#include "session/session_handler_test_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_int32(max_session_size);
DECLARE_int32(create_session_min_interval);
DECLARE_int32(last_command_timeout);
DECLARE_int32(last_create_session_timeout);

namespace mozc {

using mozc::session::testing::CreateSession;
using mozc::session::testing::CleanUp;
using mozc::session::testing::IsGoodSession;

using mozc::session::testing::JapaneseSessionHandlerTestBase;
class SessionHandlerTest : public JapaneseSessionHandlerTestBase {
};

TEST_F(SessionHandlerTest, MaxSessionSizeTest) {
  FLAGS_create_session_min_interval = 1;

  // The oldest item is remvoed
  const size_t session_size = 3;
  FLAGS_max_session_size = static_cast<int32>(session_size);
  {
    SessionHandler handler;

    // Create session_size + 1 sessions
    vector<uint64> ids;
    for (size_t i = 0; i <= session_size; ++i) {
      uint64 id = 0;
      EXPECT_TRUE(CreateSession(&handler, &id));
      ids.push_back(id);
      Util::Sleep(1500);  // 1.5 sec
    }

    for (int i = static_cast<int>(ids.size() - 1); i >= 0; --i) {
      if (i > 0) {   // this id is alive
        EXPECT_TRUE(IsGoodSession(&handler, ids[i]));
      } else {  // the first id shuold be removed
        EXPECT_FALSE(IsGoodSession(&handler, ids[i]));
      }
    }
  }

  FLAGS_max_session_size = static_cast<int32>(session_size);
  {
    SessionHandler handler;

    // Create session_size sessions
    vector<uint64> ids;
    for (size_t i = 0; i < session_size; ++i) {
      uint64 id = 0;
      EXPECT_TRUE(CreateSession(&handler, &id));
      ids.push_back(id);
      Util::Sleep(1500);  // 1.5 sec
    }

    random_shuffle(ids.begin(), ids.end());
    const uint64 oldest_id = ids[0];
    for (size_t i = 0; i < session_size; ++i) {
      EXPECT_TRUE(IsGoodSession(&handler, ids[i]));
    }

    // Create new session
    uint64 id = 0;
    EXPECT_TRUE(CreateSession(&handler, &id));

    // the oldest id no longer exists
    EXPECT_FALSE(IsGoodSession(&handler, oldest_id));
  }
}

TEST_F(SessionHandlerTest, CreateSessionMinInterval) {
  const size_t timeout = 2;
  FLAGS_create_session_min_interval = static_cast<int32>(timeout);
  SessionHandler handler;

  uint64 id = 0;
  EXPECT_TRUE(CreateSession(&handler, &id));

  // cannot do it
  EXPECT_FALSE(CreateSession(&handler, &id));

  // wait 0.5 * timeout sec
  Util::Sleep(timeout * 1000 / 2);

  EXPECT_FALSE(CreateSession(&handler, &id));

  // wait timeout
  Util::Sleep(timeout * 1000);

  EXPECT_TRUE(CreateSession(&handler, &id));
}

TEST_F(SessionHandlerTest, LastCreateSessionTimeout) {
  FLAGS_last_create_session_timeout = 2;  // 2 sec
  SessionHandler handler;
  uint64 id = 0;
  EXPECT_TRUE(CreateSession(&handler, &id));

  // wait 3 sec
  Util::Sleep(3000);
  EXPECT_TRUE(CleanUp(&handler));

  // the session is removed by server
  EXPECT_FALSE(IsGoodSession(&handler, id));
}

TEST_F(SessionHandlerTest, LastCommandTimeout) {
  FLAGS_last_command_timeout = 10;  // 10 sec
  SessionHandler handler;
  uint64 id = 0;
  EXPECT_TRUE(CreateSession(&handler, &id));

  EXPECT_TRUE(CleanUp(&handler));

  Util::Sleep(200);
  EXPECT_TRUE(IsGoodSession(&handler, id));

  Util::Sleep(200);
  EXPECT_TRUE(IsGoodSession(&handler, id));

  Util::Sleep(200);
  EXPECT_TRUE(IsGoodSession(&handler, id));

  Util::Sleep(12000);
  EXPECT_TRUE(CleanUp(&handler));
  EXPECT_FALSE(IsGoodSession(&handler, id));
}

TEST_F(SessionHandlerTest, VerifySyncIsCalled) {
  {
    // This test consumes much stack so to reduce the stack size
    // here we use scoped_ptr instead of local variable.
    scoped_ptr<ConverterMock> converter_mock(new ConverterMock);
    // This Mock is released inside of ConverterMock
    UserDataManagerMock *user_data_manager_mock = new UserDataManagerMock();
    converter_mock->SetUserDataManager(user_data_manager_mock);
    ConverterFactory::SetConverter(converter_mock.get());
    SessionHandler handler;
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::DELETE_SESSION);
    EXPECT_EQ(0, user_data_manager_mock->GetFunctionCallCount("Sync"));
    handler.EvalCommand(&command);
    EXPECT_EQ(1, user_data_manager_mock->GetFunctionCallCount("Sync"));
  }

  {
    scoped_ptr<ConverterMock> converter_mock(new ConverterMock);
    // This Mock is released inside of ConverterMock
    UserDataManagerMock *user_data_manager_mock = new UserDataManagerMock();
    converter_mock->SetUserDataManager(user_data_manager_mock);
    ConverterFactory::SetConverter(converter_mock.get());
    SessionHandler handler;
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::CLEANUP);
    EXPECT_EQ(0, user_data_manager_mock->GetFunctionCallCount("Sync"));
    handler.EvalCommand(&command);
    EXPECT_EQ(1, user_data_manager_mock->GetFunctionCallCount("Sync"));
  }
}

const char *kStorageTestData[] = {
  "angel", "bishop", "chariot", "dragon",
};

class MockStorage : public GenericStorageInterface {
 public:
  int insert_count;
  int clear_count;
  const char **insert_expect;

  MockStorage() : insert_count(0), clear_count(0) {}
  virtual ~MockStorage() {}

  virtual bool Insert(const string &key, const char *value) {
    EXPECT_EQ(string(insert_expect[insert_count]), key);
    EXPECT_EQ(string(insert_expect[insert_count]), string(value));
    ++insert_count;
    return true;
  };

  virtual const char *Lookup(const string &key) {
    return NULL;
  }

  virtual bool GetAllValues(vector<string> *values) {
    values->clear();
    for (size_t i = 0; i < arraysize(kStorageTestData); ++i) {
      values->push_back(kStorageTestData[i]);
    }
    return true;
  }

  virtual bool Clear() {
    ++clear_count;
    return true;
  }

  void SetInsertExpect(const char **expect) {
    insert_expect = expect;
  }
};

class MockStorageManager : public GenericStorageManagerInterface {
 public:
  virtual GenericStorageInterface *GetStorage(
     commands::GenericStorageEntry::StorageType storage_type) {
    return storage;
  }

  void SetStorage(MockStorage *newStorage) {
    storage = newStorage;
  }

 private:
  MockStorage *storage;
};

// Tests basic behavior of InsertToStorage and ReadAllFromStorage methods.
TEST_F(SessionHandlerTest, StorageTest) {
  // Inject mock objects.
  MockStorageManager storageManager;
  GenericStorageManagerFactory::SetGenericStorageManager(&storageManager);
  SessionHandler handler;
  {
    // InsertToStorage
    MockStorage mock_storage;
    mock_storage.SetInsertExpect(kStorageTestData);
    storageManager.SetStorage(&mock_storage);
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::INSERT_TO_STORAGE);
    commands::GenericStorageEntry *storage_entry =
        command.mutable_input()->mutable_storage_entry();
    storage_entry->set_type(commands::GenericStorageEntry::SYMBOL_HISTORY);
    storage_entry->mutable_key()->assign("dummy key");
    for (size_t i = 0; i < arraysize(kStorageTestData); ++i) {
      storage_entry->mutable_value()->Add()->assign(kStorageTestData[i]);
    }
    EXPECT_TRUE(handler.InsertToStorage(&command));
    EXPECT_EQ(arraysize(kStorageTestData), mock_storage.insert_count);
  }
  {
    // ReadAllFromStorage
    MockStorage mock_storage;
    storageManager.SetStorage(&mock_storage);
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::READ_ALL_FROM_STORAGE);
    commands::GenericStorageEntry *storage_entry =
        command.mutable_input()->mutable_storage_entry();
    storage_entry->set_type(commands::GenericStorageEntry::EMOTICON_HISTORY);
    EXPECT_TRUE(handler.ReadAllFromStorage(&command));
    EXPECT_EQ(
        commands::GenericStorageEntry::EMOTICON_HISTORY,
        command.output().storage_entry().type());
    EXPECT_EQ(
        arraysize(kStorageTestData),
        command.output().storage_entry().value().size());
  }
  {
    // Clear
    MockStorage mock_storage;
    storageManager.SetStorage(&mock_storage);
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::CLEAR_STORAGE);
    commands::GenericStorageEntry *storage_entry =
        command.mutable_input()->mutable_storage_entry();
    storage_entry->set_type(commands::GenericStorageEntry::EMOTICON_HISTORY);
    EXPECT_TRUE(handler.ClearStorage(&command));
    EXPECT_EQ(
        commands::GenericStorageEntry::EMOTICON_HISTORY,
        command.output().storage_entry().type());
    EXPECT_EQ(1, mock_storage.clear_count);
  }
}

}  // namespace mozc

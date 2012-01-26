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

#include <string>
#include "base/base.h"
#include "base/util.h"
#include "base/thread.h"
#include "client/client.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "ipc/named_event.h"
#include "languages/global_language_spec.h"
#include "languages/japanese/lang_dep_spec.h"
#include "net/http_client_mock.h"
#include "sync/sync_handler.h"
#include "sync/syncer_interface.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);
DECLARE_int32(min_sync_interval);

namespace mozc {

using config::Config;
using config::ConfigHandler;
using config::SyncConfig;

namespace sync {

class MockSyncer : public SyncerInterface {
 public:
  virtual bool Start()  {
    is_start_called_ = true;
    return start_result_;
  }

  virtual bool Sync(bool *reload_required) {
    Util::Sleep(operation_duration_);
    is_sync_called_ = true;
    *reload_required = reload_required_;
    return sync_result_;
  }

  virtual bool Clear() {
    Util::Sleep(operation_duration_);
    is_clear_called_ = true;
    return clear_result_;
  }

  void SetOperationDuration(int duration) {
    operation_duration_ = duration;
  }

  void SetStartResult(bool result) {
    start_result_ = result;
  }

  void SetSyncResult(bool result) {
    sync_result_ = result;
  }

  void SetClearResult(bool result) {
    clear_result_ = result;
  }

  void SetReloadRequired(bool result) {
    reload_required_ = result;
  }

  bool IsStartCalled() const {
    return is_start_called_;
  }

  bool IsSyncCalled() const {
    return is_sync_called_;
  }

  bool IsClearCalled() const {
    return is_clear_called_;
  }

  void Reset() {
    is_start_called_  = false;
    is_sync_called_   = false;
    is_clear_called_  = false;
    start_result_     = true;
    sync_result_      = true;
    clear_result_     = true;
    reload_required_  = true;
    operation_duration_ = 0;
  }

 private:
  bool is_start_called_;
  bool is_sync_called_;
  bool is_clear_called_;
  bool start_result_;
  bool sync_result_;
  bool clear_result_;
  bool reload_required_;
  int operation_duration_;
};

class SyncHandlerTest : public testing::Test {
 public:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    HTTPClient::SetHTTPClientHandler(&client_);
    language::GlobalLanguageSpec::SetLanguageDependentSpec(&lang_dep_spec_);
    SyncerFactory::SetSyncer(&syncer_);

    ConfigHandler::SetConfigFileName("memory://config");

    Config config = ConfigHandler::GetConfig();
    SyncConfig *sync_config = config.mutable_sync_config();
    sync_config->set_use_config_sync(true);
    sync_config->set_use_user_dictionary_sync(true);
    sync_config->set_use_user_history_sync(true);
    sync_config->set_use_contact_list_sync(true);
    sync_config->set_use_learning_preference_sync(true);
    ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    language::GlobalLanguageSpec::SetLanguageDependentSpec(NULL);
  }

  MockSyncer *GetSyncer() {
    return &syncer_;
  }

 private:
  HTTPClientMock client_;
  MockSyncer syncer_;
  japanese::LangDepSpecJapanese lang_dep_spec_;
};

namespace {
class NamedEventListenerThread : public Thread {
 public:
  explicit NamedEventListenerThread(int timeout)
      : listener_("sync"), timeout_(timeout), result_(false) {
    EXPECT_TRUE(listener_.IsAvailable());
  }

  ~NamedEventListenerThread() {
    Join();
  }

  void Run() {
    result_ = listener_.Wait(timeout_);
  }

  bool GetResult() {
    Join();
    return result_;
  }

 private:
  NamedEventListener listener_;
  int32 timeout_;
  bool result_;
};
}

TEST_F(SyncHandlerTest, NotificationTest) {
  FLAGS_min_sync_interval = 0;
  MockSyncer *syncer = GetSyncer();

  {
    syncer->Reset();
    syncer->SetOperationDuration(2000);  // sync takes 2000 msec

    // Call three times to test IsRunning state.
    EXPECT_TRUE(SyncHandler::Sync());
    EXPECT_TRUE(SyncHandler::Sync());
    EXPECT_TRUE(SyncHandler::Sync());

    {
      NamedEventListener listener("sync");
      EXPECT_TRUE(listener.IsAvailable());
      // Not signaled within 500msec
      EXPECT_FALSE(listener.Wait(500));
    }

    {
      NamedEventListener listener("sync");
      EXPECT_TRUE(listener.IsAvailable());
      // signaled within 5000msec
      EXPECT_TRUE(listener.Wait(5000));
    }

    EXPECT_TRUE(syncer->IsStartCalled());
    EXPECT_TRUE(syncer->IsSyncCalled());
    EXPECT_FALSE(syncer->IsClearCalled());
    SyncHandler::Wait();
  }

  {
    NamedEventListenerThread listener(500);
    listener.Start();
    Util::Sleep(200);
    syncer->Reset();
    syncer->SetOperationDuration(2000);  // sync takes 2000 msec
    syncer->SetStartResult(false);
    EXPECT_FALSE(SyncHandler::Sync());

    // event is singled immediately (500msec).
    EXPECT_TRUE(listener.GetResult());

    EXPECT_TRUE(syncer->IsStartCalled());
    EXPECT_FALSE(syncer->IsSyncCalled());
    EXPECT_FALSE(syncer->IsClearCalled());
    SyncHandler::Wait();
  }

  {
    syncer->Reset();
    syncer->SetOperationDuration(2000); // clear takes 2000 msec

    // Call three times to test IsRunning state.
    EXPECT_TRUE(SyncHandler::Clear());
    EXPECT_TRUE(SyncHandler::Clear());
    EXPECT_TRUE(SyncHandler::Clear());

    {
      NamedEventListener listener("sync");
      EXPECT_TRUE(listener.IsAvailable());
      // Not signaled within 500msec
      EXPECT_FALSE(listener.Wait(500));
    }

    {
      NamedEventListener listener("sync");
      EXPECT_TRUE(listener.IsAvailable());
      // signaled within 5000msec
      EXPECT_TRUE(listener.Wait(5000));
    }

    EXPECT_FALSE(syncer->IsStartCalled());
    EXPECT_FALSE(syncer->IsSyncCalled());
    EXPECT_TRUE(syncer->IsClearCalled());
    SyncHandler::Wait();
  }
}

namespace {
bool g_is_reload_called = false;

class MockClient : public client::Client {
 public:
  virtual bool Reload() {
    g_is_reload_called = true;
    return true;
  }
};

class MockClientFactory : public client::ClientFactoryInterface {
 public:
  virtual client::ClientInterface *NewClient() {
    return new MockClient;
  }
};
}  // namespace

TEST_F(SyncHandlerTest, SendReloadCommand) {
  FLAGS_min_sync_interval = 0;
  MockSyncer *syncer = GetSyncer();
  syncer->SetOperationDuration(0);
  MockClientFactory mock_client_factory;
  client::ClientFactory::SetClientFactory(&mock_client_factory);

  {
    g_is_reload_called = false;
    syncer->SetStartResult(true);
    syncer->SetSyncResult(true);
    syncer->SetReloadRequired(true);
    SyncHandler::Sync();
    SyncHandler::Wait();
    EXPECT_TRUE(g_is_reload_called);
  }

  {
    g_is_reload_called = false;
    syncer->SetStartResult(true);
    syncer->SetSyncResult(false);
    syncer->SetReloadRequired(true);
    SyncHandler::Sync();
    SyncHandler::Wait();
    EXPECT_FALSE(g_is_reload_called);
  }

  {
    g_is_reload_called = false;
    syncer->SetStartResult(true);
    syncer->SetSyncResult(true);
    syncer->SetReloadRequired(false);
    SyncHandler::Sync();
    SyncHandler::Wait();
    EXPECT_FALSE(g_is_reload_called);
  }

  {
    g_is_reload_called = false;
    syncer->SetStartResult(false);
    syncer->SetSyncResult(true);
    syncer->SetReloadRequired(true);
    SyncHandler::Sync();
    SyncHandler::Wait();
    EXPECT_FALSE(g_is_reload_called);
  }

  {
    g_is_reload_called = false;
    syncer->SetSyncResult(true);
    syncer->SetClearResult(true);
    syncer->SetReloadRequired(true);
    SyncHandler::Clear();
    SyncHandler::Wait();
    EXPECT_FALSE(g_is_reload_called);
  }

  client::ClientFactory::SetClientFactory(NULL);
}

TEST_F(SyncHandlerTest, MinIntervalTest) {
  MockSyncer *syncer = GetSyncer();
  FLAGS_min_sync_interval = 2;

  Util::Sleep(3000);

  {
    syncer->Reset();
    EXPECT_TRUE(SyncHandler::Sync());
    Util::Sleep(200);
    EXPECT_TRUE(syncer->IsSyncCalled());
    syncer->Reset();
    EXPECT_TRUE(SyncHandler::Sync());
    Util::Sleep(200);
    // Sync is not called because two Sync calls are very close.
    EXPECT_FALSE(syncer->IsSyncCalled());

    // Wait for 2 sec and call Sync again.
    syncer->Reset();
    Util::Sleep(2000);
    EXPECT_TRUE(SyncHandler::Sync());
    Util::Sleep(200);
    // Then it succeeds.
    EXPECT_TRUE(syncer->IsSyncCalled());
    SyncHandler::Wait();
  }

  Util::Sleep(3000);
  {
    syncer->Reset();
    EXPECT_TRUE(SyncHandler::Sync());
    Util::Sleep(200);
    EXPECT_TRUE(syncer->IsSyncCalled());
    syncer->Reset();
    EXPECT_FALSE(syncer->IsClearCalled());
    EXPECT_TRUE(SyncHandler::Clear());
    Util::Sleep(200);
    // Clear is still called after the sync command even within the
    // minimum interval.
    EXPECT_TRUE(syncer->IsClearCalled());

    // Wait for 2 sec and call Clear again.
    syncer->Reset();
    Util::Sleep(2000);
    EXPECT_TRUE(SyncHandler::Clear());
    Util::Sleep(200);
    // Clear is never called after Clear call.
    EXPECT_FALSE(syncer->IsClearCalled());
    SyncHandler::Wait();
  }

  Util::Sleep(3000);
  {
    syncer->Reset();
    EXPECT_TRUE(SyncHandler::Sync());
    Util::Sleep(200);
    EXPECT_TRUE(syncer->IsSyncCalled());
    syncer->Reset();
    EXPECT_FALSE(syncer->IsClearCalled());
    EXPECT_TRUE(SyncHandler::Clear());
    Util::Sleep(200);
    // Clear is still called after the sync command even within the
    // minimum interval.
    EXPECT_TRUE(syncer->IsClearCalled());

    // Sync is not called because it's within the minimum interval.
    syncer->Reset();
    EXPECT_TRUE(SyncHandler::Sync());
    Util::Sleep(200);
    EXPECT_FALSE(syncer->IsSyncCalled());
    SyncHandler::Wait();

    // Wait for 2 sec and sync will succeed.
    syncer->Reset();
    Util::Sleep(2000);
    EXPECT_TRUE(SyncHandler::Sync());
    Util::Sleep(200);
    EXPECT_TRUE(syncer->IsSyncCalled());
    SyncHandler::Wait();
  }
}
}  // sync
}  // mozc

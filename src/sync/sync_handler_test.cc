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
#include "base/clock_mock.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "base/util.h"
#include "client/client.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "ipc/named_event.h"
#include "net/http_client_mock.h"
#include "sync/oauth2_client.h"
#include "sync/oauth2_util.h"
#include "sync/sync_handler.h"
#include "sync/sync_status_manager.h"
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
    original_min_sync_interval_ = FLAGS_min_sync_interval;
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    HTTPClient::SetHTTPClientHandler(&client_);
    syncer_.Reset();
    SyncerFactory::SetSyncer(&syncer_);

    original_config_filename_ = ConfigHandler::GetConfigFileName();
    ConfigHandler::SetConfigFileName("memory://config");

    Config config = ConfigHandler::GetConfig();
    SyncConfig *sync_config = config.mutable_sync_config();
    sync_config->set_use_config_sync(true);
    sync_config->set_use_user_dictionary_sync(true);
    sync_config->set_use_user_history_sync(true);
    sync_config->set_use_contact_list_sync(true);
    sync_config->set_use_learning_preference_sync(true);
    ConfigHandler::SetConfig(config);

    SyncStatusReset();
  }

  virtual void TearDown() {
    // Since SyncHandler internally uses Singleton<SyncerThread> which is
    // stateful, it is important to initialize its internal state here.
    // Otherwise, any subsequent test which will be invoked in this process may
    // run under unusual global settings.
    SyncHandler::Wait();
    SyncHandler::ClearLastCommandAndSyncTime();

    // Also restores following global state for the subsequent test.
    ConfigHandler::SetConfigFileName(original_config_filename_);
    HTTPClient::SetHTTPClientHandler(NULL);
    SyncerFactory::SetSyncer(NULL);
    Util::SetClockHandler(NULL);
    FLAGS_min_sync_interval = original_min_sync_interval_;
  }

  MockSyncer *GetSyncer() {
    return &syncer_;
  }

  void SyncStatusReset() {
    // Reset sync status assuming authorization succeeds.
    Singleton<SyncStatusManager>::get()->SetSyncGlobalStatus(
        commands::CloudSyncStatus::INSYNC);
    Singleton<SyncStatusManager>::get()->NewSyncStatusSession();
  }

 private:
  string original_config_filename_;
  string config_filename_;
  HTTPClientMock client_;
  MockSyncer syncer_;
  int32 original_min_sync_interval_;
};

class ScopedClockMock {
 public:
  ScopedClockMock()
      : clock_mock_(Util::GetTime(), 0) {
    Util::SetClockHandler(&clock_mock_);
  }
  ~ScopedClockMock() {
    Util::SetClockHandler(NULL);
  }
  ClockMock *get() {
    return &clock_mock_;
  }

 private:
  ClockMock clock_mock_;
  DISALLOW_COPY_AND_ASSIGN(ScopedClockMock);
};

namespace {
class NamedEventListenerThread : public Thread {
 public:
  explicit NamedEventListenerThread(int timeout)
      : listener_("sync"), timeout_(timeout), result_(false) {
    LOG_IF(FATAL, !listener_.IsAvailable())
        << "Failed to intialize named event listener.";
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
}  // namespace

TEST_F(SyncHandlerTest, NotificationTest) {
  FLAGS_min_sync_interval = 0;
  MockSyncer *syncer = GetSyncer();

  // Implementation note:
  //    We intentionally do not assert any timing condition like
  //      "this operation should be finished within X seconds",
  //    as it is not naturally guaranteed on preemptive multitasking operation
  //    system, especially on highly virtualized test environment. Here we have
  //    a long enough timeout just to prevent this test from getting stuck.
  //    See b/6407046 for the background of the flakiness of this test.
  const int kTimeout = 30 * 1000;  // 60 sec.
  const int kSyncDuation = 1000;  // 1 sec.

  {
    syncer->Reset();
    syncer->SetOperationDuration(kSyncDuation);

    // Call three times to test IsRunning state.
    EXPECT_TRUE(SyncHandler::Sync());
    EXPECT_TRUE(SyncHandler::Sync());
    EXPECT_TRUE(SyncHandler::Sync());

    {
      NamedEventListener listener("sync");
      ASSERT_TRUE(listener.IsAvailable());
      // should be signaled eventually.
      EXPECT_TRUE(listener.Wait(kTimeout));
    }

    EXPECT_TRUE(syncer->IsStartCalled());
    EXPECT_TRUE(syncer->IsSyncCalled());
    EXPECT_FALSE(syncer->IsClearCalled());
    SyncHandler::Wait();
  }

  {
    NamedEventListenerThread listener(kTimeout);
    listener.Start();
    Util::Sleep(200);
    syncer->Reset();
    syncer->SetOperationDuration(kSyncDuation);
    syncer->SetStartResult(false);
    EXPECT_FALSE(SyncHandler::Sync());

    // should be signaled eventually.
    EXPECT_TRUE(listener.GetResult());

    EXPECT_TRUE(syncer->IsStartCalled());
    EXPECT_FALSE(syncer->IsSyncCalled());
    EXPECT_FALSE(syncer->IsClearCalled());
    SyncHandler::Wait();
  }

  {
    syncer->Reset();
    syncer->SetOperationDuration(kSyncDuation);

    // Call three times to test IsRunning state.
    EXPECT_TRUE(SyncHandler::Clear());
    EXPECT_TRUE(SyncHandler::Clear());
    EXPECT_TRUE(SyncHandler::Clear());

    {
      NamedEventListener listener("sync");
      ASSERT_TRUE(listener.IsAvailable());
      // should be signaled eventually.
      EXPECT_TRUE(listener.Wait(kTimeout));
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
  ScopedClockMock clock_mock;
  MockSyncer *syncer = GetSyncer();
  // The resolution of timing calculation in SyncHandler is 1 second.
  // So |FLAGS_min_sync_interval| must be relatively larger than the
  // resolution to stabilize this test.
  FLAGS_min_sync_interval = 5;  // seconds

  {
    syncer->Reset();
    {
      NamedEventListener listener("sync");
      ASSERT_TRUE(listener.IsAvailable());
      EXPECT_TRUE(SyncHandler::Sync());
      ASSERT_TRUE(listener.Wait(1000))
          << "Initial Sync should be invoked immediately";
      EXPECT_TRUE(syncer->IsSyncCalled())
          << "Initial Sync should be invoked immediately";
      SyncHandler::Wait();
    }

    // Advance the clock 2 seconds.
    clock_mock.get()->PutClockForward(2, 0);
    syncer->Reset();
    {
      NamedEventListener listener("sync");
      ASSERT_TRUE(listener.IsAvailable());
      EXPECT_TRUE(SyncHandler::Sync());
      ASSERT_FALSE(listener.Wait(500))
          << "Subsequent Sync call should wait for the next sync time "
             "window with minimum sync interval.";
      EXPECT_FALSE(syncer->IsSyncCalled())
          << "Subsequent Sync call should wait for the next sync time "
             "window with minimum sync interval.";

      ASSERT_TRUE(listener.Wait(FLAGS_min_sync_interval * 1000))
          << "The second Sync call should be finished.";
      SyncHandler::Wait();
    }

    // Advance the clock 2 wait intervals.
    clock_mock.get()->PutClockForward(FLAGS_min_sync_interval * 2, 0);
    syncer->Reset();
    {
      NamedEventListener listener("sync");
      ASSERT_TRUE(listener.IsAvailable());
      EXPECT_TRUE(SyncHandler::Sync());
      ASSERT_TRUE(listener.Wait(1000))
          << "With sufficient wait time, Sync should be invoked immediately.";
      EXPECT_TRUE(syncer->IsSyncCalled())
          << "With sufficient wait time, Sync should be invoked immediately.";
      SyncHandler::Wait();
    }

    // Advance the clock 1 second.
    clock_mock.get()->PutClockForward(1, 0);
    syncer->Reset();
    {
      NamedEventListener listener("sync");
      ASSERT_TRUE(listener.IsAvailable());
      EXPECT_TRUE(SyncHandler::Clear());
      ASSERT_TRUE(listener.Wait(1000))
          << "Even within the minimum interval, Clear should be invoked "
             "immediately.";
      EXPECT_TRUE(syncer->IsClearCalled())
          << "Even within the minimum interval, Clear should be invoked "
             "immediately.";
      SyncHandler::Wait();
    }
  }
}

TEST_F(SyncHandlerTest, ClearTest) {
  MockSyncer *syncer = GetSyncer();
  FLAGS_min_sync_interval = 0;

  {
    syncer->Reset();
    {
      NamedEventListener listener("sync");
      ASSERT_TRUE(listener.IsAvailable());
      EXPECT_TRUE(SyncHandler::Clear());
      ASSERT_TRUE(listener.Wait(1000))
          << "Initial Clear should be invoked immediately.";
      EXPECT_TRUE(syncer->IsClearCalled())
          << "Initial Clear should be invoked immediately.";
      SyncHandler::Wait();
    }

    syncer->Reset();
    {
      NamedEventListener listener("sync");
      ASSERT_TRUE(listener.IsAvailable());
      EXPECT_TRUE(SyncHandler::Clear());
      ASSERT_FALSE(listener.Wait(250))
          << "Subsequent Clear should not be simply ignored.";
      EXPECT_FALSE(syncer->IsClearCalled())
          << "Subsequent Clear should not be simply ignored.";
      SyncHandler::Wait();
    }

    syncer->Reset();
    {
      NamedEventListener listener("sync");
      ASSERT_TRUE(listener.IsAvailable());
      EXPECT_TRUE(SyncHandler::Sync());
      ASSERT_TRUE(listener.Wait(1000))
          << "Sync after Clear should be invoked immediately because "
             "SyncHandler::Clear() removes auth token.";
      EXPECT_FALSE(syncer->IsSyncCalled())
          << "Sync after Clear should fail immediately because "
             "SyncHandler::Clear() removes auth token.";
      SyncHandler::Wait();
    }
  }
}

TEST_F(SyncHandlerTest, AuthorizationFailedTest) {
  const char kCorrectAuthToken[] = "a_correct_token";
  const char kWrongAuthToken[] = "a_wrong_token";

  // Set up environment
  const OAuth2Client *oauth2_client = OAuth2Client::GetDefaultClient();
  OAuth2Util oauth2_util(oauth2_client);
  vector<pair<string, string> > params;
  params.push_back(make_pair("grant_type", "authorization_code"));
  params.push_back(make_pair("client_id", oauth2_client->client_id_));
  params.push_back(make_pair("client_secret", oauth2_client->client_secret_));
  params.push_back(make_pair("redirect_uri",
                             oauth2_util.redirect_uri_for_unittest()));
  params.push_back(make_pair("code", kCorrectAuthToken));
  params.push_back(make_pair("scope", oauth2_util.scope_for_unittest()));

  HTTPClientMock::Result expect_result;
  expect_result.expected_url = oauth2_util.request_token_uri_for_unittest();
  Util::AppendCGIParams(params, &expect_result.expected_request);
  expect_result.expected_result = "{\"access_token\":\"1/correct_token\","
      "\"token_type\":\"Bearer\"}";

  HTTPClientMock client_mock;
  client_mock.set_result(expect_result);
  HTTPClient::SetHTTPClientHandler(&client_mock);

  // Authorization is expected to succeed.
  {
    commands::Input::AuthorizationInfo auth_info;
    auth_info.set_auth_code(kCorrectAuthToken);
    SyncHandler::SetAuthorization(auth_info);
    commands::CloudSyncStatus sync_status;
    SyncHandler::GetCloudSyncStatus(&sync_status);
    EXPECT_EQ(commands::CloudSyncStatus::INSYNC,
              sync_status.global_status());
  }

  // Authorization is expected to fail.
  {
    commands::Input::AuthorizationInfo auth_info;
    auth_info.set_auth_code(kWrongAuthToken);
    SyncHandler::SetAuthorization(auth_info);
    commands::CloudSyncStatus sync_status;
    SyncHandler::GetCloudSyncStatus(&sync_status);
    EXPECT_EQ(commands::CloudSyncStatus::NOSYNC,
              sync_status.global_status());
    EXPECT_EQ(1, sync_status.sync_errors_size());
    EXPECT_EQ(commands::CloudSyncStatus::AUTHORIZATION_FAIL,
              sync_status.sync_errors(0).error_code());
  }
}

}  // namespace sync
}  // namespace mozc

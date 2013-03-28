// Copyright 2010-2013, Google Inc.
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

#include "sync/sync_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/clock_mock.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/thread.h"
#include "base/util.h"
#include "client/client.h"
#include "client/client_interface.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "ipc/named_event.h"
#include "net/http_client.h"
#include "net/http_client_mock.h"
#include "session/commands.pb.h"
#include "storage/memory_storage.h"
#include "storage/registry.h"
#include "storage/storage_interface.h"
#include "sync/oauth2_client.h"
#include "sync/oauth2_server.h"
#include "sync/oauth2_util.h"
#include "sync/sync_status_manager.h"
#include "sync/syncer_interface.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);
DECLARE_int32(min_sync_interval);

namespace mozc {

using config::Config;
using config::ConfigHandler;
using config::SyncConfig;
using testing::Mock;
using testing::Return;

namespace sync {

class GmockSyncer : public SyncerInterface {
 public:
  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(ClearLocal, bool());
  MOCK_METHOD0(ClearInternal, bool());
  MOCK_METHOD0(SyncInternal, bool());

  GmockSyncer() {
    Reset();
  }

  virtual bool Sync(bool *reload_required) {
    Util::Sleep(operation_duration_);
    *reload_required = reload_required_;
    return SyncInternal();
  }

  virtual bool Clear() {
    Util::Sleep(operation_duration_);
    return ClearInternal();
  }

  void SetOperationDuration(int duration) {
    operation_duration_ = duration;
  }

  void SetReloadRequired(bool result) {
    reload_required_ = result;
  }

  void Reset() {
    reload_required_  = true;
    operation_duration_ = 0;
  }

 private:
  bool reload_required_;
  int operation_duration_;
};

class SyncHandlerTest : public testing::Test {
 public:
  virtual void SetUp() {
    original_min_sync_interval_ = FLAGS_min_sync_interval;
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);

    original_config_filename_ = ConfigHandler::GetConfigFileName();
    ConfigHandler::SetConfigFileName("memory://config");

    // Avoid connecting to the real web network in most tests just in case.
    // |client_| is expected not to be used in all tests.
    HTTPClient::SetHTTPClientHandler(&client_);

    client::ClientFactory::SetClientFactory(NULL);
    Util::SetClockHandler(NULL);
    storage_.reset(mozc::storage::MemoryStorage::New());
    mozc::storage::Registry::SetStorage(storage_.get());

    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    SyncConfig *sync_config = config.mutable_sync_config();
    sync_config->set_use_config_sync(true);
    sync_config->set_use_user_dictionary_sync(true);
    sync_config->set_use_user_history_sync(true);
    sync_config->set_use_contact_list_sync(true);
    sync_config->set_use_learning_preference_sync(true);
    ConfigHandler::SetConfig(config);

    sync_handler_.reset(new SyncHandler);
    syncer_ = new GmockSyncer;
    sync_handler_->SetSyncerForUnittest(syncer_);

    SyncStatusReset();
  }

  virtual void TearDown() {
    SyncStatusReset();

    sync_handler_.reset();
    syncer_ = NULL;

    // Also restores following global state for the subsequent test.
    HTTPClient::SetHTTPClientHandler(NULL);
    client::ClientFactory::SetClientFactory(NULL);
    Util::SetClockHandler(NULL);
    mozc::storage::Registry::SetStorage(NULL);
    storage_.reset();

    ConfigHandler::SetConfigFileName(original_config_filename_);
    FLAGS_min_sync_interval = original_min_sync_interval_;
  }

  void SyncStatusReset() {
    // Reset sync status assuming authorization succeeds.
    Singleton<SyncStatusManager>::get()->SetSyncGlobalStatus(
        commands::CloudSyncStatus::INSYNC);
    Singleton<SyncStatusManager>::get()->NewSyncStatusSession();
  }


 protected:
  // |sync_handler_| takes an ownership of |*syncer_|.
  GmockSyncer *syncer_;
  scoped_ptr<SyncHandler> sync_handler_;

 private:
  string original_config_filename_;
  string config_filename_;
  HTTPClientMock client_;
  scoped_ptr<mozc::storage::StorageInterface> storage_;
  int32 original_min_sync_interval_;
};

class ScopedClockMock {
 public:
  ScopedClockMock() : clock_mock_(Util::GetTime(), 0) {
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

namespace {

void SetUpHttpClientMockForAuth(const string &auth_token,
                                HTTPClientMock *client_mock) {
  DCHECK(client_mock);

  const OAuth2Client &oauth2_client = OAuth2Client::GetDefaultInstance();
  const OAuth2Server &oauth2_server = OAuth2Server::GetDefaultInstance();
  OAuth2Util oauth2_util(oauth2_client, oauth2_server);
  vector<pair<string, string> > params;
  params.push_back(make_pair("grant_type", "authorization_code"));
  params.push_back(make_pair("client_id", oauth2_client.client_id_));
  params.push_back(make_pair("client_secret", oauth2_client.client_secret_));
  params.push_back(make_pair("redirect_uri",
                             oauth2_util.redirect_uri_for_unittest()));
  params.push_back(make_pair("code", auth_token));
  params.push_back(make_pair("scope", oauth2_util.scope_for_unittest()));

  HTTPClientMock::Result expect_result;
  expect_result.expected_url = oauth2_util.request_token_uri_for_unittest();
  Util::AppendCGIParams(params, &expect_result.expected_request);
  expect_result.expected_result = "{\"access_token\":\"1/correct_token\","
      "\"token_type\":\"Bearer\"}";

  client_mock->set_result(expect_result);
}

}  // namespace

TEST_F(SyncHandlerTest, NotificationTest) {
  FLAGS_min_sync_interval = 0;

  // Implementation note:
  //    We intentionally do not assert any timing condition like
  //      "this operation should be finished within X seconds",
  //    as it is not naturally guaranteed on preemptive multitasking operation
  //    system, especially on highly virtualized test environment. Here we have
  //    a long enough timeout just to prevent this test from getting stuck.
  //    See b/6407046 for the background of the flakiness of this test.
  const int kTimeout = 30 * 1000;  // 30 sec.
  const int kSyncDuation = 1000;  // 1 sec.

  {
    syncer_->Reset();
    syncer_->SetOperationDuration(kSyncDuation);

    EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*syncer_, SyncInternal()).Times(1).WillOnce(Return(true));

    // Call three times to test IsRunning state.
    EXPECT_TRUE(sync_handler_->Sync());
    EXPECT_TRUE(sync_handler_->Sync());
    EXPECT_TRUE(sync_handler_->Sync());

    NamedEventListener listener("sync");
    ASSERT_TRUE(listener.IsAvailable());
    // should be signaled eventually.
    EXPECT_TRUE(listener.Wait(kTimeout));

    sync_handler_->Wait();

    Mock::VerifyAndClearExpectations(syncer_);
  }

  {
    syncer_->Reset();
    syncer_->SetOperationDuration(kSyncDuation);

    EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(false));
    EXPECT_CALL(*syncer_, SyncInternal()).Times(0);

    NamedEventListenerThread listener(kTimeout);
    listener.Start();
    Util::Sleep(200);

    EXPECT_FALSE(sync_handler_->Sync());
    // should be signaled eventually.
    EXPECT_TRUE(listener.GetResult());

    sync_handler_->Wait();

    Mock::VerifyAndClearExpectations(syncer_);
  }

  {
    syncer_->Reset();
    syncer_->SetOperationDuration(kSyncDuation);

    EXPECT_CALL(*syncer_, Start()).Times(0);
    EXPECT_CALL(*syncer_, SyncInternal()).Times(0);
    EXPECT_CALL(*syncer_, ClearInternal()).Times(1).WillOnce(Return(true));

    // Call three times to test IsRunning state.
    EXPECT_TRUE(sync_handler_->Clear());
    EXPECT_TRUE(sync_handler_->Clear());
    EXPECT_TRUE(sync_handler_->Clear());

    NamedEventListener listener("sync");
    ASSERT_TRUE(listener.IsAvailable());
    // should be signaled eventually.
    EXPECT_TRUE(listener.Wait(kTimeout));

    sync_handler_->Wait();

    Mock::VerifyAndClearExpectations(syncer_);
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
  MockClientFactory mock_client_factory;
  client::ClientFactory::SetClientFactory(&mock_client_factory);

  ON_CALL(*syncer_, SyncInternal()).WillByDefault(Return(true));

  {
    syncer_->Reset();

    EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*syncer_, SyncInternal()).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*syncer_, ClearInternal()).Times(0);

    g_is_reload_called = false;
    syncer_->SetReloadRequired(true);
    sync_handler_->Sync();
    sync_handler_->Wait();
    EXPECT_TRUE(g_is_reload_called);

    Mock::VerifyAndClearExpectations(syncer_);
  }

  {
    syncer_->Reset();

    EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*syncer_, SyncInternal()).Times(1).WillOnce(Return(false));
    EXPECT_CALL(*syncer_, ClearInternal()).Times(0);

    g_is_reload_called = false;
    syncer_->SetReloadRequired(true);
    sync_handler_->Sync();
    sync_handler_->Wait();
    EXPECT_FALSE(g_is_reload_called);

    Mock::VerifyAndClearExpectations(syncer_);
  }

  {
    syncer_->Reset();

    EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*syncer_, SyncInternal()).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*syncer_, ClearInternal()).Times(0);

    g_is_reload_called = false;
    syncer_->SetReloadRequired(false);
    sync_handler_->Sync();
    sync_handler_->Wait();
    EXPECT_FALSE(g_is_reload_called);

    Mock::VerifyAndClearExpectations(syncer_);
  }

  {
    syncer_->Reset();

    EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(false));

    g_is_reload_called = false;
    syncer_->SetReloadRequired(true);
    sync_handler_->Sync();
    sync_handler_->Wait();
    EXPECT_FALSE(g_is_reload_called);

    Mock::VerifyAndClearExpectations(syncer_);
  }

  {
    syncer_->Reset();

    ON_CALL(*syncer_, Start()).WillByDefault(Return(true));
    ON_CALL(*syncer_, SyncInternal()).WillByDefault(Return(true));

    g_is_reload_called = false;
    syncer_->SetReloadRequired(true);
    sync_handler_->Clear();
    sync_handler_->Wait();
    EXPECT_FALSE(g_is_reload_called);

    Mock::VerifyAndClearExpectations(syncer_);
  }
}

TEST_F(SyncHandlerTest, MinIntervalTest) {
  ScopedClockMock clock_mock;

  ON_CALL(*syncer_, Start()).WillByDefault(Return(true));

  // The resolution of timing calculation in SyncHandler is 1 second.
  // So |FLAGS_min_sync_interval| must be relatively larger than the
  // resolution to stabilize this test.
  FLAGS_min_sync_interval = 5;  // seconds

  {
    EXPECT_CALL(*syncer_, SyncInternal()).Times(1).WillOnce(Return(true));

    syncer_->Reset();

    NamedEventListener listener("sync");
    ASSERT_TRUE(listener.IsAvailable());
    EXPECT_TRUE(sync_handler_->Sync());
    ASSERT_TRUE(listener.Wait(1000))
        << "Initial Sync should be invoked immediately";
    sync_handler_->Wait();

    Mock::VerifyAndClearExpectations(syncer_);
  }

  // Advance the clock 2 seconds.
  clock_mock.get()->PutClockForward(2, 0);
  {
    EXPECT_CALL(*syncer_, SyncInternal()).Times(0);

    syncer_->Reset();

    NamedEventListener listener("sync");
    ASSERT_TRUE(listener.IsAvailable());
    EXPECT_TRUE(sync_handler_->Sync());
    ASSERT_FALSE(listener.Wait(500))
        << "Subsequent Sync call should wait for the next sync time "
        "window with minimum sync interval.";

    Mock::VerifyAndClearExpectations(syncer_);

    ASSERT_TRUE(listener.Wait(FLAGS_min_sync_interval * 1000))
        << "The second Sync call should be finished.";
    sync_handler_->Wait();
  }

  // Advance the clock 2 wait intervals.
  clock_mock.get()->PutClockForward(FLAGS_min_sync_interval * 2, 0);
  {
    EXPECT_CALL(*syncer_, SyncInternal()).Times(1).WillOnce(Return(true));

    syncer_->Reset();

    NamedEventListener listener("sync");
    ASSERT_TRUE(listener.IsAvailable());
    EXPECT_TRUE(sync_handler_->Sync());
    ASSERT_TRUE(listener.Wait(1000))
        << "With sufficient wait time, Sync should be invoked immediately.";
    sync_handler_->Wait();

    Mock::VerifyAndClearExpectations(syncer_);
  }

  // Advance the clock 1 second.
  clock_mock.get()->PutClockForward(1, 0);
  {
    EXPECT_CALL(*syncer_, ClearInternal()).Times(1).WillOnce(Return(true));

    syncer_->Reset();

    NamedEventListener listener("sync");
    ASSERT_TRUE(listener.IsAvailable());
    EXPECT_TRUE(sync_handler_->Clear());
    ASSERT_TRUE(listener.Wait(1000))
        << "Even within the minimum interval, Clear should be invoked "
        "immediately.";
    sync_handler_->Wait();

    Mock::VerifyAndClearExpectations(syncer_);
  }
}

TEST_F(SyncHandlerTest, ClearTest) {
  FLAGS_min_sync_interval = 0;

  ON_CALL(*syncer_, Start()).WillByDefault(Return(true));

  {
    syncer_->Reset();

    EXPECT_CALL(*syncer_, ClearInternal()).Times(1).WillOnce(Return(true));

    NamedEventListener listener("sync");
    ASSERT_TRUE(listener.IsAvailable());
    EXPECT_TRUE(sync_handler_->Clear());
    ASSERT_TRUE(listener.Wait(1000))
        << "Initial Clear should be invoked immediately.";
    sync_handler_->Wait();

    Mock::VerifyAndClearExpectations(syncer_);
  }

  {
    syncer_->Reset();

    EXPECT_CALL(*syncer_, ClearInternal()).Times(0);

    NamedEventListener listener("sync");
    ASSERT_TRUE(listener.IsAvailable());
    EXPECT_TRUE(sync_handler_->Clear());
    ASSERT_FALSE(listener.Wait(250))
        << "Subsequent Clear should not be simply ignored.";
    sync_handler_->Wait();

    Mock::VerifyAndClearExpectations(syncer_);
  }

  {
    syncer_->Reset();

    EXPECT_CALL(*syncer_, ClearInternal()).Times(0);

    NamedEventListener listener("sync");
    ASSERT_TRUE(listener.IsAvailable());
    EXPECT_TRUE(sync_handler_->Sync());
    ASSERT_TRUE(listener.Wait(1000))
        << "Sync after Clear should be invoked immediately because "
        "sync_handler_->Clear() removes auth token.";
    sync_handler_->Wait();

    Mock::VerifyAndClearExpectations(syncer_);
  }
}

TEST_F(SyncHandlerTest, AuthorizationFailedTest) {
  const char kCorrectAuthToken[] = "a_correct_token";
  const char kWrongAuthToken[] = "a_wrong_token";

  HTTPClientMock client_mock;
  SetUpHttpClientMockForAuth(kCorrectAuthToken, &client_mock);
  HTTPClient::SetHTTPClientHandler(&client_mock);

  // Authorization is expected to succeed.
  {
    commands::Input::AuthorizationInfo auth_info;
    auth_info.set_auth_code(kCorrectAuthToken);
    sync_handler_->SetAuthorization(auth_info);
    commands::CloudSyncStatus sync_status;
    sync_handler_->GetCloudSyncStatus(&sync_status);
    EXPECT_EQ(commands::CloudSyncStatus::INSYNC, sync_status.global_status());
  }

  // Authorization is expected to fail.
  {
    commands::Input::AuthorizationInfo auth_info;
    auth_info.set_auth_code(kWrongAuthToken);
    sync_handler_->SetAuthorization(auth_info);
    commands::CloudSyncStatus sync_status;
    sync_handler_->GetCloudSyncStatus(&sync_status);
    EXPECT_EQ(commands::CloudSyncStatus::NOSYNC,
              sync_status.global_status());
    EXPECT_EQ(1, sync_status.sync_errors_size());
    EXPECT_EQ(commands::CloudSyncStatus::AUTHORIZATION_FAIL,
              sync_status.sync_errors(0).error_code());
  }
}

namespace {

// We cannot use HTTPClientMock because it cannot set |output| while returning
// false, so here we make a simple mock for it.
// TODO(peria): Unify SimpleHttpClientMock into HTTPClientMock.
class SimpleHttpClientMock : public HTTPClientInterface {
 public:
  virtual bool Get(const string &url, const HTTPClient::Option &option,
                   string *output) const {
    return true;
  }
  virtual bool Head(const string &url, const HTTPClient::Option &option,
                    string *output) const {
    return true;
  }
  virtual bool Post(const string &url, const string &data,
                    const HTTPClient::Option &option,
                    string *output) const {
    *output = "{\"error\" : \"invalid_grant\"}";
    return false;
  }
};

}  // namespace

TEST_F(SyncHandlerTest, AuthorizationRevokeTest) {
  const char kAuthToken[] = "dummy_auth_token";

  HTTPClientMock client_mock;
  SetUpHttpClientMockForAuth(kAuthToken, &client_mock);
  HTTPClient::SetHTTPClientHandler(&client_mock);

  commands::Input::AuthorizationInfo auth_info;
  auth_info.set_auth_code(kAuthToken);
  sync_handler_->SetAuthorization(auth_info);

  SimpleHttpClientMock invalid_grant_mock;
  HTTPClient::SetHTTPClientHandler(&invalid_grant_mock);

  FLAGS_min_sync_interval = 0;
  MockClientFactory mock_client_factory;
  client::ClientFactory::SetClientFactory(&mock_client_factory);

  syncer_->Reset();

  ON_CALL(*syncer_, SyncInternal()).WillByDefault(Return(true));
  EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*syncer_, SyncInternal()).Times(0);
  EXPECT_CALL(*syncer_, ClearInternal()).Times(0);
  EXPECT_CALL(*syncer_, ClearLocal()).Times(1).WillOnce(Return(true));

  syncer_->SetReloadRequired(true);
  sync_handler_->Sync();
  sync_handler_->Wait();
}

}  // namespace sync
}  // namespace mozc

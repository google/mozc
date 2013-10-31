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
#include "base/scheduler.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"
#include "client/client.h"
#include "client/client_interface.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
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
using testing::DoAll;
using testing::Mock;
using testing::Return;
using testing::SetArgPointee;
using testing::_;

namespace sync {

class GmockSyncer : public SyncerInterface {
 public:
  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(ClearLocal, bool());
  MOCK_METHOD0(Clear, bool());
  MOCK_METHOD1(Sync, bool(bool *reload_required));
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
    Scheduler::RemoveAllJobs();

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
    Scheduler::RemoveAllJobs();
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

  ScopedClockMock clock_mock;

  {
    EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*syncer_, Sync(_)).Times(1).WillOnce(DoAll(
        SetArgPointee<0>(true), Return(true)));
    EXPECT_CALL(*syncer_, Clear()).Times(0);

    g_is_reload_called = false;
    // GetReloadRequiredTimestamp returns 0 for the first time.
    EXPECT_EQ(0, sync_handler_->GetReloadRequiredTimestamp());
    sync_handler_->Sync();
    sync_handler_->Wait();
    EXPECT_TRUE(g_is_reload_called);
    EXPECT_NE(0, sync_handler_->GetReloadRequiredTimestamp());

    Mock::VerifyAndClearExpectations(syncer_);
  }

  {
    EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*syncer_, Sync(_)).Times(1).WillOnce(DoAll(
        SetArgPointee<0>(true), Return(true)));
    EXPECT_CALL(*syncer_, Clear()).Times(0);

    g_is_reload_called = false;
    const uint64 reload_required_timestamp =
        sync_handler_->GetReloadRequiredTimestamp();
    clock_mock.get()->PutClockForward(123, 0);
    sync_handler_->Sync();
    sync_handler_->Wait();
    EXPECT_TRUE(g_is_reload_called);
    EXPECT_EQ(reload_required_timestamp + 123,
              sync_handler_->GetReloadRequiredTimestamp());

    Mock::VerifyAndClearExpectations(syncer_);
  }

  {
    EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*syncer_, Sync(_)).Times(1).WillOnce(DoAll(
        SetArgPointee<0>(true), Return(false)));
    EXPECT_CALL(*syncer_, Clear()).Times(0);

    g_is_reload_called = false;
    const uint64 reload_required_timestamp =
        sync_handler_->GetReloadRequiredTimestamp();
    clock_mock.get()->PutClockForward(234, 0);
    sync_handler_->Sync();
    sync_handler_->Wait();
    EXPECT_TRUE(g_is_reload_called);
    EXPECT_EQ(reload_required_timestamp + 234,
              sync_handler_->GetReloadRequiredTimestamp());

    Mock::VerifyAndClearExpectations(syncer_);
  }

  // Reload required timestamp should NOT be updated by following tests.
  clock_mock.get()->PutClockForward(100, 0);

  {
    EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*syncer_, Sync(_)).Times(1).WillOnce(DoAll(
        SetArgPointee<0>(false), Return(true)));
    EXPECT_CALL(*syncer_, Clear()).Times(0);

    g_is_reload_called = false;
    const uint64 reload_required_timestamp =
        sync_handler_->GetReloadRequiredTimestamp();
    sync_handler_->Sync();
    sync_handler_->Wait();
    EXPECT_FALSE(g_is_reload_called);
    EXPECT_EQ(reload_required_timestamp,
              sync_handler_->GetReloadRequiredTimestamp());

    Mock::VerifyAndClearExpectations(syncer_);
  }

  {
    EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(false));
    EXPECT_CALL(*syncer_, Sync(_)).Times(0);

    g_is_reload_called = false;
    const uint64 reload_required_timestamp =
        sync_handler_->GetReloadRequiredTimestamp();
    sync_handler_->Sync();
    sync_handler_->Wait();
    EXPECT_FALSE(g_is_reload_called);
    EXPECT_EQ(reload_required_timestamp,
              sync_handler_->GetReloadRequiredTimestamp());

    Mock::VerifyAndClearExpectations(syncer_);
  }
}

TEST_F(SyncHandlerTest, MinIntervalTest) {
  // We cannot use clock mock since SyncHandler::Run() uses Util::Sleep() to
  // ensure the min interval.
  //
  // The resolution of timing calculation in SyncHandler is 1 second.
  // So |FLAGS_min_sync_interval| must be relatively larger than the
  // resolution to stabilize this test.
  FLAGS_min_sync_interval = 5;  // seconds

  // Start sync and wait.
  EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*syncer_, Sync(_)).Times(1).WillOnce(Return(true));
  EXPECT_TRUE(sync_handler_->Sync());
  Util::Sleep(1000);
  Mock::VerifyAndClearExpectations(syncer_);
  sync_handler_->Wait();

  // Start sync. Syncer::Sync() is not called soon since it is blocked.
  EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*syncer_, Sync(_)).Times(0);
  EXPECT_TRUE(sync_handler_->Sync());
  Util::Sleep(1000);
  Mock::VerifyAndClearExpectations(syncer_);

  // Wait.
  EXPECT_CALL(*syncer_, Sync(_)).Times(1).WillOnce(Return(true));
  sync_handler_->Wait();
  Mock::VerifyAndClearExpectations(syncer_);
}

TEST_F(SyncHandlerTest, AuthorizationFailedTest) {
  const char kCorrectAuthToken[] = "a_correct_token";
  const char kWrongAuthToken[] = "a_wrong_token";

  HTTPClientMock client_mock;
  SetUpHttpClientMockForAuth(kCorrectAuthToken, &client_mock);
  HTTPClient::SetHTTPClientHandler(&client_mock);

  // Authorization is expected to succeed.
  {
    EXPECT_CALL(*syncer_, ClearLocal()).Times(0);
    commands::Input::AuthorizationInfo auth_info;
    auth_info.set_auth_code(kCorrectAuthToken);
    sync_handler_->SetAuthorization(auth_info);
    commands::CloudSyncStatus sync_status;
    sync_handler_->GetCloudSyncStatus(&sync_status);
    EXPECT_EQ(commands::CloudSyncStatus::INSYNC, sync_status.global_status());
  }

  // Authorization is expected to fail with invalid auth token.
  {
    EXPECT_CALL(*syncer_, ClearLocal()).Times(1).WillOnce(Return(true));
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

  // Authorization is expected to fail without auth token.
  {
    EXPECT_CALL(*syncer_, ClearLocal()).Times(1).WillOnce(Return(true));
    const commands::Input::AuthorizationInfo auth_info;
    sync_handler_->SetAuthorization(auth_info);
    commands::CloudSyncStatus sync_status;
    sync_handler_->GetCloudSyncStatus(&sync_status);
    EXPECT_EQ(commands::CloudSyncStatus::NOSYNC,
              sync_status.global_status());
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

  EXPECT_CALL(*syncer_, Start()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*syncer_, Sync(_)).Times(0);
  EXPECT_CALL(*syncer_, Clear()).Times(0);
  EXPECT_CALL(*syncer_, ClearLocal()).Times(1).WillOnce(Return(true));

  Scheduler::AddJob(sync_handler_->GetSchedulerJobSetting());

  sync_handler_->Sync();
  sync_handler_->Wait();

  const string &sync_job_name = sync_handler_->GetSchedulerJobSetting().name();
  // Sync scheduler job should NOT be removed.
  EXPECT_TRUE(Scheduler::RemoveJob(sync_job_name));
}

}  // namespace sync
}  // namespace mozc

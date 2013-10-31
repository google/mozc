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

#include "sync/syncer.h"

#include <vector>

#include "base/logging.h"
#include "base/port.h"
#include "base/protobuf/descriptor.h"
#include "base/scoped_ptr.h"
#include "base/system_util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "storage/memory_storage.h"
#include "storage/registry.h"
#include "storage/storage_interface.h"
#include "sync/adapter_interface.h"
#include "sync/inprocess_service.h"
#include "sync/service_interface.h"
#include "sync/sync.pb.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
using mozc::config::Config;
using mozc::config::ConfigHandler;
using mozc::config::SyncConfig;
using mozc::protobuf::Descriptor;

namespace sync {

class SyncerTest : public testing::Test {
 public:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    ConfigHandler::SetConfigFileName("memory://config");

    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    SyncConfig *sync_config = config.mutable_sync_config();
    sync_config->set_use_config_sync(true);
    sync_config->set_use_user_dictionary_sync(true);
    sync_config->set_use_user_history_sync(true);
    sync_config->set_use_contact_list_sync(true);
    sync_config->set_use_learning_preference_sync(true);
    ConfigHandler::SetConfig(config);

    storage_.reset(mozc::storage::MemoryStorage::New());
    mozc::storage::Registry::SetStorage(storage_.get());
  }

  virtual void TearDown() {
    mozc::storage::Registry::SetStorage(NULL);

    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    ConfigHandler::SetConfig(config);
  }

 private:
  scoped_ptr<mozc::storage::StorageInterface> storage_;
};

class TestableSyncer : public Syncer {
 public:
  explicit TestableSyncer(ServiceInterface *service) : Syncer(service) {}
  virtual ~TestableSyncer() {}
  using Syncer::Download;
  using Syncer::Upload;
  using Syncer::GetLastDownloadTimestamp;
  using Syncer::SetLastDownloadTimestamp;
};

class MockService : public ServiceInterface {
 public:
  MockService()
      : upload_result_(false), download_result_(false), clear_result_(false) {}

  virtual bool Upload(ime_sync::UploadRequest *request,
                      ime_sync::UploadResponse *response) {
    DCHECK(request);
    DCHECK(response);
    upload_request_.CopyFrom(*request);
    response->CopyFrom(upload_response_);
    return upload_result_;
  }

  void SetUpload(const ime_sync::UploadResponse &response,
                 bool result) {
    upload_response_.CopyFrom(response);
    upload_result_ = result;
  }

  const ime_sync::UploadRequest &UploadRequest() const {
    return upload_request_;
  }

  virtual bool Download(ime_sync::DownloadRequest *request,
                        ime_sync::DownloadResponse *response) {
    DCHECK(request);
    DCHECK(response);
    download_request_.CopyFrom(*request);
    response->CopyFrom(download_response_);
    return download_result_;
  }

  void SetDownload(const ime_sync::DownloadResponse &response,
                   bool result) {
    download_response_.CopyFrom(response);
    download_result_ = result;
  }

  const ime_sync::DownloadRequest &DownloadRequest() const {
    return download_request_;
  }

  virtual bool Clear(ime_sync::ClearRequest *request,
                     ime_sync::ClearResponse *response) {
    DCHECK(request);
    DCHECK(response);
    clear_request_.CopyFrom(*request);
    response->CopyFrom(clear_response_);
    return clear_result_;
  }

  void SetClear(const ime_sync::ClearResponse &response,
                bool result) {
    clear_response_.CopyFrom(response);
    clear_result_ = result;
  }

  const ime_sync::ClearRequest &ClearRequest() const {
    return clear_request_;
  }

 private:
  ime_sync::UploadResponse   upload_response_;
  ime_sync::UploadRequest    upload_request_;
  ime_sync::DownloadResponse download_response_;
  ime_sync::DownloadRequest  download_request_;
  ime_sync::ClearResponse    clear_response_;
  ime_sync::ClearRequest     clear_request_;
  bool upload_result_;
  bool download_result_;
  bool clear_result_;

  DISALLOW_COPY_AND_ASSIGN(MockService);
};

class MockAdapter : public AdapterInterface {
 public:
  MockAdapter()
      : download_result_(false), upload_result_(false), uploaded_(false),
        mark_uploaded_called_(false), component_id_(ime_sync::MOZC_SETTING),
        last_download_timestamp_(0) {}

  virtual ~MockAdapter() {}

  virtual bool SetDownloadedItems(const ime_sync::SyncItems &items) {
    return download_result_;
  }

  void SetDownloadedItemsResult(bool result) {
    download_result_ = result;
  }

  virtual bool GetItemsToUpload(ime_sync::SyncItems *items) {
    DCHECK(items);
    // To support multiple adapters, we use MergeFrom() instead of CopyFrom().
    items->MergeFrom(upload_items_);
    return upload_result_;
  }

  void SetItemsToUploadResult(const ime_sync::SyncItems &items,
                              bool result) {
    upload_items_.CopyFrom(items);
    upload_result_ = result;
  }

  virtual bool MarkUploaded(const ime_sync::SyncItem& item, bool uploaded) {
    uploaded_ = uploaded;
    mark_uploaded_called_ = true;
    return true;
  }

  virtual bool Clear() {
    uploaded_ = false;
    return true;
  }

  bool GetUploaded() const {
    return uploaded_;
  }

  void ResetMarkUploadedCalled() {
    mark_uploaded_called_ = false;
  }

  bool IsMarkUploadedCalled() const {
    return mark_uploaded_called_;
  }

  virtual ime_sync::Component component_id() const {
    return component_id_;
  }

  void set_component_id(const ime_sync::Component id) {
    component_id_ = id;
  }

  virtual uint64 GetLastDownloadTimestamp() const {
    return last_download_timestamp_;
  }

  virtual bool SetLastDownloadTimestamp(uint64 timestamp) {
    last_download_timestamp_ = timestamp;
    return true;
  }

 private:
  ime_sync::SyncItems upload_items_;
  bool download_result_;
  bool upload_result_;
  bool uploaded_;
  bool mark_uploaded_called_;
  ime_sync::Component component_id_;
  uint64 last_download_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(MockAdapter);
};

namespace {

const void InitializeSyncItem(ime_sync::Component component_id,
                              ime_sync::SyncItem *item) {
  item->set_component(component_id);

  switch (component_id) {
    case ime_sync::MOZC_SETTING:
      item->mutable_key()->MutableExtension(sync::ConfigKey::ext);
      item->mutable_value()->MutableExtension(sync::ConfigValue::ext);
      break;
    case ime_sync::MOZC_USER_DICTIONARY:
      item->mutable_key()->MutableExtension(sync::UserDictionaryKey::ext);
      item->mutable_value()->MutableExtension(sync::UserDictionaryValue::ext);
      break;
    case ime_sync::MOZC_USER_HISTORY_PREDICTION:
      item->mutable_key()->MutableExtension(sync::UserHistoryKey::ext);
      item->mutable_value()->MutableExtension(sync::UserHistoryValue::ext);
      break;
    case ime_sync::MOZC_LEARNING_PREFERENCE:
      item->mutable_key()->MutableExtension(sync::LearningPreferenceKey::ext);
      item->mutable_value()->MutableExtension(
          sync::LearningPreferenceValue::ext);
      break;
    default:
      FAIL();
  }

  ASSERT_TRUE(item->IsInitialized());
}

void SetUpMockAdapter(ime_sync::Component component_id, MockAdapter *adapter) {
  CHECK(adapter);
  adapter->set_component_id(component_id);
  ime_sync::SyncItems sync_items;
  InitializeSyncItem(component_id, sync_items.Add());
  adapter->SetItemsToUploadResult(sync_items, true);
  adapter->SetDownloadedItemsResult(true);
}

void SetUpMockService(const vector<ime_sync::Component> &component_ids,
                      uint64 download_timestamp, MockService *service) {
  CHECK(service);
  ime_sync::DownloadResponse download_response;
  download_response.set_error(ime_sync::SYNC_OK);
  download_response.set_download_timestamp(download_timestamp);
  for (size_t i = 0; i < component_ids.size(); ++i) {
    InitializeSyncItem(component_ids[i], download_response.add_items());
  }
  service->SetDownload(download_response, true);

  ime_sync::UploadResponse upload_response;
  upload_response.set_error(ime_sync::SYNC_OK);
  ASSERT_TRUE(upload_response.IsInitialized());
  service->SetUpload(upload_response, true);

  ime_sync::ClearResponse clear_response;
  clear_response.set_error(ime_sync::SYNC_OK);
  ASSERT_TRUE(clear_response.IsInitialized());
  service->SetClear(clear_response, true);
}

}  // namespace

TEST_F(SyncerTest, Timestamp) {
  InprocessService service;
  TestableSyncer syncer(&service);

  // GetLastDownloadTimestamp() should be 0 since there is no adapter.
  syncer.SetLastDownloadTimestamp(1000);
  EXPECT_EQ(0, syncer.GetLastDownloadTimestamp());

  MockAdapter config_adapter;
  config_adapter.set_component_id(ime_sync::MOZC_SETTING);
  syncer.RegisterAdapter(&config_adapter);

  syncer.SetLastDownloadTimestamp(1000);
  EXPECT_EQ(1000, syncer.GetLastDownloadTimestamp());
  EXPECT_EQ(1000, config_adapter.GetLastDownloadTimestamp());

  syncer.SetLastDownloadTimestamp(0);
  EXPECT_EQ(0, syncer.GetLastDownloadTimestamp());

  syncer.SetLastDownloadTimestamp(123);
  EXPECT_EQ(123, syncer.GetLastDownloadTimestamp());

  MockAdapter user_dictionary_adapter;
  user_dictionary_adapter.set_component_id(ime_sync::MOZC_USER_DICTIONARY);
  syncer.RegisterAdapter(&user_dictionary_adapter);

  // GetLastDownloadTimestamp() returns minimum timestamp.
  user_dictionary_adapter.SetLastDownloadTimestamp(50);
  EXPECT_EQ(50, syncer.GetLastDownloadTimestamp());
  user_dictionary_adapter.SetLastDownloadTimestamp(234);
  EXPECT_EQ(123, syncer.GetLastDownloadTimestamp());

  syncer.SetLastDownloadTimestamp(500);
  EXPECT_EQ(500, syncer.GetLastDownloadTimestamp());
  EXPECT_EQ(500, config_adapter.GetLastDownloadTimestamp());
  EXPECT_EQ(500, user_dictionary_adapter.GetLastDownloadTimestamp());
}

TEST_F(SyncerTest, Clear) {
  MockService service;
  TestableSyncer syncer(&service);

  MockAdapter adapter;
  syncer.RegisterAdapter(&adapter);

  syncer.SetLastDownloadTimestamp(1000);

  ime_sync::ClearResponse clear_response;
  service.SetClear(clear_response, false);
  EXPECT_FALSE(syncer.Clear());
  EXPECT_EQ(1000, syncer.GetLastDownloadTimestamp());

  clear_response.Clear();
  service.SetClear(clear_response, true);
  EXPECT_FALSE(syncer.Clear());
  EXPECT_EQ(1000, syncer.GetLastDownloadTimestamp());

  clear_response.set_error(ime_sync::SYNC_SERVER_ERROR);
  service.SetClear(clear_response, true);
  EXPECT_FALSE(syncer.Clear());
  EXPECT_EQ(1000, syncer.GetLastDownloadTimestamp());

  clear_response.set_error(ime_sync::SYNC_OK);
  service.SetClear(clear_response, true);
  EXPECT_TRUE(syncer.Clear());
  EXPECT_EQ(1, service.ClearRequest().version());
  EXPECT_EQ(0, syncer.GetLastDownloadTimestamp());
}

TEST_F(SyncerTest, Download) {
  MockService service;
  TestableSyncer syncer(&service);
  bool reload_required = false;
  uint64 download_timestamp = 0;
  ime_sync::DownloadResponse download_response;

  MockAdapter adapter;
  syncer.RegisterAdapter(&adapter);

  adapter.SetDownloadedItemsResult(true);
  service.SetDownload(download_response, false);
  reload_required = true;
  EXPECT_FALSE(syncer.Download(&download_timestamp, &reload_required));
  EXPECT_FALSE(reload_required);

  download_response.Clear();
  download_response.set_error(ime_sync::SYNC_SERVER_ERROR);
  download_response.set_download_timestamp(1111);
  service.SetDownload(download_response, true);
  reload_required = true;
  EXPECT_FALSE(syncer.Download(&download_timestamp, &reload_required));
  EXPECT_FALSE(reload_required);
  EXPECT_NE(1111, download_timestamp);

  download_response.set_error(ime_sync::SYNC_OK);
  download_response.set_download_timestamp(123);
  adapter.SetDownloadedItemsResult(false);
  service.SetDownload(download_response, true);
  reload_required = true;
  EXPECT_FALSE(syncer.Download(&download_timestamp, &reload_required));
  EXPECT_FALSE(reload_required);
  EXPECT_NE(123, syncer.GetLastDownloadTimestamp());

  syncer.SetLastDownloadTimestamp(123);
  download_response.set_error(ime_sync::SYNC_OK);
  download_response.set_download_timestamp(234);
  service.SetDownload(download_response, true);
  adapter.SetDownloadedItemsResult(true);
  reload_required = true;
  EXPECT_TRUE(syncer.Download(&download_timestamp, &reload_required));
  EXPECT_FALSE(reload_required);
  EXPECT_EQ(1, service.DownloadRequest().version());
  EXPECT_EQ(123, service.DownloadRequest().last_download_timestamp());
  EXPECT_EQ(234, download_timestamp);

  download_response.Clear();
  download_response.set_error(ime_sync::SYNC_OK);
  download_response.set_download_timestamp(345);
  InitializeSyncItem(ime_sync::MOZC_SETTING,
                     download_response.mutable_items()->Add());
  service.SetDownload(download_response, true);
  adapter.SetDownloadedItemsResult(true);
  reload_required = false;
  EXPECT_TRUE(syncer.Download(&download_timestamp, &reload_required));
  EXPECT_TRUE(reload_required);
  EXPECT_EQ(1, service.DownloadRequest().version());
  EXPECT_EQ(345, download_timestamp);
}

TEST_F(SyncerTest, Upload) {
  MockService service;
  TestableSyncer syncer(&service);

  MockAdapter adapter;
  syncer.RegisterAdapter(&adapter);

  ime_sync::UploadResponse upload_response;
  service.SetUpload(upload_response, true);

  ime_sync::SyncItems empty_items;
  adapter.SetItemsToUploadResult(empty_items, true);
  adapter.ResetMarkUploadedCalled();
  EXPECT_TRUE(syncer.Upload());
  EXPECT_FALSE(adapter.IsMarkUploadedCalled());

  ime_sync::SyncItems non_empty_items;
  InitializeSyncItem(ime_sync::MOZC_SETTING, non_empty_items.Add());

  adapter.SetItemsToUploadResult(non_empty_items, false);
  adapter.ResetMarkUploadedCalled();
  service.SetUpload(upload_response, true);
  upload_response.set_error(ime_sync::SYNC_OK);
  EXPECT_FALSE(syncer.Upload());
  EXPECT_FALSE(adapter.GetUploaded());
  EXPECT_TRUE(adapter.IsMarkUploadedCalled());

  adapter.SetItemsToUploadResult(non_empty_items, true);
  adapter.ResetMarkUploadedCalled();
  upload_response.set_error(ime_sync::SYNC_OK);
  service.SetUpload(upload_response, false);
  EXPECT_FALSE(syncer.Upload());
  EXPECT_FALSE(adapter.GetUploaded());
  EXPECT_TRUE(adapter.IsMarkUploadedCalled());

  adapter.SetItemsToUploadResult(non_empty_items, true);
  adapter.ResetMarkUploadedCalled();
  upload_response.set_error(ime_sync::SYNC_SERVER_ERROR);
  service.SetUpload(upload_response, true);
  EXPECT_FALSE(syncer.Upload());
  EXPECT_FALSE(adapter.GetUploaded());
  EXPECT_TRUE(adapter.IsMarkUploadedCalled());

  adapter.SetItemsToUploadResult(non_empty_items, true);
  adapter.ResetMarkUploadedCalled();
  upload_response.set_error(ime_sync::SYNC_OK);
  service.SetUpload(upload_response, true);
  EXPECT_TRUE(syncer.Upload());
  EXPECT_TRUE(adapter.GetUploaded());
  EXPECT_TRUE(adapter.IsMarkUploadedCalled());
  EXPECT_EQ(1, service.UploadRequest().version());
}

TEST_F(SyncerTest, CheckConfig) {
  MockService service;
  TestableSyncer syncer(&service);

  vector<ime_sync::Component> component_ids;
  component_ids.push_back(ime_sync::MOZC_SETTING);
  component_ids.push_back(ime_sync::MOZC_USER_DICTIONARY);
  component_ids.push_back(ime_sync::MOZC_USER_HISTORY_PREDICTION);
  component_ids.push_back(ime_sync::MOZC_LEARNING_PREFERENCE);
  const int kComponentNum = 4;
  CHECK_EQ(kComponentNum, component_ids.size());

  // Set up environment.
  MockAdapter adapters[kComponentNum];
  for (int i = 0; i < kComponentNum; ++i) {
    MockAdapter *adapter = &adapters[i];
    SetUpMockAdapter(component_ids[i], adapter);
    syncer.RegisterAdapter(adapter);
  }
  SetUpMockService(component_ids, 100, &service);
  syncer.SetLastDownloadTimestamp(1);

  // Set config to sync.
  {
    Config config = ConfigHandler::GetConfig();
    SyncConfig *sync_config = config.mutable_sync_config();
    sync_config->set_use_config_sync(true);
    sync_config->set_use_user_dictionary_sync(true);
    sync_config->set_use_user_history_sync(true);
    sync_config->set_use_contact_list_sync(true);
    sync_config->set_use_learning_preference_sync(true);
    ConfigHandler::SetConfig(config);
  }

  bool reload_required = false;
  uint64 download_timestamp = 0;
  EXPECT_TRUE(syncer.Download(&download_timestamp, &reload_required));
  EXPECT_TRUE(reload_required);
  ime_sync::DownloadRequest download_request = service.DownloadRequest();
  EXPECT_EQ(kComponentNum, download_request.components_size());
  EXPECT_TRUE(syncer.Upload());
  for (int i = 0; i < kComponentNum; ++i) {
    EXPECT_TRUE(adapters[i].GetUploaded());
    EXPECT_EQ(component_ids[i], download_request.components(i));
  }
  EXPECT_TRUE(syncer.Clear());

  // Set config NOT to sync.
  {
    Config config = ConfigHandler::GetConfig();
    SyncConfig *sync_config = config.mutable_sync_config();
    sync_config->set_use_config_sync(false);
    sync_config->set_use_user_dictionary_sync(false);
    sync_config->set_use_user_history_sync(false);
    sync_config->set_use_contact_list_sync(false);
    sync_config->set_use_learning_preference_sync(false);
    ConfigHandler::SetConfig(config);
  }

  reload_required = false;
  EXPECT_TRUE(syncer.Download(&download_timestamp, &reload_required));
  EXPECT_TRUE(reload_required);
  EXPECT_TRUE(syncer.Upload());
  download_request = service.DownloadRequest();
  for (int i = 0; i < kComponentNum; ++i) {
    EXPECT_FALSE(adapters[i].GetUploaded());
  }
  EXPECT_EQ(0, download_request.components_size());
  EXPECT_TRUE(syncer.Clear());
}

TEST_F(SyncerTest, EnableAndDisableAdapterPartially_b9270307) {
  MockService service;
  TestableSyncer syncer(&service);

  vector<ime_sync::Component> component_ids;
  component_ids.push_back(ime_sync::MOZC_SETTING);
  component_ids.push_back(ime_sync::MOZC_USER_DICTIONARY);
  const int kComponentNum = 2;
  CHECK_EQ(kComponentNum, component_ids.size());

  // Set up environment.
  MockAdapter adapters[kComponentNum];
  for (int i = 0; i < kComponentNum; ++i) {
    MockAdapter *adapter = &adapters[i];
    SetUpMockAdapter(component_ids[i], adapter);
    syncer.RegisterAdapter(adapter);
  }
  SetUpMockService(component_ids, 100, &service);
  syncer.SetLastDownloadTimestamp(1);

  // Set config to sync config and user dictionary only.
  Config config = ConfigHandler::GetConfig();
  SyncConfig *sync_config = config.mutable_sync_config();
  sync_config->set_use_config_sync(true);
  sync_config->set_use_user_dictionary_sync(true);
  ConfigHandler::SetConfig(config);

  bool reload_required = false;
  EXPECT_TRUE(syncer.Sync(&reload_required));
  EXPECT_TRUE(reload_required);
  EXPECT_EQ(100, syncer.GetLastDownloadTimestamp());

  // Set config to disable user dictionary sync. Config sync is enabled yet.
  sync_config->set_use_user_dictionary_sync(false);
  ConfigHandler::SetConfig(config);

  SetUpMockService(component_ids, 200, &service);
  reload_required = false;
  EXPECT_TRUE(syncer.Sync(&reload_required));
  EXPECT_TRUE(reload_required);
  EXPECT_EQ(200, syncer.GetLastDownloadTimestamp());

  // Set config to enable config and user dictionary sync.
  // LastDownloadTimestamp() should return 100.
  sync_config->set_use_config_sync(true);
  sync_config->set_use_user_dictionary_sync(true);
  ConfigHandler::SetConfig(config);
  EXPECT_EQ(100, syncer.GetLastDownloadTimestamp());
}

TEST_F(SyncerTest, DontUpdateLastDownloadTimestampIfDownloadOrUploadFail) {
  MockService service;
  TestableSyncer syncer(&service);

  vector<ime_sync::Component> component_ids(1, ime_sync::MOZC_SETTING);
  MockAdapter adapter;
  SetUpMockAdapter(ime_sync::MOZC_SETTING, &adapter);
  syncer.RegisterAdapter(&adapter);
  syncer.SetLastDownloadTimestamp(42);

  SetUpMockService(component_ids, 100, &service);
  ime_sync::DownloadResponse download_response;
  download_response.set_error(ime_sync::SYNC_INVALID_AUTH);
  download_response.set_download_timestamp(100);
  service.SetDownload(download_response, false);

  bool reload_required = false;
  EXPECT_FALSE(syncer.Sync(&reload_required));
  EXPECT_FALSE(reload_required);
  EXPECT_EQ(42, syncer.GetLastDownloadTimestamp());

  SetUpMockService(component_ids, 100, &service);
  ime_sync::UploadResponse upload_response;
  upload_response.set_error(ime_sync::SYNC_SERVER_ERROR);
  service.SetUpload(upload_response, false);

  reload_required = false;
  EXPECT_FALSE(syncer.Sync(&reload_required));
  // Reload required since download is succeeds.
  EXPECT_TRUE(reload_required);
  EXPECT_EQ(42, syncer.GetLastDownloadTimestamp());
}

}  // namespace sync
}  // namespace mozc

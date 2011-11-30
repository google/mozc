// Copyright 2010-2011, Google Inc.
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
#include "config/config_handler.h"
#include "sync/adapter_interface.h"
#include "sync/inprocess_service.h"
#include "sync/sync.pb.h"
#include "sync/syncer.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
using config::Config;
using config::ConfigHandler;
using config::SyncConfig;

namespace sync {

class SyncerTest : public testing::Test {
 public:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
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

  virtual void TearDown() {}
};

class MockService : public ServiceInterface {
 public:
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
};

class MockAdapter : public AdapterInterface {
 public:
  MockAdapter() : component_id_(ime_sync::MOZC_SETTING) {}
  virtual ~MockAdapter() {}

  bool SetDownloadedItems(const ime_sync::SyncItems &items) {
    return download_result_;
  }

  void SetDownloadedItemsResult(bool result) {
    download_result_ = result;
  }

  bool GetItemsToUpload(ime_sync::SyncItems *items) {
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

  bool MarkUploaded(const ime_sync::SyncItem& item, bool uploaded) {
    uploaded_ = uploaded;
    mark_uploaded_called_ = true;
    return true;
  }

  bool Clear() {
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

 private:
  ime_sync::SyncItems upload_items_;
  bool download_result_;
  bool upload_result_;
  bool uploaded_;
  bool mark_uploaded_called_;
  ime_sync::Component component_id_;
};

TEST_F(SyncerTest, Timestamp) {
  InprocessService service;
  Syncer syncer(&service);

  syncer.SetLastDownloadTimestamp(1000);
  EXPECT_EQ(1000, syncer.GetLastDownloadTimestamp());

  syncer.SetLastDownloadTimestamp(0);
  EXPECT_EQ(0, syncer.GetLastDownloadTimestamp());

  syncer.SetLastDownloadTimestamp(123);
  EXPECT_EQ(123, syncer.GetLastDownloadTimestamp());
}

TEST_F(SyncerTest, Clear) {
  MockService service;
  Syncer syncer(&service);

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
  Syncer syncer(&service);
  bool reload_required = false;

  MockAdapter adapter;
  syncer.RegisterAdapter(&adapter);

  syncer.SetLastDownloadTimestamp(1000);

  adapter.SetDownloadedItemsResult(true);
  ime_sync::DownloadResponse download_response;
  service.SetDownload(download_response, false);
  EXPECT_FALSE(syncer.Download(&reload_required));
  EXPECT_EQ(1000, syncer.GetLastDownloadTimestamp());

  download_response.Clear();
  service.SetDownload(download_response, true);
  EXPECT_FALSE(syncer.Download(&reload_required));
  EXPECT_EQ(1000, syncer.GetLastDownloadTimestamp());

  download_response.set_error(ime_sync::SYNC_SERVER_ERROR);
  service.SetDownload(download_response, true);
  EXPECT_FALSE(syncer.Download(&reload_required));

  download_response.set_error(ime_sync::SYNC_OK);
  download_response.set_download_timestamp(123);
  adapter.SetDownloadedItemsResult(false);
  service.SetDownload(download_response, true);
  EXPECT_FALSE(syncer.Download(&reload_required));
  EXPECT_EQ(1000, syncer.GetLastDownloadTimestamp());

  download_response.set_error(ime_sync::SYNC_OK);
  download_response.set_download_timestamp(123);
  service.SetDownload(download_response, true);
  adapter.SetDownloadedItemsResult(true);
  EXPECT_TRUE(syncer.Download(&reload_required));
  EXPECT_EQ(1, service.DownloadRequest().version());
  EXPECT_EQ(1000, service.DownloadRequest().last_download_timestamp());
  EXPECT_EQ(123, syncer.GetLastDownloadTimestamp());
  EXPECT_FALSE(reload_required);

  download_response.Clear();
  download_response.set_error(ime_sync::SYNC_OK);
  download_response.set_download_timestamp(123);
  ime_sync::SyncItem *item = download_response.mutable_items()->Add();
  item->set_component(ime_sync::MOZC_SETTING);
  sync::ConfigKey *key =
      item->mutable_key()->MutableExtension(sync::ConfigKey::ext);
  sync::ConfigValue *value =
      item->mutable_value()->MutableExtension(sync::ConfigValue::ext);
  CHECK(key);
  CHECK(value);
  value->mutable_config()->CopyFrom(config::ConfigHandler::GetConfig());
  EXPECT_TRUE(download_response.IsInitialized());
  service.SetDownload(download_response, true);
  adapter.SetDownloadedItemsResult(true);
  EXPECT_TRUE(syncer.Download(&reload_required));
  EXPECT_EQ(1, service.DownloadRequest().version());
  EXPECT_TRUE(reload_required);
}

TEST_F(SyncerTest, Upload) {
  MockService service;
  Syncer syncer(&service);

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
  non_empty_items.Add()->set_component(ime_sync::MOZC_SETTING);

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
  Syncer syncer(&service);

  vector<ime_sync::Component> component_ids;
  component_ids.push_back(ime_sync::MOZC_SETTING);
  component_ids.push_back(ime_sync::MOZC_USER_DICTIONARY);
  component_ids.push_back(ime_sync::MOZC_USER_HISTORY_PREDICTION);
  component_ids.push_back(ime_sync::MOZC_LEARNING_PREFERENCE);
  const int kComponentNum = 4;
  CHECK(kComponentNum == component_ids.size());

  // Set up environment.
  MockAdapter adapters[kComponentNum];
  for (int i = 0; i < kComponentNum; ++i) {
    MockAdapter &adapter = adapters[i];
    adapter.set_component_id(component_ids[i]);
    ime_sync::SyncItems sync_items;
    sync_items.Add()->set_component(component_ids[i]);
    adapter.SetItemsToUploadResult(sync_items, true);
    adapter.SetDownloadedItemsResult(true);
    adapter.SetDownloadedItems(sync_items);
    syncer.RegisterAdapter(&adapter);
  }
  ime_sync::UploadResponse upload_response;
  upload_response.set_error(ime_sync::SYNC_OK);
  service.SetUpload(upload_response, true);
  ime_sync::DownloadResponse download_response;
  download_response.set_error(ime_sync::SYNC_OK);
  service.SetDownload(download_response, true);
  syncer.SetLastDownloadTimestamp(1);
  ime_sync::ClearResponse clear_response;
  clear_response.set_error(ime_sync::SYNC_OK);
  service.SetClear(clear_response, true);

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
  EXPECT_TRUE(syncer.Upload());
  ime_sync::DownloadRequest download_request;
  EXPECT_TRUE(syncer.Download(&reload_required));
  download_request = service.DownloadRequest();
  EXPECT_EQ(kComponentNum, download_request.components_size());
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

  EXPECT_TRUE(syncer.Upload());
  EXPECT_TRUE(syncer.Download(&reload_required));
  download_request = service.DownloadRequest();
  for (int i = 0; i < kComponentNum; ++i) {
    EXPECT_FALSE(adapters[i].GetUploaded());
  }
  EXPECT_EQ(0, download_request.components_size());
  EXPECT_TRUE(syncer.Clear());
}

}  // sync
}  // mozc

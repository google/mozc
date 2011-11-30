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
#include "sync/sync.pb.h"
#include "sync/inprocess_service.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace sync {

TEST(InprocessServiceTest, VersionTest) {
  InprocessService service;
  ime_sync::ClearRequest request;
  ime_sync::ClearResponse response;
  request.set_version(0);
  EXPECT_TRUE(service.Clear(&request, &response));
  EXPECT_NE(response.error(), ime_sync::SYNC_OK);

  request.set_version(1);
  EXPECT_TRUE(service.Clear(&request, &response));
  EXPECT_EQ(response.error(), ime_sync::SYNC_OK);

  request.set_version(2);
  EXPECT_TRUE(service.Clear(&request, &response));
  EXPECT_NE(response.error(), ime_sync::SYNC_OK);
}

TEST(InprocessServiceTest, ClearTest) {
  InprocessService service;
  EXPECT_TRUE(service.IsEmpty());

  {
    ime_sync::UploadRequest request;
    ime_sync::UploadResponse response;
    request.set_version(1);

    ime_sync::SyncItem *item = request.mutable_items()->Add();
    CHECK(item);

    item->set_component(ime_sync::LANGUAGE_MODEL);

    sync::TestKey *key =
        item->mutable_key()->MutableExtension(
            sync::TestKey::ext);
    key->set_key("test_key");
    sync::TestValue *value =
        item->mutable_value()->MutableExtension(
            sync::TestValue::ext);
    value->set_value("test_value");

    EXPECT_TRUE(service.Upload(&request, &response));
    EXPECT_FALSE(service.IsEmpty());
  }

  {
    ime_sync::ClearRequest request;
    ime_sync::ClearResponse response;
    request.set_version(1);
    EXPECT_TRUE(service.Clear(&request, &response));
    EXPECT_EQ(response.error(), ime_sync::SYNC_OK);
    EXPECT_TRUE(service.IsEmpty());
  }
}

TEST(InprocessServiceTest, DownloadUploadTest) {
  InprocessService service;
  EXPECT_TRUE(service.IsEmpty());

  {
    ime_sync::UploadRequest request;
    ime_sync::UploadResponse response;
    request.set_version(1);

    for (int i = 0; i < 100; ++i) {
      ime_sync::SyncItem *item = request.mutable_items()->Add();
      CHECK(item);
      item->set_component(ime_sync::LANGUAGE_MODEL);
      sync::TestKey *key =
          item->mutable_key()->MutableExtension(
              sync::TestKey::ext);
      key->set_key("test_key" + Util::SimpleItoa(i));
      sync::TestValue *value =
          item->mutable_value()->MutableExtension(
              sync::TestValue::ext);
      value->set_value("test_value" + Util::SimpleItoa(i));
    }

    EXPECT_TRUE(service.Upload(&request, &response));
    EXPECT_FALSE(service.IsEmpty());
  }

  // Download All
  uint64 timestamp = 0;
  {
    ime_sync::DownloadRequest request;
    ime_sync::DownloadResponse response;
    request.set_version(1);
    EXPECT_TRUE(service.Download(&request, &response));
    EXPECT_EQ(100, response.items_size());
    EXPECT_GT(response.download_timestamp(), 0);
    timestamp = response.download_timestamp();
  }

  // Download items newer than |timestamp|. results must be 0.
  {
    ime_sync::DownloadRequest request;
    ime_sync::DownloadResponse response;
    request.set_version(1);
    request.set_last_download_timestamp(timestamp);
    EXPECT_TRUE(service.Download(&request, &response));
    EXPECT_EQ(0, response.items_size());
  }

  {
    ime_sync::UploadRequest request;
    ime_sync::UploadResponse response;
    request.set_version(1);

    // update first 50 items
    for (int i = 0; i < 50; ++i) {
      ime_sync::SyncItem *item = request.mutable_items()->Add();
      CHECK(item);
      item->set_component(ime_sync::LANGUAGE_MODEL);
      sync::TestKey *key =
          item->mutable_key()->MutableExtension(
              sync::TestKey::ext);
      key->set_key("test_key" + Util::SimpleItoa(i));
      sync::TestValue *value =
          item->mutable_value()->MutableExtension(
              sync::TestValue::ext);
      value->set_value("test_value" + Util::SimpleItoa(i));
    }

    // add more 100 items
    for (int i = 100; i < 200; ++i) {
      ime_sync::SyncItem *item = request.mutable_items()->Add();
      CHECK(item);
      item->set_component(ime_sync::LANGUAGE_MODEL);
      sync::TestKey *key =
          item->mutable_key()->MutableExtension(
              sync::TestKey::ext);
      key->set_key("test_key" + Util::SimpleItoa(i));
      sync::TestValue *value =
          item->mutable_value()->MutableExtension(
              sync::TestValue::ext);
      value->set_value("test_value" + Util::SimpleItoa(i));
    }

    EXPECT_TRUE(service.Upload(&request, &response));
    EXPECT_FALSE(service.IsEmpty());
  }

  {
    ime_sync::DownloadRequest request;
    ime_sync::DownloadResponse response;
    request.set_version(1);
    request.set_last_download_timestamp(timestamp);
    EXPECT_TRUE(service.Download(&request, &response));
    EXPECT_EQ(150, response.items_size());

    for (int i = 0; i < 50; ++i) {
      const ime_sync::SyncItem &item =response.items(i);
      const sync::TestKey &key =
          item.key().GetExtension(sync::TestKey::ext);
      const sync::TestValue &value =
          item.value().GetExtension(sync::TestValue::ext);
      EXPECT_EQ(key.key(), "test_key" + Util::SimpleItoa(i));
      EXPECT_EQ(value.value(), "test_value" + Util::SimpleItoa(i));
    }

    for (int i = 50; i < 150; ++i) {
      const ime_sync::SyncItem &item =response.items(i);
      const sync::TestKey &key =
          item.key().GetExtension(sync::TestKey::ext);
      const sync::TestValue &value =
          item.value().GetExtension(sync::TestValue::ext);
      EXPECT_EQ(key.key(), "test_key" + Util::SimpleItoa(i + 50));
      EXPECT_EQ(value.value(), "test_value" + Util::SimpleItoa(i + 50));
    }
  }
}
}  // sync
}  // mozc

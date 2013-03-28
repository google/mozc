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

#include "sync/learning_preference_adapter.h"

#include <cstdlib>
#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/system_util.h"
#include "base/util.h"
#include "storage/lru_storage.h"
#include "storage/memory_storage.h"
#include "storage/registry.h"
#include "storage/storage_interface.h"
#include "sync/learning_preference_sync_util.h"
#include "sync/sync.pb.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

// TODO(peria): Fix tests with DISABLED_ prefix after refactorization
//     of rewriter finishes.

namespace mozc {
namespace sync {

using mozc::storage::LRUStorage;

class LearningPreferenceAdapterTest : public testing::Test {
 public:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);

    storage_.reset(mozc::storage::MemoryStorage::New());
    mozc::storage::Registry::SetStorage(storage_.get());
    adapter_.reset(new LearningPreferenceAdapter);
    adapter_->ClearStorage();
  }

  virtual void TearDown() {
    adapter_->ClearStorage();
    adapter_.reset();
    mozc::storage::Registry::SetStorage(NULL);
    storage_.reset();
  }

  LRUStorage *CreateStorage(const string &filename) {
    LRUStorage *storage = new LRUStorage;
    EXPECT_TRUE(storage->OpenOrCreate(
        FileUtil::JoinPath(FLAGS_test_tmpdir, filename).c_str(),
        4, 1000, 0xffff));
    return storage;
  }

  LearningPreferenceAdapter *GetAdapter() {
    return adapter_.get();
  }

 private:
  scoped_ptr<LearningPreferenceAdapter> adapter_;
  scoped_ptr<mozc::storage::StorageInterface> storage_;
};

TEST_F(LearningPreferenceAdapterTest, BucketSize) {
  LearningPreferenceAdapter *adapter = GetAdapter();
  EXPECT_EQ(512, adapter->bucket_size());
}

TEST_F(LearningPreferenceAdapterTest, BucketId) {
  LearningPreferenceAdapter *adapter = GetAdapter();
  for (int i = 0; i < 1000; ++i) {
    const uint32 id = adapter->GetNextBucketId();
    EXPECT_GT(adapter->bucket_size(), id);
    EXPECT_LE(0, id);
  }
}

TEST_F(LearningPreferenceAdapterTest, DISABLED_Storage) {
  LearningPreferenceAdapter *adapter = GetAdapter();
  const LRUStorage storage1, storage2;

  // The adapter should has two default storages at first, for
  // USER_SEGMENT_HISTORY and USER_BOUNDARY_HISTORY.
  EXPECT_EQ(2, adapter->GetStorageSize());

  adapter->ClearStorage();
  adapter->AddStorage(
      LearningPreference::Entry::USER_SEGMENT_HISTORY,
      &storage1);

  adapter->AddStorage(
      LearningPreference::Entry::USER_BOUNDARY_HISTORY,
      &storage2);

  EXPECT_EQ(2, adapter->GetStorageSize());
  EXPECT_EQ(LearningPreference::Entry::USER_SEGMENT_HISTORY,
            adapter->GetStorage(0).type);
  EXPECT_EQ(LearningPreference::Entry::USER_BOUNDARY_HISTORY,
            adapter->GetStorage(1).type);
  EXPECT_EQ(&storage1,
            adapter->GetStorage(0).lru_storage);
  EXPECT_EQ(&storage2,
            adapter->GetStorage(1).lru_storage);

  adapter->ClearStorage();
  EXPECT_EQ(0, adapter->GetStorageSize());
}

TEST_F(LearningPreferenceAdapterTest, GetItemsToUpload) {
  LearningPreferenceAdapter *adapter = GetAdapter();

  scoped_ptr<LRUStorage> storage1(CreateStorage("file1"));
  scoped_ptr<LRUStorage> storage2(CreateStorage("file2"));

  storage1->Write(0, 0, "tst0", 0);
  storage1->Write(1, 1, "tst1", 10);
  storage1->Write(2, 2, "tst2", 20);
  storage1->Write(3, 3, "tst3", 30);

  storage2->Write(0, 4, "tst4", 5);
  storage2->Write(1, 5, "tst5", 15);
  storage2->Write(2, 6, "tst6", 25);
  storage2->Write(3, 7, "tst7", 35);

  adapter->ClearStorage();
  adapter->AddStorage(LearningPreference::Entry::USER_SEGMENT_HISTORY,
                     storage1.get());
  adapter->AddStorage(LearningPreference::Entry::USER_BOUNDARY_HISTORY,
                     storage2.get());

  {
    adapter->SetLastDownloadTimestamp(10);
    adapter->Start();
    const LearningPreference &update = adapter->local_update();
    LearningPreference update_expected;
    update_expected.CopyFrom(update);
    EXPECT_EQ(5, update.entries_size());
    ime_sync::SyncItems items;
    adapter->GetItemsToUpload(&items);
    EXPECT_EQ(1, items.size());
    const ime_sync::SyncItem &item = items.Get(0);
    const sync::LearningPreferenceValue &value =
        item.value().GetExtension(sync::LearningPreferenceValue::ext);
    EXPECT_TRUE(value.has_learning_preference());
    EXPECT_EQ(value.learning_preference().DebugString(),
              update_expected.DebugString());
  }

  {
    adapter->SetLastDownloadTimestamp(100);
    adapter->Start();
    const LearningPreference &update = adapter->local_update();
    EXPECT_EQ(0, update.entries_size());
  }

  {
    adapter->SetLastDownloadTimestamp(0);
    adapter->Start();
    const LearningPreference &update = adapter->local_update();
    LearningPreference update_expected;
    update_expected.CopyFrom(update);
    EXPECT_EQ(7, update.entries_size());
    ime_sync::SyncItems items;
    adapter->GetItemsToUpload(&items);
    EXPECT_EQ(1, items.size());
    const ime_sync::SyncItem &item = items.Get(0);
    const sync::LearningPreferenceValue &value =
        item.value().GetExtension(sync::LearningPreferenceValue::ext);
    EXPECT_TRUE(value.has_learning_preference());
    EXPECT_EQ(value.learning_preference().DebugString(),
              update_expected.DebugString());
  }
}

TEST_F(LearningPreferenceAdapterTest, SetDownloadedItems) {
  LearningPreferenceAdapter *adapter = GetAdapter();

  LearningPreference *update = adapter->mutable_local_update();
  update->Clear();
  for (int i = 0; i < 1000; ++i) {
    LearningPreference::Entry *entry = update->add_entries();
    entry->set_key(i);
    entry->set_value("test");
    entry->set_last_access_time(i + 1);
    entry->set_type(LearningPreference::Entry::USER_SEGMENT_HISTORY);
  }

  LearningPreference update_expected;
  update_expected.CopyFrom(*update);

  ime_sync::SyncItems items;
  adapter->GetItemsToUpload(&items);

  LearningPreference update_actual;

  for (size_t i = 0; i < items.size(); ++i) {
    const ime_sync::SyncItem &item = items.Get(i);
    EXPECT_EQ(item.component(), adapter->component_id());
    EXPECT_TRUE(item.key().HasExtension(sync::LearningPreferenceKey::ext));
    EXPECT_TRUE(item.value().HasExtension(sync::LearningPreferenceValue::ext));
    const sync::LearningPreferenceValue &value =
        item.value().GetExtension(sync::LearningPreferenceValue::ext);
    EXPECT_TRUE(value.has_learning_preference());
    update_actual.MergeFrom(value.learning_preference());
  }

  EXPECT_EQ(update_expected.DebugString(),
            update_actual.DebugString());

  scoped_ptr<LRUStorage> storage_reference(CreateStorage("file1"));
  adapter->ClearStorage();
  adapter->AddStorage(LearningPreference::Entry::USER_SEGMENT_HISTORY,
                     storage_reference.get());

  adapter->SetDownloadedItems(items);

  LRUStorage storage;
  EXPECT_TRUE(storage.Open((
      storage_reference->filename() + ".merge_pending").c_str()));
  EXPECT_EQ(1000, storage.size());

  update_actual.Clear();
  LearningPreferenceSyncUtil::CreateUpdate(
      storage,
      LearningPreference::Entry::USER_SEGMENT_HISTORY,
      0,
      &update_actual);

  EXPECT_EQ(update_expected.DebugString(),
            update_actual.DebugString());

  FileUtil::Unlink(storage.filename());
  FileUtil::Unlink(storage.filename() + ".merge_pending");
}

TEST_F(LearningPreferenceAdapterTest, LastDownloadTimestamp) {
  LearningPreferenceAdapter *adapter = GetAdapter();
  EXPECT_TRUE(adapter->SetLastDownloadTimestamp(1234));
  EXPECT_EQ(1234, adapter->GetLastDownloadTimestamp());

  for (int i = 0; i < 1000; ++i) {
    EXPECT_TRUE(adapter->SetLastDownloadTimestamp(i));
    EXPECT_EQ(i, adapter->GetLastDownloadTimestamp());
  }
}

TEST_F(LearningPreferenceAdapterTest, MarkUploaded) {
  LearningPreferenceAdapter *adapter = GetAdapter();

  ime_sync::SyncItem item;
  item.set_component(adapter->component_id());
  sync::LearningPreferenceKey *key =
      item.mutable_key()->MutableExtension(
          sync::LearningPreferenceKey::ext);
  sync::LearningPreferenceValue *value =
      item.mutable_value()->MutableExtension(
          sync::LearningPreferenceValue::ext);
  CHECK(key);
  CHECK(value);
  key->set_bucket_id(0);

  const uint64 synced_time = Util::GetTime();
  adapter->Start();

  // last_access_time is not updated.
  adapter->SetLastDownloadTimestamp(1234);
  adapter->MarkUploaded(item, false);
  EXPECT_EQ(1234, adapter->GetLastDownloadTimestamp());

  // last_access_time is updated.
  adapter->MarkUploaded(item, true);
  const int diff = abs(static_cast<int>(synced_time -
                                        adapter->GetLastDownloadTimestamp()));
  EXPECT_LE(diff, 2);
}

}  // namespace sync
}  // namespace mozc

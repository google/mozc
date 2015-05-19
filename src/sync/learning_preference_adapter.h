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

#ifndef MOZC_SYNC_LEARNING_PREFERENCE_ADAPTER_H_
#define MOZC_SYNC_LEARNING_PREFERENCE_ADAPTER_H_

#include <string>
#include <vector>
#include "base/base.h"
#include "sync/adapter_interface.h"
#include "sync/sync.pb.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {

namespace storage {
class LRUStorage;
}  // namespace storage

namespace sync {

class LearningPreferenceAdapter : public AdapterInterface {
 public:
  LearningPreferenceAdapter();
  virtual ~LearningPreferenceAdapter();
  virtual bool Start();
  virtual bool SetDownloadedItems(const ime_sync::SyncItems &items);
  virtual bool GetItemsToUpload(ime_sync::SyncItems *items);
  virtual bool MarkUploaded(
      const ime_sync::SyncItem &item, bool uploaded);
  virtual bool Clear();
  virtual ime_sync::Component component_id() const;

 private:
  friend class LearningPreferenceAdapterTest;
  FRIEND_TEST(LearningPreferenceAdapterTest, DISABLED_Storage);
  FRIEND_TEST(LearningPreferenceAdapterTest, BucketSize);
  FRIEND_TEST(LearningPreferenceAdapterTest, BucketId);
  FRIEND_TEST(LearningPreferenceAdapterTest, LastDownloadTimestamp);
  FRIEND_TEST(LearningPreferenceAdapterTest, SetDownloadedItems);
  FRIEND_TEST(LearningPreferenceAdapterTest, GetItemsToUpload);
  FRIEND_TEST(LearningPreferenceAdapterTest, MarkUploaded);

  struct Storage {
    LearningPreference::Entry::Type type;
    const mozc::storage::LRUStorage *lru_storage;
  };

  // Inject sync-target LRUStorages into the adapter.
  // These methods are basically for unittesting.
  // Default storages are added in the constructor.
  // |storage| is const, since all mutable operations are
  // executed in the main converter thread. The |storage|
  // is used for getting meta data, like filename, seed,
  // value size, of the |storage|.
  void AddStorage(LearningPreference::Entry::Type type,
                  const mozc::storage::LRUStorage *storage);
  void ClearStorage();
  size_t GetStorageSize() const;
  const Storage &GetStorage(size_t i) const;

  // Return the size of buckets.
  uint32 bucket_size() const;

  // Return next bucket id.
  uint32 GetNextBucketId() const;

  // return local update.
  const LearningPreference &local_update() const;
  LearningPreference *mutable_local_update();

  // Return last synced history filename.
  uint64 GetLastDownloadTimestamp() const;
  bool SetLastDownloadTimestamp(uint64 last_download_time);

  vector<Storage> storages_;
  LearningPreference local_update_;
  uint64 local_update_time_;
};
}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_USER_HISTORY_ADAPTER_H_

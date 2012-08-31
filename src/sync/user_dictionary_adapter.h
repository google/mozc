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

#ifndef MOZC_SYNC_USER_DICTIONARY_ADAPTER_H_
#define MOZC_SYNC_USER_DICTIONARY_ADAPTER_H_

#include <string>
#include <vector>
#include "base/base.h"
#include "dictionary/user_dictionary_storage.h"
#include "sync/adapter_interface.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace sync {

class UserDictionaryAdapter : public AdapterInterface {
 public:
  UserDictionaryAdapter();
  virtual ~UserDictionaryAdapter();
  virtual bool SetDownloadedItems(const ime_sync::SyncItems &items);
  virtual bool GetItemsToUpload(ime_sync::SyncItems *items);
  virtual bool MarkUploaded(
      const ime_sync::SyncItem &item, bool uploaded);
  virtual bool Clear();
  virtual ime_sync::Component component_id() const;

 private:
  // TODO(team): get rid of friend test.
  FRIEND_TEST(UserDictionaryAdapterTest, BucketSize);
  FRIEND_TEST(UserDictionaryAdapterTest, BucketId);
  FRIEND_TEST(UserDictionaryAdapterTest, UserDictionaryFileName);
  FRIEND_TEST(UserDictionaryAdapterTest, TemporaryFileExceeds);
  FRIEND_TEST(UserDictionaryAdapterTest, RealScenarioTest);
  FRIEND_TEST(UserDictionaryAdapterMigrationTest, SetDownloadedItems);
  FRIEND_TEST(UserDictionaryAdapterMigrationTest, SetDownloadedItemsSnapshot);
  FRIEND_TEST(UserDictionaryAdapterMigrationTest, SetDownloadedItemsConflicts);
  FRIEND_TEST(UserDictionaryAdapterMigrationTest, GetItemsToUpload);
  FRIEND_TEST(UserDictionaryAdapterMigrationTest, GetItemsToUploadSnapShot);

  // Return the size of buckets.
  uint32 bucket_size() const;

  // Return next bucket id.
  uint32 GetNextBucketId() const;

  // Save new bucket id.
  bool SetBucketId(uint32 bucket_id);

  // This is used for unittesting.
  void set_user_dictionary_filename(const string &filename) {
    user_dictionary_filename_ = filename;
  }
  const string &user_dictionary_filename() const {
    return user_dictionary_filename_;
  }

  // Return last synced dictionary filename.
  string GetLastSyncedUserDictionaryFileName() const;
  string GetTempLastSyncedUserDictionaryFileName() const;

  string user_dictionary_filename_;
};

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_USER_DICTIONARY_ADAPTER_H_

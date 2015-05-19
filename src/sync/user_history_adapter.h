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

#ifndef MOZC_SYNC_USER_HISTORY_ADAPTER_H_
#define MOZC_SYNC_USER_HISTORY_ADAPTER_H_

#include <string>
#include <vector>
#include "base/base.h"
#include "prediction/user_history_predictor.h"
#include "sync/adapter_interface.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace sync {

class UserHistoryAdapter : public AdapterInterface {
 public:
  UserHistoryAdapter();
  virtual ~UserHistoryAdapter();
  virtual bool SetDownloadedItems(const ime_sync::SyncItems &items);
  virtual bool GetItemsToUpload(ime_sync::SyncItems *items);
  virtual bool MarkUploaded(
      const ime_sync::SyncItem &item, bool uploaded);
  virtual bool Clear();
  virtual ime_sync::Component component_id() const;

 private:
  FRIEND_TEST(UserHistoryAdapterTest, BucketSize);
  FRIEND_TEST(UserHistoryAdapterTest, BucketId);
  FRIEND_TEST(UserHistoryAdapterTest, UserHistoryFileName);
  FRIEND_TEST(UserHistoryAdapterTest, LastDownloadTimestamp);
  FRIEND_TEST(UserHistoryAdapterTest, SetDownloadedItems);
  FRIEND_TEST(UserHistoryAdapterTest, GetItemsToUpload);
  FRIEND_TEST(UserHistoryAdapterTest, MarkUploaded);
  FRIEND_TEST(UserHistoryAdapterTest, RealScenarioTest);
  FRIEND_TEST(UserHistoryAdapterTest, DISABLED_EditAndDeleteAtTheSameTime);

  // Return the size of buckets.
  uint32 bucket_size() const;

  // Return next bucket id.
  uint32 GetNextBucketId() const;

  // Set user history file name.
  // This is used for unittesting.
  void SetUserHistoryFileName(const string &filename);
  string GetUserHistoryFileName() const;

  // Return last synced history filename.
  uint64 GetLastDownloadTimestamp() const;
  bool SetLastDownloadTimestamp(uint64 last_download_time);

  string user_history_filename_;
  uint64 local_update_time_;
};
}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_USER_HISTORY_ADAPTER_H_

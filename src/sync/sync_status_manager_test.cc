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

#include "sync/sync_status_manager.h"

#include "base/base.h"
#include "base/util.h"
#include "storage/registry.h"
#include "storage/tiny_storage.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace sync {
namespace {
const int kNumSyncGlobalStatus = 4;
const int kNumSyncError = 4;
}  // namespace

class SyncStatusManagerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    original_user_profile_dir_ = Util::GetUserProfileDirectory();
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    string registory_file_path;
    Util::JoinPath(original_user_profile_dir_, "registry.db",
                   &registory_file_path);
    local_storage_.reset(storage::TinyStorage::Create(
        registory_file_path.c_str()));
    storage::Registry::SetStorage(local_storage_.get());
    manager_.reset(new SyncStatusManager());
  }

  virtual void TearDown() {
    // SyncStatusManager updates registry.db when it is destructed. So we need
    // to delete it here before we restore the original user profile directory.
    manager_.reset(NULL);
    storage::Registry::SetStorage(NULL);
    local_storage_.reset(NULL);
    Util::SetUserProfileDirectory(original_user_profile_dir_);
  }

  scoped_ptr<SyncStatusManager> manager_;

 private:
  string original_user_profile_dir_;
  scoped_ptr<storage::StorageInterface> local_storage_;
};

TEST_F(SyncStatusManagerTest, GetSetLastSyncStatus) {
  // Set up test environment.
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  for (int i = 0; i < 10; ++i) {
    commands::CloudSyncStatus status_org;
    const uint64 t = Util::Random(1 << 30);
    status_org.set_global_status(commands::CloudSyncStatus::SYNC_SUCCESS);
    status_org.set_last_synced_timestamp(t);
    manager_->SetLastSyncStatus(status_org);

    commands::CloudSyncStatus status_new;
    status_new.set_global_status(commands::CloudSyncStatus::INSYNC);
    EXPECT_NE(t, status_new.last_synced_timestamp());
    EXPECT_NE(commands::CloudSyncStatus::SYNC_SUCCESS,
              status_new.global_status());

    manager_->GetLastSyncStatus(&status_new);
    EXPECT_EQ(t, status_new.last_synced_timestamp());
    EXPECT_EQ(commands::CloudSyncStatus::SYNC_SUCCESS,
              status_new.global_status());
  }
}

TEST_F(SyncStatusManagerTest, UpdateSyncStatus) {
  // Set up test environment.
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  for (int i = 0; i < 10; ++i) {
    commands::CloudSyncStatus status;
    commands::CloudSyncStatus::SyncGlobalStatus global =
        static_cast<commands::CloudSyncStatus::SyncGlobalStatus>(
            Util::Random(kNumSyncGlobalStatus));
    manager_->SetSyncGlobalStatus(global);
    manager_->GetLastSyncStatus(&status);
    EXPECT_EQ(global, status.global_status());
  }
}

TEST_F(SyncStatusManagerTest, StackOfSyncErrors) {
  // Set up test environment.
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  commands::CloudSyncStatus status;
  // global_status has no mean in this test, but it is required.
  status.set_global_status(commands::CloudSyncStatus::INSYNC);
  for (size_t i = 0; i < 10; ++i) {
    commands::CloudSyncStatus::ErrorCode error =
        static_cast<commands::CloudSyncStatus::ErrorCode>(
            Util::Random(kNumSyncError));
    manager_->AddSyncError(error);
    manager_->GetLastSyncStatus(&status);
    EXPECT_EQ(i + 1, status.sync_errors_size());
    EXPECT_EQ(error, status.sync_errors(i).error_code());
  }

  // Clean up sync_errors.
  manager_->NewSyncStatusSession();
  manager_->GetLastSyncStatus(&status);
  EXPECT_EQ(0, status.sync_errors_size());
}

}  // namespace sync
}  // namespace mozc

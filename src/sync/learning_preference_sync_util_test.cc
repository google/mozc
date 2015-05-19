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

#include "sync/learning_preference_sync_util.h"

#include <string>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/freelist.h"
#include "base/util.h"
#include "storage/lru_storage.h"
#include "sync/sync_util.h"
#include "sync/sync.pb.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace sync {
namespace {

using mozc::storage::LRUStorage;

TEST(LearningPreferenceSyncUtilTest, CreateUpdate) {
  const string filename1 = Util::JoinPath(FLAGS_test_tmpdir,
                                          "file1");
  const string filename2 = Util::JoinPath(FLAGS_test_tmpdir,
                                          "file2");

  Util::Unlink(filename1);
  Util::Unlink(filename2);

  LRUStorage storage1;
  LRUStorage storage2;

  EXPECT_TRUE(storage1.OpenOrCreate(
      filename1.c_str(), 4, 10, 0xffede));
  EXPECT_TRUE(storage2.OpenOrCreate(
      filename2.c_str(), 4, 10, 0xffede));

  // index, key, value, timestamp
  storage1.Write(0, 0, "tst1", 0);
  storage1.Write(1, 1, "tst2", 10);
  storage1.Write(2, 2, "tst3", 20);
  storage1.Write(3, 3, "tst4", 30);

  // index, key, value, timestamp
  storage2.Write(0, 4, "tst5", 5);
  storage2.Write(1, 5, "tst6", 15);
  storage2.Write(2, 6, "tst7", 25);
  storage2.Write(3, 7, "tst8", 35);

  LearningPreference update;

  LearningPreferenceSyncUtil::CreateUpdate(
      storage1,
      LearningPreference::Entry::USER_SEGMENT_HISTORY,
      10,   // newer than 10
      &update);

  EXPECT_EQ(2, update.entries_size());

  EXPECT_EQ(LearningPreference::Entry::USER_SEGMENT_HISTORY,
            update.entries(0).type());
  EXPECT_EQ(LearningPreference::Entry::USER_SEGMENT_HISTORY,
            update.entries(1).type());
  EXPECT_EQ(2, update.entries(0).key());
  EXPECT_EQ(3, update.entries(1).key());
  EXPECT_EQ("tst3", update.entries(0).value());
  EXPECT_EQ("tst4", update.entries(1).value());
  EXPECT_EQ(20, update.entries(0).last_access_time());
  EXPECT_EQ(30, update.entries(1).last_access_time());

  LearningPreferenceSyncUtil::CreateUpdate(
      storage2,
      LearningPreference::Entry::USER_BOUNDARY_HISTORY,
      10,   // newer than 10
      &update);

  EXPECT_EQ(5, update.entries_size());

  EXPECT_EQ(LearningPreference::Entry::USER_SEGMENT_HISTORY,
            update.entries(0).type());
  EXPECT_EQ(LearningPreference::Entry::USER_SEGMENT_HISTORY,
            update.entries(1).type());
  EXPECT_EQ(LearningPreference::Entry::USER_BOUNDARY_HISTORY,
            update.entries(2).type());
  EXPECT_EQ(LearningPreference::Entry::USER_BOUNDARY_HISTORY,
            update.entries(3).type());
  EXPECT_EQ(LearningPreference::Entry::USER_BOUNDARY_HISTORY,
            update.entries(4).type());
  EXPECT_EQ(2, update.entries(0).key());
  EXPECT_EQ(3, update.entries(1).key());
  EXPECT_EQ(5, update.entries(2).key());
  EXPECT_EQ(6, update.entries(3).key());
  EXPECT_EQ(7, update.entries(4).key());
  EXPECT_EQ("tst3", update.entries(0).value());
  EXPECT_EQ("tst4", update.entries(1).value());
  EXPECT_EQ("tst6", update.entries(2).value());
  EXPECT_EQ("tst7", update.entries(3).value());
  EXPECT_EQ("tst8", update.entries(4).value());
  EXPECT_EQ(20, update.entries(0).last_access_time());
  EXPECT_EQ(30, update.entries(1).last_access_time());
  EXPECT_EQ(15, update.entries(2).last_access_time());
  EXPECT_EQ(25, update.entries(3).last_access_time());
  EXPECT_EQ(35, update.entries(4).last_access_time());

  Util::Unlink(filename1);
  Util::Unlink(filename2);
}

TEST(LearningPreferenceSyncUtilTest, CreateMergePendingFile) {
  LearningPreference update;

  LearningPreference::Entry *entry[4];
  for (int i = 0; i < arraysize(entry); ++i) {
    entry[i] = update.add_entries();
  }

  entry[0]->set_key(0);
  entry[0]->set_value("tst0");
  entry[0]->set_last_access_time(0);
  entry[0]->set_type(LearningPreference::Entry::USER_SEGMENT_HISTORY);

  entry[1]->set_key(1);
  entry[1]->set_value("tst1");
  entry[1]->set_last_access_time(10);
  entry[1]->set_type(LearningPreference::Entry::USER_SEGMENT_HISTORY);

  entry[2]->set_key(2);
  entry[2]->set_value("tst2");
  entry[2]->set_last_access_time(20);
  entry[2]->set_type(LearningPreference::Entry::USER_BOUNDARY_HISTORY);

  entry[3]->set_key(3);
  entry[3]->set_value("tst3");
  entry[3]->set_last_access_time(30);
  entry[3]->set_type(LearningPreference::Entry::USER_BOUNDARY_HISTORY);

  {
    const string filename1 = Util::JoinPath(FLAGS_test_tmpdir,
                                            "file1");
    const string filename2 = Util::JoinPath(FLAGS_test_tmpdir,
                                            "file2");

    Util::Unlink(filename1);
    Util::Unlink(filename2);

    LRUStorage storage1;
    LRUStorage storage2;
    EXPECT_TRUE(storage1.OpenOrCreate(
        filename1.c_str(), 4, 10, 0xffede));
    EXPECT_TRUE(storage2.OpenOrCreate(
        filename2.c_str(), 4, 10, 0xffede));

    LearningPreferenceSyncUtil::CreateMergePendingFile(
        storage1,
        LearningPreference::Entry::USER_SEGMENT_HISTORY,
        update);

    LearningPreferenceSyncUtil::CreateMergePendingFile(
        storage2,
        LearningPreference::Entry::USER_BOUNDARY_HISTORY,
        update);

    Util::Unlink(filename1);
    Util::Unlink(filename2);
  }

  {
    const string filename1 = Util::JoinPath(FLAGS_test_tmpdir,
                                            "file1.merge_pending");
    const string filename2 = Util::JoinPath(FLAGS_test_tmpdir,
                                            "file2.merge_pending");

    LRUStorage storage1;
    LRUStorage storage2;
    EXPECT_TRUE(storage1.Open(filename1.c_str()));
    EXPECT_TRUE(storage2.Open(filename2.c_str()));

    EXPECT_EQ(2, storage1.size());
    EXPECT_EQ(2, storage2.size());

    uint64 key;
    string value;
    uint32 last_access_time;

    storage1.Read(0, &key, &value, &last_access_time);
    EXPECT_EQ(0, key);
    EXPECT_EQ("tst0", value);
    EXPECT_EQ(0, last_access_time);

    storage1.Read(1, &key, &value, &last_access_time);
    EXPECT_EQ(1, key);
    EXPECT_EQ("tst1", value);
    EXPECT_EQ(10, last_access_time);

    storage2.Read(0, &key, &value, &last_access_time);
    EXPECT_EQ(2, key);
    EXPECT_EQ("tst2", value);
    EXPECT_EQ(20, last_access_time);

    storage2.Read(1, &key, &value, &last_access_time);
    EXPECT_EQ(3, key);
    EXPECT_EQ("tst3", value);
    EXPECT_EQ(30, last_access_time);

    Util::Unlink(filename1);
    Util::Unlink(filename2);
  }
}
}  // namespace
}  // sync
}  // mozc

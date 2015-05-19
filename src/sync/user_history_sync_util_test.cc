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

#include "sync/user_history_sync_util.h"

#include <string>
#include "base/base.h"
#include "base/clock_mock.h"
#include "base/file_stream.h"
#include "base/freelist.h"
#include "base/number_util.h"
#include "base/util.h"
#include "prediction/user_history_predictor.h"
#include "sync/sync_util.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace sync {

class UserHistorySyncUtilTest : public testing::Test {
 public:
  virtual void SetUp() {
    clock_mock_.reset(new ClockMock(0, 0));
    clock_mock_->SetAutoPutClockForward(1, 0);
    Util::SetClockHandler(clock_mock_.get());
  }

  virtual void TearDown() {
    Util::SetClockHandler(NULL);
  }

  scoped_ptr<ClockMock> clock_mock_;
};

TEST_F(UserHistorySyncUtilTest, MergeEntry) {
  UserHistorySyncUtil::Entry entry;
  UserHistorySyncUtil::Entry new_entry;

  entry.set_suggestion_freq(10);
  entry.set_conversion_freq(12);
  entry.set_last_access_time(100);
  entry.set_removed(false);

  new_entry.set_suggestion_freq(11);
  new_entry.set_conversion_freq(8);
  new_entry.set_last_access_time(20);
  new_entry.set_removed(true);

  UserHistorySyncUtil::MergeEntry(entry, &new_entry);
  // take max values of two entries
  EXPECT_EQ(11, new_entry.suggestion_freq());
  EXPECT_EQ(12, new_entry.conversion_freq());
  EXPECT_EQ(100, new_entry.last_access_time());

  // follow the value in |entry|.
  EXPECT_FALSE(new_entry.removed());

  {
    UserHistorySyncUtil::Entry a, b;
    b.add_next_entries()->set_entry_fp(1);
    b.add_next_entries()->set_entry_fp(2);
    b.add_next_entries()->set_entry_fp(3);
    UserHistorySyncUtil::MergeEntry(a, &b);
    EXPECT_EQ(3, b.next_entries_size());
  }

  {
    UserHistorySyncUtil::Entry a, b;
    a.add_next_entries()->set_entry_fp(1);
    a.add_next_entries()->set_entry_fp(2);
    a.add_next_entries()->set_entry_fp(3);
    UserHistorySyncUtil::MergeEntry(a, &b);
    EXPECT_EQ(3, b.next_entries_size());
  }

  {
    UserHistorySyncUtil::Entry a, b;
    a.add_next_entries()->set_entry_fp(1);
    a.add_next_entries()->set_entry_fp(2);
    a.add_next_entries()->set_entry_fp(3);
    a.add_next_entries()->set_entry_fp(4);
    a.add_next_entries()->set_entry_fp(5);
    a.add_next_entries()->set_entry_fp(6);
    b.add_next_entries()->set_entry_fp(10);
    b.add_next_entries()->set_entry_fp(20);
    UserHistorySyncUtil::MergeEntry(a, &b);
    EXPECT_EQ(4, b.next_entries_size());
    EXPECT_EQ(1, b.next_entries(0).entry_fp());
    EXPECT_EQ(2, b.next_entries(1).entry_fp());
    EXPECT_EQ(3, b.next_entries(2).entry_fp());
    EXPECT_EQ(4, b.next_entries(3).entry_fp());
  }

  {
    UserHistorySyncUtil::Entry a, b;
    b.add_next_entries()->set_entry_fp(1);
    b.add_next_entries()->set_entry_fp(2);
    b.add_next_entries()->set_entry_fp(3);
    b.add_next_entries()->set_entry_fp(4);
    b.add_next_entries()->set_entry_fp(5);
    b.add_next_entries()->set_entry_fp(6);
    a.add_next_entries()->set_entry_fp(10);
    a.add_next_entries()->set_entry_fp(20);
    UserHistorySyncUtil::MergeEntry(a, &b);
    EXPECT_EQ(4, b.next_entries_size());
    EXPECT_EQ(10, b.next_entries(0).entry_fp());
    EXPECT_EQ(20, b.next_entries(1).entry_fp());
    EXPECT_EQ(3, b.next_entries(2).entry_fp());
    EXPECT_EQ(4, b.next_entries(3).entry_fp());
  }

  {
    UserHistorySyncUtil::Entry a, b;
    b.add_next_entries()->set_entry_fp(1);
    b.add_next_entries()->set_entry_fp(2);
    a.add_next_entries()->set_entry_fp(10);
    a.add_next_entries()->set_entry_fp(20);
    UserHistorySyncUtil::MergeEntry(a, &b);
    EXPECT_EQ(2, b.next_entries_size());
    EXPECT_EQ(10, b.next_entries(0).entry_fp());
    EXPECT_EQ(20, b.next_entries(1).entry_fp());
  }

  EXPECT_EQ(10000, UserHistoryPredictor::cache_size());
}

TEST_F(UserHistorySyncUtilTest, CreateUpdate) {
  UserHistorySyncUtil::UserHistory history;

  for (int i = 0; i < 1000; ++i) {
    UserHistorySyncUtil::Entry *entry = history.add_entries();
    entry->set_key("key" + NumberUtil::SimpleItoa(i));
    entry->set_key("value" + NumberUtil::SimpleItoa(i));
    entry->set_last_access_time(static_cast<uint64>(i));
  }

  {
    UserHistorySyncUtil::UserHistory update;
    UserHistorySyncUtil::CreateUpdate(history, 0, &update);
    EXPECT_EQ(1000, update.entries_size());
  }

  {
    UserHistorySyncUtil::UserHistory update;
    UserHistorySyncUtil::CreateUpdate(history, 2000, &update);
    EXPECT_EQ(0, update.entries_size());
  }

  {
    UserHistorySyncUtil::UserHistory update;
    UserHistorySyncUtil::CreateUpdate(history, 100, &update);
    EXPECT_EQ(900, update.entries_size());
  }
}

TEST_F(UserHistorySyncUtilTest, MergeUpdates) {
  FreeList<UserHistorySyncUtil::UserHistory> freelist(1024);

  vector<const UserHistorySyncUtil::UserHistory *> updates;
  UserHistorySyncUtil::UserHistory local_history;

  EXPECT_TRUE(UserHistorySyncUtil::MergeUpdates(updates,
                                                &local_history));
  UserHistorySyncUtil::AddRandomUpdates(&local_history);

  UserHistorySyncUtil::UserHistory prev;

  {
    prev.CopyFrom(local_history);
    EXPECT_TRUE(UserHistorySyncUtil::MergeUpdates(updates,
                                                  &local_history));
    EXPECT_EQ(prev.DebugString(), local_history.DebugString());
  }

  for (int i = 0; i < 10; ++i) {
    UserHistorySyncUtil::UserHistory *update = freelist.Alloc();
    UserHistorySyncUtil::AddRandomUpdates(update);
    updates.push_back(update);
  }

  {
    EXPECT_TRUE(UserHistorySyncUtil::MergeUpdates(updates,
                                                  &local_history));
    EXPECT_NE(prev.DebugString(), local_history.DebugString());
  }

  // new entries must be sorted by last_access_time.
  EXPECT_LT(local_history.entries_size(),
            UserHistoryPredictor::cache_size());

  uint64 prev_timestamp = kuint64max;
  for (size_t i = 0; i < local_history.entries_size(); ++i) {
    EXPECT_GE(prev_timestamp,
              local_history.entries(i).last_access_time());
    prev_timestamp = local_history.entries(i).last_access_time();
  }
}

TEST_F(UserHistorySyncUtilTest, MergeUpdatesClearAllEvent) {
  UserHistorySyncUtil::UserHistory local_history;

  UserHistorySyncUtil::AddRandomUpdates(&local_history);
  UserHistorySyncUtil::AddRandomUpdates(&local_history);
  UserHistorySyncUtil::AddRandomUpdates(&local_history);
  EXPECT_GT(local_history.entries_size(), 0);

  UserHistorySyncUtil::UserHistory update;
  UserHistorySyncUtil::Entry *entry = update.add_entries();
  entry->set_entry_type(
      UserHistorySyncUtil::UserHistory::Entry::CLEAN_ALL_EVENT);
  entry->set_last_access_time(kuint32max - 1);
  vector<const UserHistorySyncUtil::UserHistory *> updates;
  updates.push_back(&update);

  // All entries are removed as CLEAN_ALL_EVENT is applied.
  UserHistorySyncUtil::MergeUpdates(updates, &local_history);
  EXPECT_EQ(1, local_history.entries_size());
}

TEST_F(UserHistorySyncUtilTest, MergeUpdatesClearUnusedEvent) {
  UserHistorySyncUtil::UserHistory local_history;

  UserHistorySyncUtil::AddRandomUpdates(&local_history);
  UserHistorySyncUtil::AddRandomUpdates(&local_history);
  UserHistorySyncUtil::AddRandomUpdates(&local_history);
  EXPECT_GT(local_history.entries_size(), 0);

  size_t remain_size = 0;
  for (int i = 0; i < local_history.entries_size(); ++i) {
    if (i % 3 == 0) {
      local_history.mutable_entries(i)->set_suggestion_freq(0);
    }
    if (local_history.entries(i).suggestion_freq() > 0) {
      ++remain_size;
    }
  }

  UserHistorySyncUtil::UserHistory update;
  UserHistorySyncUtil::Entry *entry = update.add_entries();
  entry->set_entry_type(
      UserHistorySyncUtil::UserHistory::Entry::CLEAN_UNUSED_EVENT);
  entry->set_last_access_time(kuint32max - 1);
  vector<const UserHistorySyncUtil::UserHistory *> updates;
  updates.push_back(&update);

  UserHistorySyncUtil::MergeUpdates(updates, &local_history);
  EXPECT_EQ(1 + remain_size, local_history.entries_size());
}
}  // namespace sync
}  // namespace mozc

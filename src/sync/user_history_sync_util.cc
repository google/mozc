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

#include "sync/user_history_sync_util.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "base/freelist.h"
#include "prediction/user_history_predictor.h"
#include "prediction/user_history_predictor.pb.h"
#include "sync/sync_util.h"

namespace mozc {
namespace sync {

class EntryCompare {
 public:
  bool operator() (const UserHistorySyncUtil::Entry *a,
                   const UserHistorySyncUtil::Entry *b) const {
    return a->last_access_time() > b->last_access_time();
  }
};

bool UserHistorySyncUtil::CreateUpdate(const UserHistory &history,
                                       uint64 last_access_time,
                                       UserHistory *update) {
  DCHECK(update);
  update->Clear();

  for (size_t i = 0; i < history.entries_size(); ++i) {
    if (history.entries(i).last_access_time() >= last_access_time) {
      update->add_entries()->CopyFrom(history.entries(i));
    }
  }

  return true;
}

bool UserHistorySyncUtil::MergeEntry(const Entry &entry, Entry *new_entry) {
  DCHECK(new_entry);

  new_entry->set_suggestion_freq(max(new_entry->suggestion_freq(),
                                     entry.suggestion_freq()));
  new_entry->set_conversion_freq(max(new_entry->conversion_freq(),
                                     entry.conversion_freq()));
  new_entry->set_last_access_time(max(new_entry->last_access_time(),
                                      entry.last_access_time()));
  new_entry->set_removed(entry.removed());

  // merge next_entries.
  // next_entries performs like a queue whose size is 4.
  // Basically, this function tries to overwrite the existing next_entries.
  const size_t reserve_size = min(
      static_cast<int>(UserHistoryPredictor::max_next_entries_size()),
      max(new_entry->next_entries_size(),
          entry.next_entries_size()));

  const size_t size = min(
      static_cast<int>(UserHistoryPredictor::max_next_entries_size()),
      entry.next_entries_size());

  DCHECK_LE(size, reserve_size);

  // resize next entries.
  while (new_entry->next_entries_size() > reserve_size) {
    new_entry->mutable_next_entries()->RemoveLast();
  }

  while (new_entry->next_entries_size() < size) {
    new_entry->add_next_entries();
  }

  for (size_t i = 0; i < size; ++i) {
    new_entry->mutable_next_entries(i)->CopyFrom(entry.next_entries(i));
  }

  return true;
}

bool UserHistorySyncUtil::MergeUpdates(
    const vector<const UserHistory *> &updates,
    UserHistory *history) {
  DCHECK(history);

  // First, aggregate all updates
  vector<const Entry *> all_entries;

  // aggregate remote updates
  for (size_t i = 0; i < updates.size(); ++i) {
    const UserHistory *update = updates[i];
    DCHECK(update);
    for (size_t j = 0; j < update->entries_size(); ++j) {
      all_entries.push_back(&(update->entries(j)));
    }
  }

  if (all_entries.empty()) {
    VLOG(1) << "No need to update history";
    return true;
  }

  // aggregate local history
  for (size_t i = 0; i < history->entries_size(); ++i) {
    all_entries.push_back(&(history->entries(i)));
  }

  if (all_entries.empty()) {
    VLOG(1) << "No need to update history";
    return true;
  }

  VLOG(1) << all_entries.size() << " entries are found";

  // sort by last_access_time again.
  sort(all_entries.begin(), all_entries.end(), EntryCompare());

  vector<Entry *> merged_entries;
  FreeList<Entry> freelist(1024);

  {
    // Group by the same entry here.
    map<uint32, Entry *> merged_map;
    for (size_t i = 0; i < all_entries.size(); ++i) {
      DCHECK(all_entries[i]);
      const uint32 fp =
          UserHistoryPredictor::EntryFingerprint(*(all_entries[i]));
      map<uint32, Entry *>::iterator it = merged_map.find(fp);
      if (it != merged_map.end()) {
        DCHECK(it->second);
        MergeEntry(*(all_entries[i]), it->second);
      } else {
        Entry *entry = freelist.Alloc();
        DCHECK(entry);
        entry->Clear();
        entry->CopyFrom(*(all_entries[i]));
        merged_map.insert(make_pair(fp, entry));
      }
    }

    for (map<uint32, Entry *>::iterator it = merged_map.begin();
         it != merged_map.end(); ++it) {
      DCHECK(it->second);
      merged_entries.push_back(it->second);
    }
  }

  VLOG(1) << merged_entries.size() << " sorted entries";

  // sort by last_access_time
  sort(merged_entries.begin(), merged_entries.end(), EntryCompare());

  // find the latest CLEAN_ALL_EVENT and remove entries which
  // were created before the event.
  for (size_t i = 0; i < merged_entries.size(); ++i) {
    DCHECK(merged_entries[i]);
    if (merged_entries[i]->entry_type() ==
        UserHistory::Entry::CLEAN_ALL_EVENT) {
      for (int j = i + 1; j < merged_entries.size(); ++j) {
        merged_entries[j]->set_removed(true);
        VLOG(2) << "Removed: " << merged_entries[j]->DebugString();
      }
      break;
    }
  }

  // find the latest CLEAN_UNUSED_EVENT and
  // emulate UNUSED_EVENT behavior over the entries which
  // were created before the event.
  for (size_t i = 0; i < merged_entries.size(); ++i) {
    DCHECK(merged_entries[i]);
    if (merged_entries[i]->entry_type() ==
        UserHistory::Entry::CLEAN_UNUSED_EVENT) {
      for (int j = i + 1; j < merged_entries.size(); ++j) {
        if (merged_entries[j]->suggestion_freq() == 0) {
          merged_entries[j]->set_removed(true);
          VLOG(2) << "Removed: " << merged_entries[j]->DebugString();
        }
      }
      break;
    }
  }

  const size_t kLRUCacheSize = UserHistoryPredictor::cache_size();

  history->Clear();
  for (size_t i = 0; i < merged_entries.size(); ++i) {
    if (!merged_entries[i]->removed()) {
      history->add_entries()->CopyFrom(*(merged_entries[i]));
    }
    if (history->entries_size() >= kLRUCacheSize) {
      break;
    }
  }

  return true;
}

void UserHistorySyncUtil::AddRandomUpdates(UserHistory *history) {
  CHECK(history);
  if (Util::Random(10) == 0) {
    history->Clear();

    // insert ALL_CLEAR command for sync
    Entry *entry = history->add_entries();
    entry->set_entry_type(UserHistory::Entry::CLEAN_ALL_EVENT);
    entry->set_last_access_time(static_cast<uint32>(Util::GetTime()));
  }

  set<uint32> seen;
  for (int i = 0; i < history->entries_size(); ++i) {
    const uint32 fp =
        UserHistoryPredictor::EntryFingerprint(history->entries(i));
    seen.insert(fp);
    if (Util::Random(10) == 0) {
      // update values
      Entry *entry = history->mutable_entries(i);
      entry->set_conversion_freq(
          entry->conversion_freq() + Util::Random(3));
      entry->set_suggestion_freq(
          entry->suggestion_freq() + Util::Random(3));
      entry->set_last_access_time(Util::GetTime());
    }
  }

  const int add_size = Util::Random(50) + 1;
  for (int i = 0; i < add_size; ++i) {
    Entry *entry = history->add_entries();
    const string key = SyncUtil::GenRandomString(3);
    const string &value = key;
    entry->set_key(key);
    entry->set_value(value);
    entry->set_conversion_freq(Util::Random(3));
    entry->set_suggestion_freq(entry->conversion_freq() +
                               Util::Random(5));
    entry->set_last_access_time(Util::GetTime());

    const uint32 fp =
        UserHistoryPredictor::EntryFingerprint(*entry);
    if (!seen.insert(fp).second) {
      history->mutable_entries()->RemoveLast();
    }
  }
}
}  // sync
}  // mozc

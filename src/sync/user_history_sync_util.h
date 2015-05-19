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

#ifndef MOZC_SYNC_USER_DICTIONARY_SYNC_UTIL_H_
#define MOZC_SYNC_USER_DICTIONARY_SYNC_UTIL_H_

#include <string>
#include <vector>
#include "base/base.h"
#include "prediction/user_history_predictor.pb.h"

namespace mozc {

class UserHistoryStorage;

namespace sync {

class UserHistorySyncUtil {
 public:
  typedef user_history_predictor::UserHistory UserHistory;
  typedef user_history_predictor::UserHistory::Entry Entry;
  typedef user_history_predictor::UserHistory::NextEntry NextEntry;
  typedef user_history_predictor::UserHistory::Entry::EntryType EntryType;

  // Create |update| from |history|. It basically
  // aggreagate entries which are accessed after |last_access_time|.
  static bool CreateUpdate(const UserHistory &history,
                           uint64 last_access_time,
                           UserHistory *update);

  // Merge |entry| to |new_entry|.
  static bool MergeEntry(const Entry &entry, Entry *new_entry);

  // Merge a sequence of updates to the current |history|.
  static bool MergeUpdates(
      const vector<const UserHistory *> &updates,
      UserHistory *hitory);

  // Add random updates to |history|.
  // Used for unittesting.
  static void AddRandomUpdates(UserHistory *history);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UserHistorySyncUtil);
};
}  // sync
}  // mozc

#endif  // MOZC_SYNC_USER_DICTIONARY_SYNC_UTIL_H_

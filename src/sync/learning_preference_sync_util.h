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

#ifndef MOZC_SYNC_LEARNING_PREFERENCE_SYNC_UTIL_H_
#define MOZC_SYNC_LEARNING_PREFERENCE_SYNC_UTIL_H_

#include <string>
#include "base/port.h"
#include "sync/sync.pb.h"

namespace mozc {

namespace storage {
class LRUStorage;
}  // namepace storage

namespace sync {

class LearningPreferenceSyncUtil {
 public:
  // Create a local update from |storage| and save it |local_update|.
  // All items, which should be synced to the cloud, must be newer than
  // |last_access_time|. The storage data is handled as an independent
  // entry whose type is |type|.
  static bool CreateUpdate(const mozc::storage::LRUStorage &storage,
                           LearningPreference::Entry::Type type,
                           uint64 last_access_time,
                           LearningPreference *local_update);

  // Create a new local LRUStorage file from |remote_update| and |storage|.
  // |storage| is used for setting the meta data of the storage,
  // like output filename, value size, and seed. The new pending file name
  // is created as storage->filename() + ".merge_pending";
  // Since |remote_update| contains different types of entries, you can
  // filter them by |type|.
  static bool CreateMergePendingFile(
      const mozc::storage::LRUStorage &storage,
      LearningPreference::Entry::Type type,
      const LearningPreference &remote_update);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(LearningPreferenceSyncUtil);
};

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_LEARNING_PREFERENCE_SYNC_UTIL_H_

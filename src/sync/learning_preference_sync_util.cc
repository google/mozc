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
#include "base/logging.h"
#include "base/util.h"
#include "storage/lru_storage.h"

namespace mozc {
namespace sync {

using mozc::storage::LRUStorage;

bool LearningPreferenceSyncUtil::CreateUpdate(
    const LRUStorage &storage,
    LearningPreference::Entry::Type type,
    uint64 target_last_access_time,
    LearningPreference *local_update) {
  DCHECK(local_update);

  uint64 key;
  string value;
  uint32 last_access_time;
  for (size_t i = 0; i < storage.size(); ++i) {
    storage.Read(i, &key, &value, &last_access_time);
    if (last_access_time > target_last_access_time) {
      LearningPreference::Entry *entry = local_update->add_entries();
      DCHECK(entry);
      entry->set_type(type);
      entry->set_key(key);
      entry->set_value(value);
      entry->set_last_access_time(last_access_time);
    }
  }

  return true;
}

bool LearningPreferenceSyncUtil::CreateMergePendingFile(
    const LRUStorage &storage,
    LearningPreference::Entry::Type type,
    const LearningPreference &remote_update) {

  size_t merge_storage_size = 0;
  for (size_t i = 0; i < remote_update.entries_size(); ++i) {
    if (remote_update.entries(i).type() == type) {
      ++merge_storage_size;
    }
  }

  if (merge_storage_size == 0) {
    VLOG(1) << "No update is required: " << static_cast<int>(type);
    return true;
  }

  const char kFileSuffixTmp[] = ".merge_pending.tmp";
  const char kFileSuffix[] = ".merge_pending";
  const string filename = storage.filename() + kFileSuffix;
  const string filename_tmp = storage.filename() + kFileSuffixTmp;

  {
    if (!LRUStorage::CreateStorageFile(filename_tmp.c_str(),
                                       storage.value_size(),
                                       merge_storage_size,
                                       storage.seed())) {
      LOG(ERROR) << "CreateStorageFile failed: " << filename;
      return false;
    }

    LRUStorage merge_storage;
    if (!merge_storage.Open(filename_tmp.c_str())) {
      LOG(ERROR) << "LRUStorage::Open() failed: " << filename;
      return false;
    }

    size_t index = 0;
    for (size_t i = 0; i < remote_update.entries_size(); ++i) {
      if (remote_update.entries(i).type() != type) {
        continue;
      }
      DCHECK_LT(index, merge_storage_size);
      const LearningPreference::Entry &entry = remote_update.entries(i);
      merge_storage.Write(index,
                          entry.key(),
                          entry.value(),
                          entry.last_access_time());
      ++index;
    }
  }

  if (!Util::AtomicRename(filename_tmp, filename)) {
    LOG(ERROR) << "AtomicRename failed";
    return false;
  }

  return true;
}

}  // namespace sync
}  // namespace mozc

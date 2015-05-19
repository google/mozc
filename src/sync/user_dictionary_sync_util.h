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

#ifndef MOZC_SYNC_USER_DICTIONARY_SYNC_UTIL_H_
#define MOZC_SYNC_USER_DICTIONARY_SYNC_UTIL_H_

#include <string>
#include <vector>
#include "base/base.h"
#include "dictionary/user_dictionary_storage.pb.h"

namespace mozc {

class UserDictionaryStorage;

namespace sync {

class UserDictionarySyncUtil {
 public:
  typedef
  user_dictionary::UserDictionaryStorage UserDictionaryStorageBase;

  typedef
  user_dictionary::UserDictionaryStorage::UserDictionary UserDictionary;

  typedef
  user_dictionary::UserDictionaryStorage::UserDictionary::Entry
  UserDictionaryEntry;

  // Return true if |storage1| and |storage2| contain the same entries.
  // Even if the orders of entries are different, this function returns
  // true as long as they have the same entries.
  // This function is used for unittesting.
  static bool IsEqualStorage(const UserDictionaryStorageBase &storage1,
                             const UserDictionaryStorageBase &storage2);

  // Return a fingerprint of |entry|
  static uint64 EntryFingerprint(const UserDictionaryEntry &entry);

  // Return true if number of updates in |update| exceeds
  // some pre-defined threshold.
  static bool ShouldCreateSnapshot(const UserDictionaryStorageBase &update);

  // Create a snapshot of |storage_new| and save it into |update|.
  // It internally calles update->CopyFrom(storage_new).
  static bool CreateSnapshot(const UserDictionaryStorageBase &storage_new,
                             UserDictionaryStorageBase *update);

  // Given two user dictionaries |dictionary_old| and |dictionary_new|,
  // create an |update| which reflects the diffs of
  // |dictionary_old| and |dictionary_new|.
  // You may not need to call this function directly.
  static bool CreateDictionaryUpdate(const UserDictionary &dictionary_old,
                                     const UserDictionary &dictionary_new,
                                     UserDictionary *update);

  // Given two user dictionary storages |storage_old| and
  // |storage_new|, create a new storage which reflects the diff
  // between |storage_old| and |storage_new|.
  static bool CreateUpdate(const UserDictionaryStorageBase &storage_old,
                           const UserDictionaryStorageBase &storage_new,
                           UserDictionaryStorageBase *update);

  // Given one update, merge it to the current |storage|.
  static bool MergeUpdate(const UserDictionaryStorageBase &update,
                          UserDictionaryStorageBase *storage);

  // Given a sequence of updates, merge them to the current |storage|.
  // |updates| must be sorted by timestamp.
  // This method basically does the followings:
  // 1) find the latest update in |updates| having SNAPSHOT storage_type.
  // 2) merge the rest of updates which are newer than snapshot to |storage|.
  // return true if storage is synced to the local disk successfully.
  static bool MergeUpdates(
      const vector<const UserDictionaryStorageBase *> &updates,
      UserDictionaryStorageBase *storage);

  // Get lock and save storage, after verifying the numbers of entries in its
  // sync dictionaries do not exceed the limit.
  static bool VerifyLockAndSaveStorage(UserDictionaryStorage *storage);

  // Get lock and save storage.
  static bool LockAndSaveStorage(UserDictionaryStorage *storage);

 private:
  UserDictionarySyncUtil() {}
  ~UserDictionarySyncUtil() {}
};
}  // sync
}  // mozc

#endif  // MOZC_SYNC_USER_DICTIONARY_SYNC_UTIL_H_

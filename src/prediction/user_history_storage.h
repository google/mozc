// Copyright 2010-2021, Google Inc.
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

#ifndef MOZC_PREDICTION_USER_HISTORY_STORAGE_H_
#define MOZC_PREDICTION_USER_HISTORY_STORAGE_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "base/thread.h"
#include "prediction/user_history_predictor.pb.h"
#include "storage/encrypted_string_storage.h"
#include "storage/lru_cache.h"

// TODO(taku): Better to change the namespace instead of prediction.
namespace mozc::prediction {

class UserHistoryStorage;

namespace internal {

// Snapshot<T> is a smart pointer with unique lock.
// unique lock (RAII-based lock) is passed (moved) in the
// constructor so that the lock is released in detractor.
// `T` is managed by other module that holds the mutex.
// Snapshot<T> guarantees the thread-safe access to T.
template <class T>
class Snapshot {
 public:
  Snapshot() = delete;
  Snapshot(Snapshot&&) = default;
  Snapshot& operator=(Snapshot&&) = default;
  Snapshot(const Snapshot&) = delete;
  Snapshot& operator=(const Snapshot&) = delete;

  T* get() const { return ptr_; }
  T* operator->() const { return ptr_; }
  T& operator*() const { return *ptr_; }
  operator bool() const { return static_cast<bool>(ptr_); }

 private:
  friend class prediction::UserHistoryStorage;
  // `lock` must be moved so that we can release the lock in destructor.
  Snapshot(T* ptr, std::unique_lock<RecursiveMutex>&& lock)
      : ptr_(ptr), lock_(std::move(lock)) {}

  T* ptr_ = nullptr;
  std::unique_lock<RecursiveMutex> lock_;
};

}  // namespace internal

// UserHistoryStorage is a class that encapsulates lookup, insertion, and
// deletion of user history, as well as serialization to disk. All methods
// are thread-safe. This class is introduced to abstract and hide
// the storage implementation.
//
// Lookup method returns the Snapshot<Entry> that holds the scoped recursive
// mutex lock managed by UserHistoryStorage instance. The exclusive
// access on the same thread is guaranteed while `snapshot` is alive. Release
// `snapshot` when not necessary.
//
//  auto snapshot = storage.Lookup(fp);
//  LOG(INFO) << snapshot->key() << "\t" << snapshot->value();
//
class UserHistoryStorage {
 public:
  // Loads/stores the dict from/to `filename`.
  explicit UserHistoryStorage(absl::string_view filename);

  // Uses the default history filename.
  UserHistoryStorage();

  ~UserHistoryStorage();

  using Entry = user_history_predictor::UserHistory::Entry;

  using EntrySnapshot = internal::Snapshot<Entry>;
  using ConstEntrySnapshot = internal::Snapshot<const Entry>;
  using UniqueLock = std::unique_lock<RecursiveMutex>;

  // Non-blocking version of Save.
  void AsyncSave();

  // Non-blocking version of Load.
  void AsyncLoad();

  // Waits until the syncer finishes.
  void Wait() const;

  // Clears the storage and internal data.
  // This method is blocking.
  void Clear();

  // Returns true if the syncer is running.
  bool IsSyncerRunning() const;

  // Returns true if the syncer is in the critical section.
  // The Syncer handles both file IO and memory IO. The latter memory IO is the
  // actual critical section, where exclusive operation is necessary. If a
  // thread is inside the critical section, all other methods are blocked. This
  // method allows to prevent unintentional blocking.
  bool IsSyncerInCriticalSection() const;

  // Returns a unique lock object for RAII-based locking.
  // Example:
  // {
  //   auto lock = AcquireUniqueLock();
  //   for (...) { auto snapshot = Lookup(...); }
  // }
  //
  // By explicitly acquiring the lock initially, the Mutex lock inside
  // the for-loop is skipped due to the recursive call, resulting in better
  // performance on the same thread.
  UniqueLock AcquireUniqueLock() const;

  // Loads the user history from the local disk. This method is blocking.
  bool Load();

  // Saves the user history to the local disk. This method is blocking.
  bool Save();

  // Iterates the all entries in LRU order.
  // `func` is the callback. When `func` returns false, stops the iteration.
  void ForEach(absl::FunctionRef<bool(uint64_t, const Entry&)> func) const;

  //  Iterates the all entries in LRU order. Mutable entries are passed.
  void ForEach(absl::FunctionRef<bool(uint64_t, Entry&)> func);

  // Returns true if `fp` exists in the storage.
  bool Contains(uint64_t fp) const { return static_cast<bool>(Lookup(fp)); }

  // Inserts or updates the entry associated with `fp`.
  // Returns the inserted or updated entry. LRU order is updated.
  EntrySnapshot Insert(uint64_t fp) const;

  // Inserts new `entry` and update LRU order.
  void Insert(Entry entry) const;

  // Lookup the immutable entry associated with `fp`.
  // LRU order is not updated.
  ConstEntrySnapshot Lookup(uint64_t fp) const;

  // Lookup the mutable entry associated with `fp`.
  // LRU order is not updated.
  EntrySnapshot MutableLookup(uint64_t fp) const;

  // Defines the wrapper of Contains/Lookup/MutableLookup that
  // accept arbitrary `args`. Fingerprint(args) must be defined.
#define DEFINE_FP_WRAPPER(Method, RetType)                   \
  template <typename... Args>                                \
  inline RetType Method(Args&&... args) const {              \
    return Method(Fingerprint(std::forward<Args>(args)...)); \
  }

  DEFINE_FP_WRAPPER(Contains, bool);
  DEFINE_FP_WRAPPER(Lookup, ConstEntrySnapshot);
  DEFINE_FP_WRAPPER(MutableLookup, EntrySnapshot);

#undef DEFINE_FP_WRAPPER

  // Returns the LRU-head entry.
  ConstEntrySnapshot Head() const;

  // Returns the head->next entry.
  ConstEntrySnapshot HeadNext() const;

  // Returns null entry.
  // Useful to initialize snapshot with null variable.
  // auto snapshot = storage.NullEntry();
  // if (...) return snapshot;
  // snapshot = std::move(snapshot2);
  ConstEntrySnapshot NullEntry() const;

  // Finds the entry with linear search. Only top `size` elements are
  // searched. When the size is -1 (default) search all entries.
  ConstEntrySnapshot FindIf(
      absl::FunctionRef<bool(uint64_t, const Entry&)> func,
      int size = -1) const;

  // Erases `fps` from the storage.
  void Erase(absl::Span<const uint64_t> fps) const;

  // Returns true if the storage is empty.
  bool IsEmpty() const;

  // Returns fingerprints from various object.
  static uint64_t Fingerprint(absl::string_view key, absl::string_view value);
  static uint64_t Fingerprint(const Entry& entry);

 private:
  friend class UserHistoryStorageTestPeer;

  const std::string& filename() const { return filename_; }

  bool Load(user_history_predictor::UserHistory&& proto);

  // Migrate old 32bit Fingerprint to 64bit Fingerprint.
  static uint32_t FingerprintDepereated(absl::string_view key,
                                        absl::string_view value);

  static void MigrateNextEntries(
      user_history_predictor::UserHistory* absl_nonnull proto);

  using DicCache = storage::LruCache<uint64_t, Entry>;
  using DicElement = DicCache::Element;

  // Sets true if the internal data must be synced.
  mutable std::atomic<bool> needs_sync_ = false;

  // Sets true to cancel the syncer threads.
  std::atomic<bool> canceled_ = false;

  mutable TaskManager task_manager_;

  mutable RecursiveMutex mutex_;
  mutable std::unique_ptr<DicCache> dic_;

  const std::string filename_;
};
}  // namespace mozc::prediction

#endif  // MOZC_PREDICTION_USER_HISTORY_STORAGE_H_

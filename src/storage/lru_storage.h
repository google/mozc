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

#ifndef MOZC_STORAGE_LRU_STORAGE_H_
#define MOZC_STORAGE_LRU_STORAGE_H_

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "base/mmap.h"

namespace mozc {
namespace storage {

class LruStorage {
 public:
  LruStorage() = default;
  LruStorage(LruStorage&&) = default;
  LruStorage& operator=(LruStorage&&) = default;
  ~LruStorage() { Close(); }

  bool Open(const char* filename);
  void Close();

  // Try to open existing database
  // If the file is broken or cannot open, tries to recreate
  // new file
  bool OpenOrCreate(const char* filename, size_t new_value_size,
                    size_t new_size, uint32_t new_seed);

  // Looks up elements by key.
  const char* absl_nullable Lookup(absl::string_view key,
                                   uint32_t* last_access_time) const;
  const char* absl_nullable Lookup(absl::string_view key) const {
    uint32_t last_access_time;
    return Lookup(key, &last_access_time);
  }

  // A safer lookup for string values (the pointers returned by above Lookup()'s
  // are not null terminated.)
  absl::string_view LookupAsString(const absl::string_view key) const {
    const char* ptr = Lookup(key);
    return (ptr == nullptr) ? absl::string_view()
                            : absl::string_view(ptr, value_size_);
  }

  // Returns all the values.  The order is new to old (*values->begin() is the
  // most recently used one).
  void GetAllValues(std::vector<std::string>* values) const;

  // Clears all LRU cache.  The mapped file is also initialized
  bool Clear();

  // Merges other data into this LRU.
  bool Merge(const char* filename);
  bool Merge(const LruStorage& storage);

  // Updates timestamp.
  bool Touch(absl::string_view key);

  // Inserts a key value pair.
  bool Insert(absl::string_view key, const char* value);

  // Inserts a key value pair only if |key| already exists.
  // CAUTION: despite the name, it does nothing if there's no value of |key|.
  bool TryInsert(absl::string_view key, const char* value);

  // Deletes the element if exists.  Returns false on failure (it's not failure
  // if the element for |key| doesn't exist.)
  bool Delete(absl::string_view key);

  // Deletes all the elements that have timestamp less than |timestamp|, i.e.,
  // the last access is before |timestamp|.  Returns the number of deleted
  // elements.
  int DeleteElementsBefore(uint32_t timestamp);

  // Returns the byte length of each item, which is the user specified value
  // size + 12 bytes.  Here, 12 bytes is used for fingerprint (8 bytes) and
  // timestamp (4 bytes).
  size_t item_size() const { return value_size_ + kItemHeaderSize; }

  // Returns the user specified value size.
  size_t value_size() const { return value_size_; }

  // Returns the maximum number of item (capacity).
  size_t size() const { return size_; }

  // Returns the number of items in LRU.
  size_t used_size() const { return lru_list_.size(); }

  // Returns the seed used for fingerprinting.
  uint32_t seed() const { return seed_; }

  const std::string& filename() const { return filename_; }

  // Writes one entry at |i| th index.
  // i must be 0 <= i < size.
  // This data will not update the index of the storage.
  void Write(size_t i, uint64_t fp, absl::string_view value,
             uint32_t last_access_time);

  // Reads one entry from |i| th index.
  // i must be 0 <= i < size.
  void Read(size_t i, uint64_t* fp, std::string* value,
            uint32_t* last_access_time) const;

  // Creates Instance from file. Call Open internally
  static std::unique_ptr<LruStorage> Create(const char* filename);

  // Creates Instance from file. Call OpenOrCreate internally
  static std::unique_ptr<LruStorage> Create(const char* filename,
                                            size_t value_size, size_t size,
                                            uint32_t seed);

  // Creates an empty LRU db file
  static bool CreateStorageFile(const char* filename, size_t value_size,
                                size_t size, uint32_t seed);

  // The byte length used to store fingerprint and timestamp for each item.
  // * 8 bytes for fingerprint
  // * 4 bytes for timestamp.
  static constexpr size_t kItemHeaderSize = 12;

 private:
  // Initializes this LRU from memory buffer.
  bool Open(char* ptr, size_t ptr_size);

  // Deletes the element from |fp| or |it|.
  bool Delete(uint64_t fp);
  bool Delete(std::list<char*>::iterator it);

  // Actual implementation of Delete() methods.
  bool Delete(uint64_t fp, std::list<char*>::iterator it);

  size_t value_size_ = 0;
  size_t size_ = 0;
  uint32_t seed_ = 0;
  char* next_item_ = nullptr;
  char* begin_ = nullptr;
  char* end_ = nullptr;
  std::string filename_;
  std::list<char*> lru_list_;  // Front is the most recently used data.
  absl::flat_hash_map<uint64_t, std::list<char*>::iterator> lru_map_;
  Mmap mmap_;
};

}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_LRU_STORAGE_H_

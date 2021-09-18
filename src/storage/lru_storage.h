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

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/mmap.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace storage {

class LruStorage {
 public:
  LruStorage();

  LruStorage(const LruStorage &) = delete;
  LruStorage &operator=(const LruStorage &) = delete;

  ~LruStorage();

  bool Open(const char *filename);
  void Close();

  // Try to open existing database
  // If the file is broken or cannot open, tries to recreate
  // new file
  bool OpenOrCreate(const char *filename, size_t new_value_size,
                    size_t new_size, uint32_t new_seed);

  // Looks up elements by key.
  const char *Lookup(const std::string &key, uint32_t *last_access_time) const;
  const char *Lookup(const std::string &key) const;

  // A safer lookup for string values (the pointers returned by above Lookup()'s
  // are not null terminated.)
  absl::string_view LookupAsString(const std::string &key) const {
    const char *ptr = Lookup(key);
    return (ptr == nullptr) ? absl::string_view()
                            : absl::string_view(ptr, value_size_);
  }

  // Returns all the values.  The order is new to old (*values->begin() is the
  // most recently used one).
  void GetAllValues(std::vector<std::string> *values) const;

  // Clears all LRU cache.  The mapped file is also initialized
  bool Clear();

  // Merges other data into this LRU.
  bool Merge(const char *filename);
  bool Merge(const LruStorage &storage);

  // Updates timestamp.
  bool Touch(const std::string &key);

  // Inserts a key value pair.
  bool Insert(const std::string &key, const char *value);

  // Inserts a key value pair only if |key| already exists.
  // CAUTION: despite the name, it does nothing if there's no value of |key|.
  bool TryInsert(const std::string &key, const char *value);

  // Deletes the element if exists.  Returns false on failure (it's not failure
  // if the element for |key| doesn't exist.)
  bool Delete(const std::string &key);

  // Deletes all the elements that have timestamp less than |timestamp|, i.e.,
  // the last access is before |timestamp|.  Returns the number of deleted
  // elements.
  int DeleteElementsBefore(uint32_t timestamp);

  // Deletes all the elements that are not accessed for 62 days.
  // Returns the number of deleted elements.
  int DeleteElementsUntouchedFor62Days();

  // Returns the byte length of each item, which is the user specified value
  // size + 12 bytes.  Here, 12 bytes is used for fingerprint (8 bytes) and
  // timestamp (4 bytes).
  size_t item_size() const;

  // Returns the user specified value size.
  size_t value_size() const;

  // Returns the maximum number of item (capacity).
  size_t size() const;

  // Returns the number of items in LRU.
  size_t used_size() const;

  // Returns the seed used for fingerprinting.
  uint32_t seed() const;

  const std::string &filename() const;

  // Writes one entry at |i| th index.
  // i must be 0 <= i < size.
  // This data will not update the index of the storage.
  void Write(size_t i, uint64_t fp, const std::string &value,
             uint32_t last_access_time);

  // Reads one entry from |i| th index.
  // i must be 0 <= i < size.
  void Read(size_t i, uint64_t *fp, std::string *value,
            uint32_t *last_access_time) const;

  // Creates Instance from file. Call Open internally
  static LruStorage *Create(const char *filename);

  // Creates Instance from file. Call OpenOrCreate internally
  static LruStorage *Create(const char *filename, size_t value_size,
                            size_t size, uint32_t seed);

  // Creates an empty LRU db file
  static bool CreateStorageFile(const char *filename, size_t value_size,
                                size_t size, uint32_t seed);

 private:
  // Initializes this LRU from memory buffer.
  bool Open(char *ptr, size_t ptr_size);

  // Deletes the element from |fp| or |it|.
  bool Delete(uint64_t fp);
  bool Delete(std::list<char *>::iterator it);

  // Actual implementation of Delete() methods.
  bool Delete(uint64_t fp, std::list<char *>::iterator it);

  size_t value_size_;
  size_t size_;
  uint32_t seed_;
  char *next_item_;
  char *begin_;
  char *end_;
  std::string filename_;
  std::list<char *> lru_list_;  // Front is the most recently used data.
  absl::flat_hash_map<uint64_t, std::list<char *>::iterator> lru_map_;
  std::unique_ptr<Mmap> mmap_;
};

}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_LRU_STORAGE_H_

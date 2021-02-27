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

#ifndef MOZC_STORAGE_REGISTRY_H_
#define MOZC_STORAGE_REGISTRY_H_

#include <cstdint>
#include <cstring>
#include <string>

#include "base/logging.h"
#include "base/port.h"

namespace mozc {
namespace storage {

class StorageInterface;

// The idea of Registry module is the same as Windows Registry.
// You can use it for saving small data like timestamp, auth_token.
// DO NOT USE it to save big data or data which are frequently updated.
// Lookup() and Insert() do process-wide global lock and it may be slow.
// All methods are thread-safe.
//
// TODO(taku): Currently, Registry won't guarantee that two processes
// can con-currently share the same data. We will replace the backend
// of Registry (Storage Interface) with sqlite
//
// Example:
//  uint64 timestamp = 0;
//  CHECK(Registry::Lookup<uint64>("timestamp", &timestamp));
//
//  string value = "hello world";
//  CHECK(Registry::Insert<string>("hello", value));
class Registry {
 public:
  template <typename T>
  static bool Lookup(const std::string &key, T *value) {
    DCHECK(value);
    std::string tmp;
    if (!LookupInternal(key, &tmp)) {
      return false;
    }
    if (sizeof(*value) != tmp.size()) {
      return false;
    }
    memcpy(reinterpret_cast<char *>(value), tmp.data(), tmp.size());
    return true;
  }

  static bool Lookup(const std::string &key, std::string *value) {
    return LookupInternal(key, value);
  }

  static bool Lookup(const std::string &key, bool *value) {
    uint8_t v = 0;
    const bool result = Lookup<uint8_t>(key, &v);
    *value = (v != 0);
    return result;
  }

  // insert key and data
  // It is not guaranteed that the data is synced to the disk
  template <typename T>
  static bool Insert(const std::string &key, const T &value) {
    std::string tmp(reinterpret_cast<const char *>(&value), sizeof(value));
    return InsertInternal(key, tmp);
  }

  // Insert key and string value
  // It is not guaranteed that the data is synced to the disk
  static bool Insert(const std::string &key, const std::string &value) {
    return InsertInternal(key, value);
  }

  // Insert key and bool value
  // It is not guaranteed that the data is synced to the disk
  static bool Insert(const std::string &key, const bool value) {
    const uint8_t tmp = static_cast<uint8_t>(value);
    return Insert<uint8_t>(key, tmp);
  }

  // Syncing the data into disk
  static bool Sync();

  // Erase key
  static bool Erase(const std::string &key);

  // clear internal keys and values
  static bool Clear();

  // Inject internal storage for unittesting.
  // TinyStorage is used by default
  // TODO(taku) replace it with SQLITE
  static void SetStorage(StorageInterface *handler);

 private:
  static bool LookupInternal(const std::string &key, std::string *value);
  static bool InsertInternal(const std::string &key, const std::string &value);

  DISALLOW_IMPLICIT_CONSTRUCTORS(Registry);
};
}  // namespace storage
}  // namespace mozc
#endif  // MOZC_STORAGE_REGISTRY_H_

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

#include "storage/memory_storage.h"

#include <map>
#include <string>

#include "base/logging.h"

namespace mozc {
namespace storage {

namespace {

class MemoryStorageImpl : public storage::StorageInterface {
 public:
  bool Open(const std::string &filename) override {
    data_.clear();
    return true;
  }

  bool Sync() override { return true; }

  bool Lookup(const std::string &key, std::string *value) const override {
    CHECK(value);
    std::map<std::string, std::string>::const_iterator it = data_.find(key);
    if (it == data_.end()) {
      return false;
    }
    *value = it->second;
    return true;
  }

  bool Insert(const std::string &key, const std::string &value) override {
    data_[key] = value;
    return true;
  }

  bool Erase(const std::string &key) override {
    std::map<std::string, std::string>::iterator it = data_.find(key);
    if (it != data_.end()) {
      data_.erase(it);
      return true;
    }
    return false;
  }

  bool Clear() override {
    data_.clear();
    return true;
  }

  size_t Size() const override { return data_.size(); }

  MemoryStorageImpl() {}
  ~MemoryStorageImpl() override {}

 private:
  std::map<std::string, std::string> data_;
};

}  // namespace

StorageInterface *MemoryStorage::New() { return new MemoryStorageImpl; }

}  // namespace storage
}  // namespace mozc

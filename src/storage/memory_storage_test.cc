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

#include <iterator>
#include <memory>
#include <string>

#include "storage/storage_interface.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"

namespace mozc {
namespace storage {
namespace {

using TargetMap = absl::flat_hash_map<std::string, std::string>;

void CreateKeyValue(TargetMap *output, int size) {
  output->clear();
  for (int i = 0; i < size; ++i) {
    output->emplace(absl::StrCat("key", i), absl::StrCat("value", i));
  }
}

}  // namespace

TEST(MemoryStorageTest, SimpleTest) {
  static constexpr int kSize[] = {10, 100, 1000};

  for (int i = 0; i < std::size(kSize); ++i) {
    std::unique_ptr<StorageInterface> storage(MemoryStorage::New());

    // Insert
    TargetMap target;
    CreateKeyValue(&target, kSize[i]);
    {
      for (const auto &[key, value] : target) {
        EXPECT_TRUE(storage->Insert(key, value));
      }
    }

    // Lookup
    for (const auto &[key, expected] : target) {
      std::string value;
      EXPECT_TRUE(storage->Lookup(key, &value));
      EXPECT_EQ(value, expected);
    }

    for (const auto &it : target) {
      const std::string key = absl::StrCat(it.first, ".dummy");
      std::string value;
      EXPECT_FALSE(storage->Lookup(key, &value));
    }

    // Erase
    int id = 0;
    for (const auto &it : target) {
      if (id % 2 == 0) {
        EXPECT_TRUE(storage->Erase(it.first));
        const std::string key = absl::StrCat(it.first, ".dummy");
        EXPECT_FALSE(storage->Erase(key));
      }
      ++id;
    }

    id = 0;
    for (const auto &it : target) {
      std::string value;
      const std::string &key = it.first;
      if (id % 2 == 0) {
        EXPECT_FALSE(storage->Lookup(key, &value));
      } else {
        EXPECT_TRUE(storage->Lookup(key, &value));
      }
      ++id;
    }
  }
}

}  // namespace storage
}  // namespace mozc

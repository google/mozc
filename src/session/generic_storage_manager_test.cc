// Copyright 2010-2014, Google Inc.
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

#include "session/generic_storage_manager.h"

#include "base/file_util.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace {

static const string GetTemporaryFilePath() {
  return mozc::FileUtil::JoinPath(FLAGS_test_tmpdir,
                                  "generic_storage_test.db");
}

}  // namespace

namespace mozc {

TEST(GenericStorageManagerFactoryTest, GetStorage) {
  GenericStorageInterface *symbol =
      GenericStorageManagerFactory::GetStorage(
          commands::GenericStorageEntry::SYMBOL_HISTORY);
  GenericStorageInterface *emoticon =
      GenericStorageManagerFactory::GetStorage(
          commands::GenericStorageEntry::EMOTICON_HISTORY);
  GenericStorageInterface *invalid =
      GenericStorageManagerFactory::GetStorage(
          (commands::GenericStorageEntry::StorageType)(100));

  EXPECT_TRUE(symbol);
  EXPECT_TRUE(emoticon);
  EXPECT_FALSE(invalid);
  EXPECT_NE(symbol, emoticon);
}

// Checks the basic operations of LRU.
// Detailed test is omitted because storage/lru_storage_test.cc does.
TEST(GenericLruStorageTest, BasicOperations) {
  const int kValueSize = 12;
  const int kSize = 10;
  const string kPrinfFormat = Util::StringPrintf("%%%dlu", kValueSize);

  GenericLruStorage storage(
      GetTemporaryFilePath().data(), kValueSize, kSize, 123);
  // Inserts (kSize + 1) entries.
  for (size_t i = 0; i < kSize + 1; ++i) {
    const string value = Util::StringPrintf(kPrinfFormat.data(), i);
    const string key = string("key") + value;
    storage.Insert(key, value.data());
    // Check the existence.
    EXPECT_EQ(value, string(storage.Lookup(key), kValueSize));
  }

  // First inserted entry is already pushed out.
  EXPECT_EQ(NULL, storage.Lookup("0"));
  for (size_t i = 1; i < kSize + 1; ++i) {
    const string value = Util::StringPrintf(kPrinfFormat.data(), i);
    const string key = string("key") + value;
    EXPECT_EQ(value, string(storage.Lookup(key), kValueSize));
  }

  // Check the list.
  vector<string> values;
  storage.GetAllValues(&values);
  EXPECT_EQ(kSize, values.size());
  for (size_t i = 1; i < kSize + 1; ++i) {
    EXPECT_EQ(Util::StringPrintf(kPrinfFormat.data(), i),
              values.at(kSize - i));
  }

  // Clean
  storage.Clear();
  storage.GetAllValues(&values);
  EXPECT_TRUE(values.empty());
}

}  // namespace mozc

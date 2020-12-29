// Copyright 2010-2020, Google Inc.
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

#include "storage/tiny_storage.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/file_util.h"
#include "base/port.h"
#include "storage/storage_interface.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/strings/str_format.h"

namespace mozc {
namespace storage {
namespace {

void CreateKeyValue(std::map<std::string, std::string> *output, int size) {
  output->clear();
  for (int i = 0; i < size; ++i) {
    char key[64];
    char value[64];
    absl::SNPrintF(key, sizeof(key), "key%d", i);
    absl::SNPrintF(value, sizeof(value), "value%d", i);
    output->insert(std::pair<std::string, std::string>(key, value));
  }
}

}  // namespace

class TinyStorageTest : public testing::Test {
 protected:
  TinyStorageTest() {}

  void SetUp() override { UnlinkDBFileIfExists(); }

  void TearDown() override { UnlinkDBFileIfExists(); }

  static void UnlinkDBFileIfExists() {
    const std::string path = GetTemporaryFilePath();
    if (FileUtil::FileExists(path)) {
      FileUtil::Unlink(path);
    }
  }

  static StorageInterface *CreateStorage() { return TinyStorage::New(); }

  static std::string GetTemporaryFilePath() {
    // This name should be unique to each test.
    return FileUtil::JoinPath(FLAGS_test_tmpdir, "TinyStorageTest_test.db");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TinyStorageTest);
};

TEST_F(TinyStorageTest, TinyStorageTest) {
  const std::string filename = GetTemporaryFilePath();

  static const int kSize[] = {10, 100, 1000};

  for (int i = 0; i < arraysize(kSize); ++i) {
    FileUtil::Unlink(filename);
    std::unique_ptr<StorageInterface> storage(CreateStorage());

    // Insert
    std::map<std::string, std::string> target;
    CreateKeyValue(&target, kSize[i]);
    {
      EXPECT_TRUE(storage->Open(filename));
      for (std::map<std::string, std::string>::const_iterator it =
               target.begin();
           it != target.end(); ++it) {
        EXPECT_TRUE(storage->Insert(it->first, it->second));
      }
    }

    // Lookup
    for (std::map<std::string, std::string>::const_iterator it = target.begin();
         it != target.end(); ++it) {
      std::string value;
      EXPECT_TRUE(storage->Lookup(it->first, &value));
      EXPECT_EQ(value, it->second);
    }

    for (std::map<std::string, std::string>::const_iterator it = target.begin();
         it != target.end(); ++it) {
      const std::string key = it->first + ".dummy";
      std::string value;
      EXPECT_FALSE(storage->Lookup(key, &value));
    }

    storage->Sync();

    std::unique_ptr<StorageInterface> storage2(CreateStorage());
    EXPECT_TRUE(storage2->Open(filename));
    EXPECT_EQ(storage->Size(), storage2->Size());

    // Lookup
    for (std::map<std::string, std::string>::const_iterator it = target.begin();
         it != target.end(); ++it) {
      std::string value;
      EXPECT_TRUE(storage2->Lookup(it->first, &value));
      EXPECT_EQ(value, it->second);
    }

    for (std::map<std::string, std::string>::const_iterator it = target.begin();
         it != target.end(); ++it) {
      const std::string key = it->first + ".dummy";
      std::string value;
      EXPECT_FALSE(storage2->Lookup(key, &value));
    }

    // Erase
    int id = 0;
    for (std::map<std::string, std::string>::const_iterator it = target.begin();
         it != target.end(); ++it) {
      if (id % 2 == 0) {
        EXPECT_TRUE(storage->Erase(it->first));
        const std::string key = it->first + ".dummy";
        EXPECT_FALSE(storage->Erase(key));
      }
    }

    for (std::map<std::string, std::string>::const_iterator it = target.begin();
         it != target.end(); ++it) {
      std::string value;
      const std::string &key = it->first;
      if (id % 2 == 0) {
        EXPECT_FALSE(storage->Lookup(key, &value));
      } else {
        EXPECT_TRUE(storage->Lookup(key, &value));
      }
    }
  }
}

}  // namespace storage
}  // namespace mozc

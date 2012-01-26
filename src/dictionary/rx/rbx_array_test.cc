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

#include <sstream>

#include "base/file_stream.h"
#include "base/hash_tables.h"
#include "base/mmap.h"
#include "base/util.h"
#include "dictionary/rx/rbx_array.h"
#include "dictionary/rx/rbx_array_builder.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace rx {
namespace {

class RbxArrayTest : public testing::Test {
 protected:
  RbxArrayTest()
      : test_rbx_(FLAGS_test_tmpdir + "/test_rbx") {}

  virtual void SetUp() {
    Util::Unlink(test_rbx_);
  }

  virtual void TearDown() {
    Util::Unlink(test_rbx_);
  }

  void PushTestStr(const string &value, RbxArrayBuilder *builder) const {
    DCHECK(builder);
    ostringstream oss;
    const int len = value.size();
    oss.write(reinterpret_cast<const char *>(&len), sizeof(len));
    oss.write(reinterpret_cast<const char *>(value.data()), len);
    const string &str = oss.str();
    builder->Push(str);
  }

  string GetTestStr(const RbxArray &array, int index) const {
    const unsigned char *ptr = array.Get(index);
    EXPECT_TRUE(ptr != NULL);
    const int len = *(reinterpret_cast<const int *>(ptr));
    ptr += sizeof(int);
    return string(reinterpret_cast<const char *>(ptr), len);
  }

  void WriteToFile(const RbxArrayBuilder &builder) {
    OutputFileStream ofs(test_rbx_.c_str(), ios::binary|ios::out);
    builder.WriteImage(&ofs);
    EXPECT_TRUE(Util::FileExists(test_rbx_));
  }

  void ReadFromFile(RbxArray *array) {
    DCHECK(array);
    EXPECT_TRUE(Util::FileExists(test_rbx_));
    mapping_.reset(new Mmap<char>());
    mapping_->Open(test_rbx_.c_str());
    const char *ptr = mapping_->begin();
    EXPECT_TRUE(array->OpenImage(
        reinterpret_cast<const unsigned char *>(ptr)));
  }
  scoped_ptr<Mmap<char> > mapping_;

  const string test_rbx_;
};

TEST_F(RbxArrayTest, BasicTest) {
  {
    RbxArrayBuilder builder;
    PushTestStr(string(1000, 'a'), &builder);
    PushTestStr("", &builder);
    PushTestStr("", &builder);
    PushTestStr("", &builder);
    PushTestStr("a", &builder);
    builder.Build();
    WriteToFile(builder);
  }
  RbxArray array;
  ReadFromFile(&array);
  {
    EXPECT_EQ(string(1000, 'a'), GetTestStr(array, 0));
    EXPECT_EQ("", GetTestStr(array, 1));
    EXPECT_EQ("", GetTestStr(array, 2));
    EXPECT_EQ("", GetTestStr(array, 3));
    EXPECT_EQ("a", GetTestStr(array, 4));
  }
}

TEST_F(RbxArrayTest, RandomTest) {
  const int kTestSize = 1000;
  vector<string> inserted;
  {
    srand(0);
    RbxArrayBuilder builder;
    for (size_t i = 0; i < kTestSize; ++i) {
      const int len = static_cast<int>(
          1.0 * 10000 * rand() / (RAND_MAX + 1.0));
      const string key = string(len, 'a');
      PushTestStr(key, &builder);
      inserted.push_back(key);
    }
    builder.Build();
    WriteToFile(builder);
  }

  RbxArray array;
  ReadFromFile(&array);
  {
    for (size_t i = 0; i < inserted.size(); ++i) {
      EXPECT_EQ(inserted[i], GetTestStr(array, i));
    }
  }
}
}  // namespace
}  // namespace rx
}  // namespace mozc

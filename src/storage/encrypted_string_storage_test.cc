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

#include "base/scoped_ptr.h"
#include "base/util.h"
#include "base/file_stream.h"
#include "storage/encrypted_string_storage.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace storage {

class EncryptedStringStorageTest : public testing::Test {
 protected:
  void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    filename_ = Util::JoinPath(Util::GetUserProfileDirectory(),
                               "encrypted_string_storage_for_test.db");

    storage_.reset(new EncryptedStringStorage(filename_));
  }

  string filename_;
  scoped_ptr<EncryptedStringStorage> storage_;
};

TEST_F(EncryptedStringStorageTest, SaveAndLoad) {
  const char *kData = "abcdefghijklmnopqrstuvwxyz";
  ASSERT_TRUE(storage_->Save(kData));

  string output;
  ASSERT_TRUE(storage_->Load(&output));

  EXPECT_EQ(kData, output);
}

TEST_F(EncryptedStringStorageTest, Encrypt) {
  const string original_data = "abcdefghijklmnopqrstuvwxyz";
  ASSERT_TRUE(storage_->Save(original_data));

  InputFileStream ifs(filename_.c_str(), (ios::in | ios::binary));
  const size_t kBufSize = 128;
  char buf[kBufSize];
  // |ifs.readsome(buf, kBufSize)| returns 0 on Visual C++ because
  // |ifs.rdbuf()->in_avail()| is still 0 just after the file is opened.
  // So we use 'read' and 'gcount' instead.
  ifs.read(buf, kBufSize);
  const size_t read_size = ifs.gcount();
  ifs.peek();
  ASSERT_TRUE(ifs.eof());
  const string result(buf, read_size);

  // Saved stirng is longer than original string since it has some data
  // used for encryption.
  EXPECT_LT(original_data.size(), result.size());
  EXPECT_TRUE(result.find(original_data) == string::npos);
}

}  // namespace storage
}  // namespace mozc

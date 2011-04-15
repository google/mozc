// Copyright 2010-2011, Google Inc.
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

#include <string.h>
#include "base/file_stream.h"
#include "base/mmap.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

TEST(MmapTest, MmapTest) {
  const string filename = Util::JoinPath(FLAGS_test_tmpdir, "test.db");

  const size_t kFileNameSize[] = { 1, 100, 1024, 8192 };

  for (int i = 0; i < arraysize(kFileNameSize); ++i) {
    Util::Unlink(filename);
    scoped_array<char> buf(new char[kFileNameSize[i]]);

    memset(buf.get(), 0, kFileNameSize[i]);

    {
      OutputFileStream ofs(filename.c_str(),
                           ios::out | ios::binary);
      EXPECT_TRUE(ofs);
      ofs.write(buf.get(), kFileNameSize[i]);
    }

    EXPECT_TRUE(Util::GetSecureRandomSequence(buf.get(),
                                              kFileNameSize[i]));

    // Write Test
    {
      Mmap<char> mmap;
      EXPECT_TRUE(mmap.Open(filename.c_str(), "r+"));
      memcpy(mmap.begin(), buf.get(), kFileNameSize[i]);

      for (int j = 0; j < kFileNameSize[i]; ++j) {
        EXPECT_EQ(buf[j], mmap[j]);
      }

      memset(mmap.begin(), 0, kFileNameSize[i]);

      for (int j = 0; j < kFileNameSize[i]; ++j) {
        EXPECT_EQ(0, mmap[j]);
      }

      for (int j = 0; j < kFileNameSize[i]; ++j) {
        mmap[j] = buf[j];
      }

      for (int j = 0; j < kFileNameSize[i]; ++j) {
        EXPECT_EQ(buf[j], mmap[j]);
      }
    }

    // Read test
    {
      Mmap<char> mmap;
      EXPECT_TRUE(mmap.Open(filename.c_str(), "r"));
      for (int j = 0; j < kFileNameSize[i]; ++j) {
        EXPECT_EQ(buf[j], mmap[j]);
      }
    }

    Util::Unlink(filename);
  }
}
}  // namespace
}  // namespace mozc

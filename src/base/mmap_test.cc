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

#include "base/mmap.h"

#include <cstring>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "testing/gmock.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "absl/algorithm/container.h"
#include "absl/flags/flag.h"
#include "absl/random/random.h"

namespace mozc {
namespace {

TEST(MmapTest, MmapTest) {
  const std::string filename =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test.db");
  absl::BitGen gen;

  constexpr size_t kFileNameSize[] = {1, 100, 1024, 8192};
  for (int i = 0; i < std::size(kFileNameSize); ++i) {
    ASSERT_OK(FileUtil::UnlinkIfExists(filename));
    std::vector<char> buf(kFileNameSize[i]);
    ASSERT_OK(FileUtil::SetContents(
        filename, absl::string_view(buf.data(), kFileNameSize[i])));
    absl::c_generate(
        buf, [&gen]() -> char { return absl::Uniform<unsigned char>(gen); });

    // Write Test
    {
      Mmap mmap;
      EXPECT_TRUE(mmap.Open(filename, "r+"));
      memcpy(mmap.begin(), buf.data(), kFileNameSize[i]);

      for (int j = 0; j < kFileNameSize[i]; ++j) {
        EXPECT_EQ(mmap[j], buf[j]);
      }

      memset(mmap.begin(), 0, kFileNameSize[i]);

      for (int j = 0; j < kFileNameSize[i]; ++j) {
        EXPECT_EQ(mmap[j], 0);
      }

      for (int j = 0; j < kFileNameSize[i]; ++j) {
        mmap[j] = buf[j];
      }

      for (int j = 0; j < kFileNameSize[i]; ++j) {
        EXPECT_EQ(mmap[j], buf[j]);
      }
    }

    // Read test
    {
      Mmap mmap;
      EXPECT_TRUE(mmap.Open(filename, "r"));
      for (int j = 0; j < kFileNameSize[i]; ++j) {
        EXPECT_EQ(mmap[j], buf[j]);
      }
    }

    EXPECT_OK(FileUtil::Unlink(filename));
  }
}

TEST(MmapTest, MaybeMLockTest) {
  const size_t data_len = 32;
  std::unique_ptr<void, void (*)(void*)> addr(malloc(data_len), &free);
  if (Mmap::IsMLockSupported()) {
    ASSERT_EQ(Mmap::MaybeMLock(addr.get(), data_len), 0);
    EXPECT_EQ(Mmap::MaybeMUnlock(addr.get(), data_len), 0);
  } else {
    EXPECT_EQ(Mmap::MaybeMLock(addr.get(), data_len), -1);
    EXPECT_EQ(Mmap::MaybeMUnlock(addr.get(), data_len), -1);
  }
}

}  // namespace
}  // namespace mozc

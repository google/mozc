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

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/random/random.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

std::vector<char> GetRandomContents(size_t size) {
  std::vector<char> data(size);
  absl::BitGen gen;
  absl::c_generate(
      data, [&gen]() -> char { return absl::Uniform<unsigned char>(gen); });
  return data;
}

TEST(MmapTest, DefaultCtor) {
  Mmap mmap;  // Nothing is mapped.
  EXPECT_TRUE(mmap.empty());
}

TEST(MmapTest, MoveCtor) {
  constexpr absl::string_view kTestContents = "mmap test";

  const absl::StatusOr<TempFile> temp_file =
      TempDirectory::Default().CreateTempFile();
  ASSERT_OK(temp_file);
  ASSERT_OK(FileUtil::SetContents(temp_file->path(), kTestContents));

  absl::StatusOr<Mmap> mmap = Mmap::Map(temp_file->path());
  ASSERT_OK(mmap);
  EXPECT_EQ(mmap->span(), kTestContents);

  Mmap moved(*std::move(mmap));
  EXPECT_EQ(moved.span(), kTestContents);
  EXPECT_TRUE(mmap->span().empty());  // NOLINT
}

TEST(MmapTest, MoveAssign) {
  constexpr absl::string_view kTestContents[2] = {"mmap test 1", "mmap test 2"};

  const absl::StatusOr<TempFile> temp_files[] = {
      TempDirectory::Default().CreateTempFile(),
      TempDirectory::Default().CreateTempFile()};
  for (size_t i = 0; i < 2; ++i) {
    ASSERT_OK(temp_files[i]);
    ASSERT_OK(FileUtil::SetContents(temp_files[i]->path(), kTestContents[i]));
  }

  absl::StatusOr<Mmap> mmap1 = Mmap::Map(temp_files[0]->path());
  ASSERT_OK(mmap1);
  EXPECT_EQ(mmap1->span(), kTestContents[0]);

  absl::StatusOr<Mmap> mmap2 = Mmap::Map(temp_files[1]->path());
  ASSERT_OK(mmap2);
  EXPECT_EQ(mmap2->span(), kTestContents[1]);

  *mmap1 = *std::move(mmap2);
  EXPECT_EQ(mmap1->span(), kTestContents[1]);
  EXPECT_TRUE(mmap2->span().empty());  // NOLINT
}

TEST(MmapTest, FailsIfFileDoesNotExist) {
  const absl::StatusOr<Mmap> mmap = Mmap::Map("");
  EXPECT_FALSE(mmap.ok());
}

TEST(MmapTest, FailsIfOffsetExceedsFileSize) {
  constexpr size_t kFileSize = 128;
  const absl::StatusOr<TempFile> temp_file =
      TempDirectory::Default().CreateTempFile();
  ASSERT_OK(temp_file);
  ASSERT_OK(
      FileUtil::SetContents(temp_file->path(), std::string(kFileSize, 'a')));
  EXPECT_FALSE(
      Mmap::Map(temp_file->path(), 512, std::nullopt, Mmap::READ_ONLY).ok());
}

TEST(MmapTest, FailsIfMapSizeIsZero) {
  constexpr size_t kFileSize = 128;
  const absl::StatusOr<TempFile> temp_file =
      TempDirectory::Default().CreateTempFile();
  ASSERT_OK(temp_file);
  ASSERT_OK(
      FileUtil::SetContents(temp_file->path(), std::string(kFileSize, 'a')));

  EXPECT_FALSE(Mmap::Map(temp_file->path(), 0, 0, Mmap::READ_ONLY).ok());
  EXPECT_FALSE(Mmap::Map(temp_file->path(), 100, 0, Mmap::READ_ONLY).ok());

  // If offset is at the end of file, the resulting size is zero.
  EXPECT_FALSE(
      Mmap::Map(temp_file->path(), kFileSize, std::nullopt, Mmap::READ_ONLY)
          .ok());
}

TEST(MmapTest, MaybeMLockTest) {
  constexpr size_t kDataLen = 32;
  std::unique_ptr<void, void (*)(void*)> addr(malloc(kDataLen), &free);
  if (Mmap::IsMLockSupported()) {
    ASSERT_EQ(Mmap::MaybeMLock(addr.get(), kDataLen), 0);
    EXPECT_EQ(Mmap::MaybeMUnlock(addr.get(), kDataLen), 0);
  } else {
    EXPECT_EQ(Mmap::MaybeMLock(addr.get(), kDataLen), -1);
    EXPECT_EQ(Mmap::MaybeMUnlock(addr.get(), kDataLen), -1);
  }
}

class MmapEntireFileTest : public ::testing::TestWithParam<size_t> {};

TEST_P(MmapEntireFileTest, Read) {
  // Create a file with random contents.
  const size_t filesize = GetParam();
  const std::vector<char> data = GetRandomContents(filesize);
  const absl::StatusOr<TempFile> temp_file =
      TempDirectory::Default().CreateTempFile();
  ASSERT_OK(temp_file);
  ASSERT_OK(FileUtil::SetContents(temp_file->path(),
                                  absl::string_view(data.data(), data.size())));

  // Mmap the file and check its contents.
  const absl::StatusOr<Mmap> mmap =
      Mmap::Map(temp_file->path(), Mmap::READ_ONLY);
  ASSERT_OK(mmap);
  EXPECT_EQ(mmap->span(), data);
}

TEST_P(MmapEntireFileTest, Write) {
  // Create a file with full of a's.
  const size_t filesize = GetParam();
  const absl::StatusOr<TempFile> temp_file =
      TempDirectory::Default().CreateTempFile();
  ASSERT_OK(temp_file);
  ASSERT_OK(
      FileUtil::SetContents(temp_file->path(), std::string(filesize, 'a')));

  // Mmap the file and writes random contents.
  const std::vector<char> data = GetRandomContents(filesize);
  {
    absl::StatusOr<Mmap> mmap = Mmap::Map(temp_file->path(), Mmap::READ_WRITE);
    ASSERT_OK(mmap);
    ASSERT_EQ(mmap->size(), data.size());
    absl::c_copy(data, mmap->begin());
  }

  // Read the file and check its contents.
  absl::StatusOr<std::string> contents =
      FileUtil::GetContents(temp_file->path());
  ASSERT_OK(contents);
  EXPECT_EQ(*contents, absl::string_view(data.data(), data.size()));
}

INSTANTIATE_TEST_SUITE_P(MmapTestSuite, MmapEntireFileTest,
                         ::testing::Values(1, 8, 1024, 4096, 7777, 8192));

// File size, offset, and length to mmap.
using Params = std::tuple<size_t, size_t, std::optional<size_t>>;

class MmapPartialFileTest : public ::testing::TestWithParam<Params> {};

TEST_P(MmapPartialFileTest, Read) {
  const auto [filesize, offset, size] = GetParam();

  const std::vector<char> data = GetRandomContents(filesize);
  const absl::StatusOr<TempFile> temp_file =
      TempDirectory::Default().CreateTempFile();
  ASSERT_OK(temp_file);
  ASSERT_OK(FileUtil::SetContents(temp_file->path(),
                                  absl::string_view(data.data(), data.size())));

  const absl::string_view expected(data.data() + offset,
                                   size.value_or(filesize - offset));
  absl::StatusOr<Mmap> mmap =
      Mmap::Map(temp_file->path(), offset, size, Mmap::READ_ONLY);
  ASSERT_OK(mmap) << mmap.status();
  EXPECT_EQ(mmap->span(), expected);

  // Test move operations for partial mmap.
  Mmap mmap2(*std::move(mmap));  // Move constructed.
  EXPECT_EQ(mmap2.span(), expected);

  Mmap mmap3;
  EXPECT_TRUE(mmap3.empty());
  mmap3 = std::move(mmap2);  // Move assigned.
  EXPECT_EQ(mmap3.span(), expected);
}

TEST_P(MmapPartialFileTest, Write) {
  const auto [filesize, offset, size] = GetParam();
  const size_t map_size = size.value_or(filesize - offset);

  const absl::StatusOr<TempFile> temp_file =
      TempDirectory::Default().CreateTempFile();
  ASSERT_OK(temp_file);
  ASSERT_OK(
      FileUtil::SetContents(temp_file->path(), std::string(filesize, 'a')));

  const std::vector<char> data = GetRandomContents(map_size);
  {
    absl::StatusOr<Mmap> mmap =
        Mmap::Map(temp_file->path(), offset, map_size, Mmap::READ_WRITE);
    ASSERT_OK(mmap) << mmap.status();
    ASSERT_EQ(mmap->size(), data.size());
    absl::c_copy(data, mmap->begin());
  }
  absl::StatusOr<std::string> contents =
      FileUtil::GetContents(temp_file->path());
  ASSERT_OK(contents);
  EXPECT_EQ(contents->substr(0, offset), std::string(offset, 'a'));
  EXPECT_EQ(contents->substr(offset, map_size),
            absl::string_view(data.data(), data.size()));
  EXPECT_EQ(contents->substr(offset + map_size),
            std::string(filesize - offset - map_size, 'a'));
}

INSTANTIATE_TEST_SUITE_P(MmapTestSuite, MmapPartialFileTest,
                         ::testing::Values(Params(1, 0, 1),                   //
                                           Params(1024, 0, 1024),             //
                                           Params(1024, 0, 321),              //
                                           Params(1024, 1000, std::nullopt),  //
                                           Params(1024, 1000, 24),            //
                                           Params(1024, 321, 567),            //
                                           Params(4096, 0, 4096),             //
                                           Params(4096, 3000, 1096),          //
                                           Params(4096, 500, std::nullopt),   //
                                           Params(4096, 500, 3000),           //
                                           Params(7777, 0, 7777),             //
                                           Params(7777, 2500, 5000),          //
                                           Params(7777, 5000, std::nullopt),  //
                                           Params(7777, 5000, 2000),          //
                                           Params(8192, 0, 8192),             //
                                           Params(8192, 1000, 3000),          //
                                           Params(8192, 1000, 7000),          //
                                           Params(8192, 5000, std::nullopt),  //
                                           Params(8192, 5000, 2000),          //
                                           Params(8192, 5000, 3192)));

}  // namespace
}  // namespace mozc

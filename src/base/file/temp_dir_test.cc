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

#include "base/file/temp_dir.h"

#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/environ_mock.h"
#include "base/file_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::StartsWith;

MATCHER(FileExists, "") { return FileUtil::FileExists(arg).ok(); }

class TempDirectoryTest : public ::testing::Test {
 protected:
  ~TempDirectoryTest() override {
    for (const std::string &path : paths_) {
      EXPECT_THAT(path, Not(FileExists()));
    }
  }

  TempFile CreateFile(const TempDirectory &t) {
    absl::StatusOr<TempFile> f = t.CreateTempFile();
    EXPECT_OK(f) << t.path();
    LOG(INFO) << "CreateFile created: " << f->path();
    EXPECT_THAT(f->path(), StartsWith(t.path()));
    EXPECT_NE(f->path(), t.path());
    paths_.push_back(f->path());
    return *std::move(f);
  }

  TempDirectory CreateDirectory(const TempDirectory &t) {
    absl::StatusOr<TempDirectory> d = t.CreateTempDirectory();
    EXPECT_OK(d) << t.path();
    LOG(INFO) << "CreateDirectory created: " << d->path();
    EXPECT_THAT(d->path(), StartsWith(t.path()));
    EXPECT_NE(d->path(), t.path());
    paths_.push_back(d->path());
    return *std::move(d);
  }

  std::vector<std::string> paths_;
};

TEST_F(TempDirectoryTest, Default) {
  const TempDirectory temp_dir = TempDirectory::Default();
  EXPECT_THAT(temp_dir.path(), Not(IsEmpty()));

  // Try to write something to a temporary file.
  constexpr absl::string_view kTestContent = "testing temp dir";
  TempFile temp_file = CreateFile(temp_dir);
  EXPECT_OK(FileUtil::SetContents(temp_file.path(), kTestContent));
  absl::StatusOr<std::string> content = FileUtil::GetContents(temp_file.path());
  EXPECT_OK(content);
  EXPECT_EQ(*content, kTestContent);
  std::ofstream of1(FileUtil::JoinPath(temp_dir.path(), "test"));
  EXPECT_TRUE(of1.good());

  // Create some more directories and files.
  TempDirectory nested_dir = CreateDirectory(temp_dir);
  TempFile nested1 = CreateFile(nested_dir);
  TempFile nested2 = CreateFile(nested_dir);
  std::string dir_to_keep_path;
  {
    // Test keep().
    TempDirectory dir_to_keep = CreateDirectory(nested_dir);
    dir_to_keep_path = dir_to_keep.path();
    EXPECT_FALSE(dir_to_keep.keep());
    dir_to_keep.set_keep(true);
    EXPECT_TRUE(dir_to_keep.keep());
  }
  // This directory should continue to exist after going out of scope.
  EXPECT_OK(FileUtil::DirectoryExists(dir_to_keep_path));

  std::ofstream of2(FileUtil::JoinPath(nested_dir.path(), "test"));
  EXPECT_TRUE(of2.good());

  TempDirectory nested_nested_dir = CreateDirectory(nested_dir);
  TempFile nested_nested = CreateFile(nested_nested_dir);
}

TEST_F(TempDirectoryTest, TestTmpDirEnv) {
  // Set a random temp directory to TEST_TMPDIR.
  const absl::StatusOr<TempDirectory> temp_dir = TempDirectory::Default();
  EXPECT_OK(temp_dir);
  const TempDirectory test_dir = CreateDirectory(*temp_dir);

  EnvironMock env_mock;
  {
    env_mock.SetEnv("TEST_TMPDIR", test_dir.path());
    absl::StatusOr<TempDirectory> new_temp_dir = TempDirectory::Default();
    EXPECT_OK(new_temp_dir);
    EXPECT_EQ(new_temp_dir->path(), test_dir.path());
  }
  // Check that the destructor doesn't delete the directory when constructed by
  // Default().
  EXPECT_OK(FileUtil::DirectoryExists(test_dir.path()));

  // Clear TEST_TMPDIR so we can test other code paths.
  env_mock.SetEnv("TEST_TMPDIR", "");
  absl::StatusOr<TempDirectory> system_temp_dir = TempDirectory::Default();
  EXPECT_OK(system_temp_dir);
  EXPECT_THAT(system_temp_dir->path(), Not(IsEmpty()));
}

}  // namespace
}  // namespace mozc

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

#include "base/file/recursive.h"

#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc::file {
namespace {

class RecursiveTest : public testing::Test {
 protected:
  TempDirectory CreateTempDirectory(const TempDirectory &t) {
    absl::StatusOr<TempDirectory> d = t.CreateTempDirectory();
    EXPECT_OK(d);
    return *std::move(d);
  }
};

TEST_F(RecursiveTest, DeleteRecursively) {
  TempDirectory temp = TempDirectory::Default();

  TempDirectory dir = CreateTempDirectory(temp);
  {
    std::ofstream of1(FileUtil::JoinPath(dir.path(), "file1"));
    std::ofstream of2(FileUtil::JoinPath(dir.path(), "file2"));
  }

  TempDirectory dir2 = CreateTempDirectory(dir);
  {
    std::ofstream of1(FileUtil::JoinPath(dir2.path(), "file1"));
    std::ofstream of2(FileUtil::JoinPath(dir2.path(), "file2"));
  }

  EXPECT_OK(DeleteRecursively(dir.path()));
  EXPECT_TRUE(absl::IsNotFound(FileUtil::DirectoryExists(dir.path())));
}

TEST_F(RecursiveTest, DeleteRecursivelyNonExistent) {
  TempDirectory temp = TempDirectory::Default();
  TempDirectory dir = CreateTempDirectory(temp);
  // Path leaf that doesn't exist.
  EXPECT_OK(DeleteRecursively(FileUtil::JoinPath(dir.path(), "non_existent")));
  // Path component that doesn't exist.
  EXPECT_OK(DeleteRecursively(
      FileUtil::JoinPath({dir.path(), "non_existent", "non_existent"})));
}

}  // namespace
}  // namespace mozc::file

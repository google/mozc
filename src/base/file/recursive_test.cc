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

#include <utility>

#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "absl/status/statusor.h"

namespace mozc::file {
namespace {

TempFile CreateTempFile(const TempDirectory &t) {
  absl::StatusOr<TempFile> f = t.CreateTempFile();
  EXPECT_OK(f);
  return *std::move(f);
}

TempDirectory CreateTempDirectory(const TempDirectory &t) {
  absl::StatusOr<TempDirectory> d = t.CreateTempDirectory();
  EXPECT_OK(d);
  return *std::move(d);
}

TEST(RecursiveTest, DeleteRecursively) {
  TempDirectory temp = TempDirectory::Default();

  TempDirectory dir = CreateTempDirectory(temp);
  CreateTempFile(dir);
  CreateTempFile(dir);
  CreateTempFile(dir);

  TempDirectory dir2 = CreateTempDirectory(dir);
  CreateTempFile(dir2);

  TempDirectory dir3 = CreateTempDirectory(dir2);
  CreateTempFile(dir3);

  EXPECT_OK(DeleteRecursively(dir.path()));
  EXPECT_FALSE(FileUtil::DirectoryExists(dir.path()).ok());
  // Path leaf doesn't exist.
  EXPECT_OK(DeleteRecursively(dir.path()));
  // Path component doesn't exist.
  EXPECT_OK(DeleteRecursively(dir3.path()));
}

}  // namespace
}  // namespace mozc::file

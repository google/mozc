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

#include "base/file_util_mock.h"

#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

// The following are helpers to verify that the status of absl::StatusOr<bool>
// is OK and its value is true/false.
MATCHER(IsOkAndTrue, negation ? "is not OK or false" : "is OK and true") {
  return arg.ok() && *arg;
}
MATCHER(IsOkAndFalse, negation ? "is not OK or true" : "is OK and false") {
  return arg.ok() && !*arg;
}

#define EXPECT_OK_AND_TRUE(expr) EXPECT_THAT(expr, ::mozc::IsOkAndTrue());
#define EXPECT_OK_AND_FALSE(expr) EXPECT_THAT(expr, ::mozc::IsOkAndFalse());

TEST(FileUtilMockTest, DirectoryMockTests) {
  FileUtilMock mock;

  EXPECT_OK(FileUtil::CreateDirectory("/tmp/mozc"));
  EXPECT_OK(FileUtil::RemoveDirectory("/tmp/mozc"));
  EXPECT_FALSE(FileUtil::DirectoryExists("/tmp/no_mozc").ok());
  EXPECT_FALSE(FileUtil::DirectoryExists("/tmp/").ok());  // Limitation of mock.
}

TEST(FileUtilMockTest, FileMockTests) {
  FileUtilMock mock;

  mock.CreateFile("/mozc/file.txt");
  EXPECT_OK(FileUtil::Unlink("/mozc/file.txt"));
  EXPECT_FALSE(FileUtil::FileExists("/mozc/file.txt").ok());

  mock.CreateFile("/mozc/file1.txt");
  mock.CreateFile("/mozc/file2.txt");
  EXPECT_OK_AND_FALSE(
      FileUtil::IsEqualFile("/mozc/file1.txt", "/mozc/file2.txt"));

  EXPECT_OK(FileUtil::CopyFile("/mozc/file2.txt", "/mozc/file3.txt"));
  EXPECT_OK_AND_TRUE(
      FileUtil::IsEqualFile("/mozc/file2.txt", "/mozc/file3.txt"));

  EXPECT_OK(FileUtil::AtomicRename("/mozc/file3.txt", "/mozc/file4.txt"));
  EXPECT_FALSE(FileUtil::FileExists("/mozc/file3.txt").ok());
  EXPECT_OK(FileUtil::FileExists("/mozc/file4.txt"));
  EXPECT_OK_AND_TRUE(
      FileUtil::IsEqualFile("/mozc/file2.txt", "/mozc/file4.txt"));

  absl::StatusOr<FileTimeStamp> time1 =
      FileUtil::GetModificationTime("/mozc/file1.txt");
  ASSERT_OK(time1);
  absl::StatusOr<FileTimeStamp> time2 =
      FileUtil::GetModificationTime("/mozc/file2.txt");
  ASSERT_OK(time2);
  EXPECT_NE(*time1, *time2);

  EXPECT_FALSE(FileUtil::GetModificationTime("/mozc/file3.txt").ok());
  absl::StatusOr<FileTimeStamp> time4 =
      FileUtil::GetModificationTime("/mozc/file4.txt");
  ASSERT_OK(time4);
  EXPECT_EQ(*time2, *time4);
}

TEST(FileUtilMockTest, HardLinkTests) {
  FileUtilMock mock;

  // # Hard link for files.
  EXPECT_OK_AND_TRUE(
      FileUtil::IsEquivalent("/mozc/file1.txt", "/mozc/file1.txt"));
  EXPECT_OK_AND_FALSE(
      FileUtil::IsEquivalent("/mozc/file1.txt", "/mozc/file2.txt"));

  // file1 does not exist.
  EXPECT_FALSE(
      FileUtil::CreateHardLink("/mozc/file1.txt", "/mozc/file2.txt").ok());

  mock.CreateFile("/mozc/file1.txt");
  EXPECT_OK(FileUtil::CreateHardLink("/mozc/file1.txt", "/mozc/file2.txt"));
  EXPECT_OK_AND_TRUE(
      FileUtil::IsEquivalent("/mozc/file1.txt", "/mozc/file2.txt"));

  // file2 already exists.
  EXPECT_FALSE(
      FileUtil::CreateHardLink("/mozc/file1.txt", "/mozc/file2.txt").ok());
  EXPECT_OK_AND_TRUE(
      FileUtil::IsEquivalent("/mozc/file1.txt", "/mozc/file2.txt"));

  // # Hard link for directories.
  EXPECT_OK_AND_TRUE(FileUtil::IsEquivalent("/mozc/dir1", "/mozc/dir1"));
  EXPECT_OK_AND_FALSE(FileUtil::IsEquivalent("/mozc/dir1", "/mozc/dir2"));

  // dir1 does not exist.
  EXPECT_FALSE(FileUtil::CreateHardLink("/mozc/dir1", "/mozc/dir2").ok());

  EXPECT_OK(FileUtil::CreateDirectory("/mozc/dir1"));
  EXPECT_OK(FileUtil::CreateHardLink("/mozc/dir1", "/mozc/dir2"));
  EXPECT_OK_AND_TRUE(FileUtil::IsEquivalent("/mozc/dir1", "/mozc/dir2"));

  // dir2 already exists.
  EXPECT_FALSE(FileUtil::CreateHardLink("/mozc/dir1", "/mozc/dir2").ok());
  EXPECT_OK_AND_TRUE(FileUtil::IsEquivalent("/mozc/dir1", "/mozc/dir2"));
}

TEST(FileUtilMockTest, IsEquivalentTests) {
  FileUtilMock mock;
  constexpr char kFile1[] = "/mozc/file1.txt";
  constexpr char kFile2[] = "/mozc/file2.txt";

  EXPECT_OK_AND_TRUE(FileUtil::IsEquivalent(kFile1, kFile1));
  EXPECT_OK_AND_FALSE(FileUtil::IsEquivalent(kFile1, kFile2));

  mock.CreateFile(kFile1);
  EXPECT_OK_AND_TRUE(FileUtil::IsEquivalent(kFile1, kFile1));
  EXPECT_FALSE(FileUtil::IsEquivalent(kFile1, kFile2).ok());

  EXPECT_OK(FileUtil::CreateHardLink(kFile1, kFile2));
  EXPECT_OK_AND_TRUE(FileUtil::IsEquivalent(kFile1, kFile2));
}

}  // namespace
}  // namespace mozc

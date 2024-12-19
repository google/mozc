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

#include "base/file_util.h"

#include <ios>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/file/temp_dir.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

#ifdef _WIN32
#include <windows.h>

#include "base/win32/wide_char.h"
#endif  // _WIN32

// Ad-hoc workadound against macro problem on Windows.
// On Windows, following macros, defined when you include <windows.h>,
// should be removed here because they affects the method name definition of
// Util class.
// TODO(yukawa): Use different method name if applicable.
#undef CreateDirectory
#undef RemoveDirectory
#undef CopyFile

namespace mozc {
namespace {

#define CreateTestFile(filename, data) \
  ASSERT_OK(::mozc::FileUtil::SetContents(filename, data))

TEST(FileUtilTest, CreateDirectory) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  EXPECT_OK(FileUtil::DirectoryExists(temp_dir.path()));
  // dirpath = FLAGS_test_tmpdir/testdir
  const std::string dirpath = FileUtil::JoinPath(temp_dir.path(), "testdir");

  // Delete dirpath, if it exists.
  ASSERT_OK(FileUtil::RemoveDirectoryIfExists(dirpath));
  ASSERT_FALSE(FileUtil::FileExists(dirpath).ok());

  // Create the directory.
  EXPECT_OK(FileUtil::CreateDirectory(dirpath));
  EXPECT_OK(FileUtil::DirectoryExists(dirpath));

#if !defined(_WIN32)
  // On Windows, CreateDirectory does not return OK if the directory already
  // exists. See the implementation of CreateDirectory in file_util.cc.
  // https://github.com/google/mozc/issues/1076
  //
  // Create the same directory again.
  EXPECT_OK(FileUtil::CreateDirectory(dirpath));
#endif  // !_WIN32

  // Delete the directory.
  ASSERT_OK(FileUtil::RemoveDirectory(dirpath));
  ASSERT_FALSE(FileUtil::FileExists(dirpath).ok());
}

TEST(FileUtilTest, DirectoryExists) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  EXPECT_OK(FileUtil::DirectoryExists(temp_dir.path()));
  const std::string filepath = FileUtil::JoinPath(temp_dir.path(), "testfile");

  // Delete filepath, if it exists.
  ASSERT_OK(FileUtil::UnlinkIfExists(filepath));
  ASSERT_FALSE(FileUtil::FileExists(filepath).ok());

  // Create a file.
  CreateTestFile(filepath, "test data");
  EXPECT_OK(FileUtil::FileExists(filepath));
  EXPECT_FALSE(FileUtil::DirectoryExists(filepath).ok());

  // Delete the file.
  ASSERT_OK(FileUtil::Unlink(filepath));
  ASSERT_FALSE(FileUtil::FileExists(filepath).ok());
}

TEST(FileUtilTest, Unlink) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string filepath = FileUtil::JoinPath(temp_dir.path(), "testfile");
  ASSERT_OK(FileUtil::UnlinkIfExists(filepath));
  EXPECT_FALSE(FileUtil::FileExists(filepath).ok());

  CreateTestFile(filepath, "simple test");
  EXPECT_OK(FileUtil::FileExists(filepath));
  EXPECT_OK(FileUtil::Unlink(filepath));
  EXPECT_FALSE(FileUtil::FileExists(filepath).ok());

#ifdef _WIN32
  constexpr DWORD kTestAttributeList[] = {
      FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_HIDDEN,
      FILE_ATTRIBUTE_NORMAL,  FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
      FILE_ATTRIBUTE_OFFLINE, FILE_ATTRIBUTE_READONLY,
      FILE_ATTRIBUTE_SYSTEM,  FILE_ATTRIBUTE_TEMPORARY,
  };

  const std::wstring wfilepath = win32::Utf8ToWide(filepath);
  for (size_t i = 0; i < std::size(kTestAttributeList); ++i) {
    SCOPED_TRACE(absl::StrFormat("AttributeTest %zd", i));
    CreateTestFile(filepath, "attribute_test");
    EXPECT_NE(FALSE,
              ::SetFileAttributesW(wfilepath.c_str(), kTestAttributeList[i]));
    EXPECT_OK(FileUtil::FileExists(filepath));
    EXPECT_OK(FileUtil::Unlink(filepath));
    EXPECT_FALSE(FileUtil::FileExists(filepath).ok());
  }
#endif  // _WIN32

  EXPECT_OK(FileUtil::UnlinkIfExists(filepath));
}

#ifdef _WIN32
TEST(FileUtilTest, HideFile) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string filename = FileUtil::JoinPath(temp_dir.path(), "testfile");

  EXPECT_FALSE(FileUtil::HideFile(filename));

  const std::wstring wfilename = win32::Utf8ToWide(filename);

  CreateTestFile(filename, "test data");
  EXPECT_OK(FileUtil::FileExists(filename));

  EXPECT_NE(FALSE,
            ::SetFileAttributesW(wfilename.c_str(), FILE_ATTRIBUTE_NORMAL));
  EXPECT_TRUE(FileUtil::HideFile(filename));
  EXPECT_EQ(::GetFileAttributesW(wfilename.c_str()),
            FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);

  EXPECT_NE(::SetFileAttributesW(wfilename.c_str(), FILE_ATTRIBUTE_ARCHIVE),
            FALSE);
  EXPECT_TRUE(FileUtil::HideFile(filename));
  EXPECT_EQ(::GetFileAttributesW(wfilename.c_str()),
            FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_ATTRIBUTE_ARCHIVE);

  EXPECT_NE(::SetFileAttributesW(wfilename.c_str(), FILE_ATTRIBUTE_NORMAL),
            FALSE);
  EXPECT_TRUE(FileUtil::HideFileWithExtraAttributes(filename,
                                                    FILE_ATTRIBUTE_TEMPORARY));
  EXPECT_EQ(::GetFileAttributesW(wfilename.c_str()),
            FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_ATTRIBUTE_TEMPORARY);

  EXPECT_NE(FALSE,
            ::SetFileAttributesW(wfilename.c_str(), FILE_ATTRIBUTE_ARCHIVE));
  EXPECT_TRUE(FileUtil::HideFileWithExtraAttributes(filename,
                                                    FILE_ATTRIBUTE_TEMPORARY));
  EXPECT_EQ(::GetFileAttributesW(wfilename.c_str()),
            FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_ATTRIBUTE_ARCHIVE |
                FILE_ATTRIBUTE_TEMPORARY);
}
#endif  // _WIN32

TEST(FileUtilTest, IsEqualFile) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string filename1 = FileUtil::JoinPath(temp_dir.path(), "test1");
  const std::string filename2 = FileUtil::JoinPath(temp_dir.path(), "test2");

  EXPECT_FALSE(FileUtil::IsEqualFile(filename1, filename2).ok());

  CreateTestFile(filename1, "test data1");
  EXPECT_FALSE(FileUtil::IsEqualFile(filename1, filename2).ok());

  CreateTestFile(filename2, "test data1");
  absl::StatusOr<bool> s = FileUtil::IsEqualFile(filename1, filename2);
  EXPECT_OK(s);
  EXPECT_TRUE(*s);

  CreateTestFile(filename2, "test data1 test data1");
  s = FileUtil::IsEqualFile(filename1, filename2);
  EXPECT_OK(s);
  EXPECT_FALSE(*s);

  CreateTestFile(filename2, "test data2");
  s = FileUtil::IsEqualFile(filename1, filename2);
  EXPECT_OK(s);
  EXPECT_FALSE(*s);
}

TEST(FileUtilTest, IsEquivalent) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string filename1 = FileUtil::JoinPath(temp_dir.path(), "test1");
  const std::string filename2 = FileUtil::JoinPath(temp_dir.path(), "test2");
  EXPECT_FALSE(FileUtil::IsEquivalent(filename1, filename1).ok());
  EXPECT_FALSE(FileUtil::IsEquivalent(filename1, filename2).ok());

  CreateTestFile(filename1, "test data");
  absl::StatusOr<bool> s = FileUtil::IsEquivalent(filename1, filename1);
  if (absl::IsUnimplemented(s.status())) {
    return;
  }
  EXPECT_OK(s);
  EXPECT_TRUE(*s);

  // filename2 doesn't exist, so the status is not OK.
  EXPECT_FALSE(FileUtil::IsEquivalent(filename1, filename2).ok());

  // filename2 exists but it's a different file.
  CreateTestFile(filename2, "test data");
  s = FileUtil::IsEquivalent(filename1, filename2);
  EXPECT_OK(s);
  EXPECT_FALSE(*s);
}

TEST(FileUtilTest, CopyFile) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  // just test rename operation works as intended
  const std::string from = FileUtil::JoinPath(temp_dir.path(), "copy_from");
  const std::string to = FileUtil::JoinPath(temp_dir.path(), "copy_to");

  CreateTestFile(from, "simple test");
  EXPECT_OK(FileUtil::CopyFile(from, to));
  absl::StatusOr<bool> s = FileUtil::IsEqualFile(from, to);
  EXPECT_OK(s);
  EXPECT_TRUE(*s);

  CreateTestFile(from, "overwrite test");
  EXPECT_OK(FileUtil::CopyFile(from, to));
  s = FileUtil::IsEqualFile(from, to);
  EXPECT_OK(s);
  EXPECT_TRUE(*s);

#ifdef _WIN32
  struct TestData {
    TestData(DWORD from, DWORD to) : from_attributes(from), to_attributes(to) {}
    const DWORD from_attributes;
    const DWORD to_attributes;
  };
  const TestData kTestDataList[] = {
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_ARCHIVE),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_HIDDEN),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_NORMAL),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_OFFLINE),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_READONLY),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_SYSTEM),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_TEMPORARY),

      TestData(FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_NORMAL),
      TestData(FILE_ATTRIBUTE_NORMAL,
               FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY),
      TestData(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM,
               FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM),
  };

  for (size_t i = 0; i < std::size(kTestDataList); ++i) {
    const std::string test_label =
        "overwrite test with attributes " + std::to_string(i);
    SCOPED_TRACE(test_label);
    CreateTestFile(from, test_label);

    const TestData &kData = kTestDataList[i];
    const std::wstring wfrom = win32::Utf8ToWide(from);
    const std::wstring wto = win32::Utf8ToWide(to);
    EXPECT_NE(FALSE,
              ::SetFileAttributesW(wfrom.c_str(), kData.from_attributes));
    EXPECT_NE(FALSE, ::SetFileAttributesW(wto.c_str(), kData.to_attributes));

    EXPECT_OK(FileUtil::CopyFile(from, to));
    absl::StatusOr<bool> s = FileUtil::IsEqualFile(from, to);
    EXPECT_OK(s);
    EXPECT_TRUE(*s);
    EXPECT_EQ(::GetFileAttributesW(wfrom.c_str()), kData.from_attributes);
    EXPECT_EQ(::GetFileAttributesW(wto.c_str()), kData.from_attributes);

    EXPECT_NE(FALSE,
              ::SetFileAttributesW(wfrom.c_str(), FILE_ATTRIBUTE_NORMAL));
    EXPECT_NE(FALSE, ::SetFileAttributesW(wto.c_str(), FILE_ATTRIBUTE_NORMAL));
  }
#endif  // _WIN32
}

TEST(FileUtilTest, AtomicRename) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  // just test rename operation works as intended
  const std::string from =
      FileUtil::JoinPath(temp_dir.path(), "atomic_rename_test_from");
  const std::string to =
      FileUtil::JoinPath(temp_dir.path(), "atomic_rename_test_to");

  // |from| is not found
  EXPECT_FALSE(FileUtil::AtomicRename(from, to).ok());
  CreateTestFile(from, "test");
  EXPECT_OK(FileUtil::AtomicRename(from, to));

  // from is deleted
  EXPECT_FALSE(FileUtil::FileExists(from).ok());
  EXPECT_OK(FileUtil::FileExists(to));

  {
    absl::StatusOr<std::string> content = FileUtil::GetContents(to);
    ASSERT_OK(content);
    EXPECT_EQ(*content, "test");
  }

  EXPECT_FALSE(FileUtil::AtomicRename(from, to).ok());

  ASSERT_OK(FileUtil::UnlinkIfExists(from));
  ASSERT_OK(FileUtil::UnlinkIfExists(to));

  // overwrite the file
  CreateTestFile(from, "test");
  CreateTestFile(to, "test");
  EXPECT_OK(FileUtil::AtomicRename(from, to));

#ifdef _WIN32
  struct TestData {
    TestData(DWORD from, DWORD to) : from_attributes(from), to_attributes(to) {}
    const DWORD from_attributes;
    const DWORD to_attributes;
  };
  const TestData kTestDataList[] = {
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_ARCHIVE),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_HIDDEN),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_NORMAL),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_OFFLINE),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_READONLY),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_SYSTEM),
      TestData(FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_TEMPORARY),

      TestData(FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_NORMAL),
      TestData(FILE_ATTRIBUTE_NORMAL,
               FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY),
      TestData(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM,
               FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM),
  };

  for (size_t i = 0; i < std::size(kTestDataList); ++i) {
    const std::string test_label =
        "overwrite file with attributes " + std::to_string(i);
    SCOPED_TRACE(test_label);
    CreateTestFile(from, test_label);

    const TestData &kData = kTestDataList[i];
    const std::wstring wfrom = win32::Utf8ToWide(from);
    const std::wstring wto = win32::Utf8ToWide(to);
    EXPECT_NE(FALSE,
              ::SetFileAttributesW(wfrom.c_str(), kData.from_attributes));
    EXPECT_NE(FALSE, ::SetFileAttributesW(wto.c_str(), kData.to_attributes));

    EXPECT_OK(FileUtil::AtomicRename(from, to));
    EXPECT_EQ(::GetFileAttributesW(wto.c_str()), kData.from_attributes);
    EXPECT_FALSE(FileUtil::FileExists(from).ok());
    EXPECT_OK(FileUtil::FileExists(to));

    ::SetFileAttributesW(wfrom.c_str(), FILE_ATTRIBUTE_NORMAL);
    ::SetFileAttributesW(wto.c_str(), FILE_ATTRIBUTE_NORMAL);
  }
#endif  // _WIN32
}

TEST(FileUtilTest, CreateHardLink) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string filename1 = FileUtil::JoinPath(temp_dir.path(), "test1");
  const std::string filename2 = FileUtil::JoinPath(temp_dir.path(), "test2");

  CreateTestFile(filename1, "test data");
  absl::Status s = FileUtil::CreateHardLink(filename1, filename2);
  if (absl::IsUnimplemented(s)) {
    return;
  }
  EXPECT_OK(s);
  absl::StatusOr<bool> equiv = FileUtil::IsEquivalent(filename1, filename2);
  ASSERT_OK(equiv);
  EXPECT_TRUE(*equiv);

  EXPECT_FALSE(FileUtil::CreateHardLink(filename1, filename2).ok());
}

#ifdef _WIN32
#define SP "\\"
#else  // _WIN32
#define SP "/"
#endif  // _WIN32

TEST(FileUtilTest, JoinPath) {
  EXPECT_TRUE(FileUtil::JoinPath({}).empty());
  EXPECT_EQ(FileUtil::JoinPath({"foo"}), "foo");
  EXPECT_EQ(FileUtil::JoinPath({"foo", "bar"}), "foo" SP "bar");
  EXPECT_EQ(FileUtil::JoinPath({"foo", "bar", "baz"}), "foo" SP "bar" SP "baz");

  // Some path components end with delimiter.
  EXPECT_EQ(FileUtil::JoinPath({"foo" SP, "bar", "baz"}),
            "foo" SP "bar" SP "baz");
  EXPECT_EQ(FileUtil::JoinPath({"foo", "bar" SP, "baz"}),
            "foo" SP "bar" SP "baz");
  EXPECT_EQ(FileUtil::JoinPath({"foo", "bar", "baz" SP}),
            "foo" SP "bar" SP "baz" SP);

  // Containing empty strings.
  EXPECT_TRUE(FileUtil::JoinPath({"", "", ""}).empty());
  EXPECT_EQ(FileUtil::JoinPath({"", "foo", "bar"}), "foo" SP "bar");
  EXPECT_EQ(FileUtil::JoinPath({"foo", "", "bar"}), "foo" SP "bar");
  EXPECT_EQ(FileUtil::JoinPath({"foo", "bar", ""}), "foo" SP "bar");
}

TEST(FileUtilTest, Dirname) {
  EXPECT_EQ(FileUtil::Dirname(SP "foo" SP "bar"), SP "foo");
  EXPECT_EQ(FileUtil::Dirname(SP "foo" SP "bar" SP "foo.txt"),
            SP "foo" SP "bar");
  EXPECT_EQ(FileUtil::Dirname("foo.txt"), "");
  EXPECT_EQ(FileUtil::Dirname(SP), "");
}

TEST(FileUtilTest, Basename) {
  EXPECT_EQ(FileUtil::Basename(SP "foo" SP "bar"), "bar");
  EXPECT_EQ(FileUtil::Basename(SP "foo" SP "bar" SP "foo.txt"), "foo.txt");
  EXPECT_EQ(FileUtil::Basename("foo.txt"), "foo.txt");
  EXPECT_EQ(FileUtil::Basename("." SP "foo.txt"), "foo.txt");
  EXPECT_EQ(FileUtil::Basename("." SP ".foo.txt"), ".foo.txt");
  EXPECT_EQ(FileUtil::Basename(SP), "");
  EXPECT_EQ(FileUtil::Basename("foo" SP "bar" SP "buz" SP), "");
}

#undef SP

TEST(FileUtilTest, NormalizeDirectorySeparator) {
#ifdef _WIN32
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("\\foo\\bar"), "\\foo\\bar");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("/foo\\bar"), "\\foo\\bar");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("\\foo/bar"), "\\foo\\bar");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("/foo/bar"), "\\foo\\bar");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("\\foo\\bar\\"),
            "\\foo\\bar\\");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("/foo/bar/"), "\\foo\\bar\\");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator(""), "");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("/"), "\\");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("\\"), "\\");
#else   // _WIN32
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("\\foo\\bar"), "\\foo\\bar");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("/foo\\bar"), "/foo\\bar");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("\\foo/bar"), "\\foo/bar");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("/foo/bar"), "/foo/bar");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("\\foo\\bar\\"),
            "\\foo\\bar\\");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("/foo/bar/"), "/foo/bar/");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator(""), "");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("/"), "/");
  EXPECT_EQ(FileUtil::NormalizeDirectorySeparator("\\"), "\\");
#endif  // _WIN32
}

TEST(FileUtilTest, GetModificationTime) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  EXPECT_FALSE(FileUtil::GetModificationTime(
                   FileUtil::JoinPath(temp_dir.path(), "not_existent_file"))
                   .ok());

  const std::string &path = FileUtil::JoinPath(temp_dir.path(), "testfile");
  CreateTestFile(path, "content");
  absl::StatusOr<FileTimeStamp> time_stamp1 =
      FileUtil::GetModificationTime(path);
  ASSERT_OK(time_stamp1);
  EXPECT_NE(0, *time_stamp1);

  absl::StatusOr<FileTimeStamp> time_stamp2 =
      FileUtil::GetModificationTime(path);
  ASSERT_OK(time_stamp2);
  EXPECT_EQ(*time_stamp1, *time_stamp2);
}

TEST(FileUtilTest, GetAndSetContents) {
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();
  const std::string filename = FileUtil::JoinPath(temp_dir.path(), "test.txt");

  // File doesn't exist yet.
  EXPECT_TRUE(absl::IsNotFound(FileUtil::GetContents(filename).status()));

  // Basic write and read test.
  ASSERT_OK(FileUtil::SetContents(filename, "test"));
  FileUnlinker unlinker(filename);
  absl::StatusOr<std::string> content = FileUtil::GetContents(filename);
  EXPECT_OK(content);
  EXPECT_EQ(*content, "test");

  // Overwrite test.
  ASSERT_OK(FileUtil::SetContents(filename, "more tests!"));
  content = FileUtil::GetContents(filename);
  EXPECT_OK(content);
  EXPECT_EQ(*content, "more tests!");

  // Text mode write.
  ASSERT_OK(FileUtil::SetContents(filename, "test\ntest\n", std::ios::out));
  content = FileUtil::GetContents(filename);
  EXPECT_OK(content);
#ifdef _WIN32
  EXPECT_EQ(*content, "test\r\ntest\r\n");
#else   // _WIN32
  EXPECT_EQ(*content, "test\ntest\n");
#endif  // _WIN32

  // Text mode read.
  ASSERT_OK(FileUtil::SetContents(filename, "test\r\ntest\r\n"));
  content = FileUtil::GetContents(filename, std::ios::in);
  EXPECT_OK(content);
#ifdef _WIN32
  EXPECT_EQ(*content, "test\ntest\n");
#else   // _WIN32
  EXPECT_EQ(*content, "test\r\ntest\r\n");
#endif  // _WIN32
}

TEST(FileUtilTest, FileUnlinker) {
  absl::StatusOr<TempFile> temp_file =
      TempDirectory::Default().CreateTempFile();
  ASSERT_OK(temp_file);
  ASSERT_OK(FileUtil::SetContents(temp_file->path(), "test"));
  {
    FileUnlinker unlinker(temp_file->path());
    EXPECT_OK(FileUtil::FileExists(temp_file->path()));
  }
  EXPECT_FALSE(FileUtil::FileExists(temp_file->path()).ok());
}

}  // namespace
}  // namespace mozc

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

#ifdef OS_WIN
#include <Windows.h>
#endif  // OS_WIN

#include <fstream>
#include <string>

#include "base/file_stream.h"
#include "base/logging.h"
#include "base/util.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"

// Ad-hoc workadound against macro problem on Windows.
// On Windows, following macros, defined when you include <Windows.h>,
// should be removed here because they affects the method name definition of
// Util class.
// TODO(yukawa): Use different method name if applicable.
#ifdef CreateDirectory
#undef CreateDirectory
#endif  // CreateDirectory
#ifdef RemoveDirectory
#undef RemoveDirectory
#endif  // RemoveDirectory
#ifdef CopyFile
#undef CopyFile
#endif  // CopyFile

namespace mozc {
namespace {

void CreateTestFile(const std::string &filename, const std::string &data) {
  OutputFileStream ofs(filename.c_str(), std::ios::binary | std::ios::trunc);
  ofs << data;
  EXPECT_TRUE(ofs.good());
}

TEST(FileUtilTest, CreateDirectory) {
  EXPECT_OK(FileUtil::DirectoryExists(absl::GetFlag(FLAGS_test_tmpdir)));
  // dirpath = FLAGS_test_tmpdir/testdir
  const std::string dirpath =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "testdir");

  // Delete dirpath, if it exists.
  ASSERT_OK(FileUtil::RemoveDirectoryIfExists(dirpath));
  ASSERT_FALSE(FileUtil::FileExists(dirpath).ok());

  // Create the directory.
  EXPECT_OK(FileUtil::CreateDirectory(dirpath));
  EXPECT_OK(FileUtil::DirectoryExists(dirpath));

  // Delete the directory.
  ASSERT_OK(FileUtil::RemoveDirectory(dirpath));
  ASSERT_FALSE(FileUtil::FileExists(dirpath).ok());
}

TEST(FileUtilTest, DirectoryExists) {
  EXPECT_OK(FileUtil::DirectoryExists(absl::GetFlag(FLAGS_test_tmpdir)));
  const std::string filepath =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "testfile");

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
  const std::string filepath =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "testfile");
  ASSERT_OK(FileUtil::UnlinkIfExists(filepath));
  EXPECT_FALSE(FileUtil::FileExists(filepath).ok());

  CreateTestFile(filepath, "simple test");
  EXPECT_OK(FileUtil::FileExists(filepath));
  EXPECT_OK(FileUtil::Unlink(filepath));
  EXPECT_FALSE(FileUtil::FileExists(filepath).ok());

#ifdef OS_WIN
  constexpr DWORD kTestAttributeList[] = {
      FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_HIDDEN,
      FILE_ATTRIBUTE_NORMAL,  FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
      FILE_ATTRIBUTE_OFFLINE, FILE_ATTRIBUTE_READONLY,
      FILE_ATTRIBUTE_SYSTEM,  FILE_ATTRIBUTE_TEMPORARY,
  };

  std::wstring wfilepath;
  Util::Utf8ToWide(filepath, &wfilepath);
  for (size_t i = 0; i < std::size(kTestAttributeList); ++i) {
    SCOPED_TRACE(Util::StringPrintf("AttributeTest %zd", i));
    CreateTestFile(filepath, "attribute_test");
    EXPECT_NE(FALSE,
              ::SetFileAttributesW(wfilepath.c_str(), kTestAttributeList[i]));
    EXPECT_OK(FileUtil::FileExists(filepath));
    EXPECT_OK(FileUtil::Unlink(filepath));
    EXPECT_FALSE(FileUtil::FileExists(filepath).ok());
  }
#endif  // OS_WIN

  EXPECT_OK(FileUtil::UnlinkIfExists(filepath));
}

#ifdef OS_WIN
TEST(FileUtilTest, HideFile) {
  const std::string filename =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "testfile");
  ASSERT_OK(FileUtil::UnlinkIfExists(filename));

  EXPECT_FALSE(FileUtil::HideFile(filename));

  std::wstring wfilename;
  Util::Utf8ToWide(filename.c_str(), &wfilename);

  CreateTestFile(filename, "test data");
  EXPECT_OK(FileUtil::FileExists(filename));

  EXPECT_NE(FALSE,
            ::SetFileAttributesW(wfilename.c_str(), FILE_ATTRIBUTE_NORMAL));
  EXPECT_TRUE(FileUtil::HideFile(filename));
  EXPECT_EQ(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
            ::GetFileAttributesW(wfilename.c_str()));

  EXPECT_NE(FALSE,
            ::SetFileAttributesW(wfilename.c_str(), FILE_ATTRIBUTE_ARCHIVE));
  EXPECT_TRUE(FileUtil::HideFile(filename));
  EXPECT_EQ(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_ATTRIBUTE_ARCHIVE,
            ::GetFileAttributesW(wfilename.c_str()));

  EXPECT_NE(FALSE,
            ::SetFileAttributesW(wfilename.c_str(), FILE_ATTRIBUTE_NORMAL));
  EXPECT_TRUE(FileUtil::HideFileWithExtraAttributes(filename,
                                                    FILE_ATTRIBUTE_TEMPORARY));
  EXPECT_EQ(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_ATTRIBUTE_TEMPORARY,
            ::GetFileAttributesW(wfilename.c_str()));

  EXPECT_NE(FALSE,
            ::SetFileAttributesW(wfilename.c_str(), FILE_ATTRIBUTE_ARCHIVE));
  EXPECT_TRUE(FileUtil::HideFileWithExtraAttributes(filename,
                                                    FILE_ATTRIBUTE_TEMPORARY));
  EXPECT_EQ(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_ATTRIBUTE_ARCHIVE |
                FILE_ATTRIBUTE_TEMPORARY,
            ::GetFileAttributesW(wfilename.c_str()));

  ASSERT_OK(FileUtil::Unlink(filename));
}
#endif  // OS_WIN

TEST(FileUtilTest, IsEqualFile) {
  const std::string filename1 =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test1");
  const std::string filename2 =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test2");
  ASSERT_OK(FileUtil::UnlinkIfExists(filename1));
  ASSERT_OK(FileUtil::UnlinkIfExists(filename2));
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

  ASSERT_OK(FileUtil::Unlink(filename1));
  ASSERT_OK(FileUtil::Unlink(filename2));
}

TEST(FileUtilTest, IsEquivalent) {
  const std::string filename1 =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test1");
  const std::string filename2 =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test2");
  ASSERT_OK(FileUtil::UnlinkIfExists(filename1));
  ASSERT_OK(FileUtil::UnlinkIfExists(filename2));
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
  // just test rename operation works as intended
  const std::string from =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "copy_from");
  const std::string to =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "copy_to");
  ASSERT_OK(FileUtil::UnlinkIfExists(from));
  ASSERT_OK(FileUtil::UnlinkIfExists(to));

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

#ifdef OS_WIN
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
    std::wstring wfrom, wto;
    Util::Utf8ToWide(from.c_str(), &wfrom);
    Util::Utf8ToWide(to.c_str(), &wto);
    EXPECT_NE(FALSE,
              ::SetFileAttributesW(wfrom.c_str(), kData.from_attributes));
    EXPECT_NE(FALSE, ::SetFileAttributesW(wto.c_str(), kData.to_attributes));

    EXPECT_OK(FileUtil::CopyFile(from, to));
    absl::StatusOr<bool> s = FileUtil::IsEqualFile(from, to);
    EXPECT_OK(s);
    EXPECT_TRUE(*s);
    EXPECT_EQ(kData.from_attributes, ::GetFileAttributesW(wfrom.c_str()));
    EXPECT_EQ(kData.from_attributes, ::GetFileAttributesW(wto.c_str()));

    EXPECT_NE(FALSE,
              ::SetFileAttributesW(wfrom.c_str(), FILE_ATTRIBUTE_NORMAL));
    EXPECT_NE(FALSE, ::SetFileAttributesW(wto.c_str(), FILE_ATTRIBUTE_NORMAL));
  }
#endif  // OS_WIN

  ASSERT_OK(FileUtil::Unlink(from));
  ASSERT_OK(FileUtil::Unlink(to));
}

TEST(FileUtilTest, AtomicRename) {
  // just test rename operation works as intended
  const std::string from = FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir),
                                              "atomic_rename_test_from");
  const std::string to = FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir),
                                            "atomic_rename_test_to");
  ASSERT_OK(FileUtil::UnlinkIfExists(from));
  ASSERT_OK(FileUtil::UnlinkIfExists(to));

  // |from| is not found
  EXPECT_FALSE(FileUtil::AtomicRename(from, to).ok());
  CreateTestFile(from, "test");
  EXPECT_OK(FileUtil::AtomicRename(from, to));

  // from is deleted
  EXPECT_FALSE(FileUtil::FileExists(from).ok());
  EXPECT_OK(FileUtil::FileExists(to));

  {
    InputFileStream ifs(to.c_str());
    EXPECT_TRUE(ifs.good());
    std::string line;
    std::getline(ifs, line);
    EXPECT_EQ("test", line);
  }

  EXPECT_FALSE(FileUtil::AtomicRename(from, to).ok());

  ASSERT_OK(FileUtil::UnlinkIfExists(from));
  ASSERT_OK(FileUtil::UnlinkIfExists(to));

  // overwrite the file
  CreateTestFile(from, "test");
  CreateTestFile(to, "test");
  EXPECT_OK(FileUtil::AtomicRename(from, to));

#ifdef OS_WIN
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
    std::wstring wfrom, wto;
    Util::Utf8ToWide(from.c_str(), &wfrom);
    Util::Utf8ToWide(to.c_str(), &wto);
    EXPECT_NE(FALSE,
              ::SetFileAttributesW(wfrom.c_str(), kData.from_attributes));
    EXPECT_NE(FALSE, ::SetFileAttributesW(wto.c_str(), kData.to_attributes));

    EXPECT_OK(FileUtil::AtomicRename(from, to));
    EXPECT_EQ(kData.from_attributes, ::GetFileAttributesW(wto.c_str()));
    EXPECT_FALSE(FileUtil::FileExists(from).ok());
    EXPECT_OK(FileUtil::FileExists(to));

    ::SetFileAttributesW(wfrom.c_str(), FILE_ATTRIBUTE_NORMAL);
    ::SetFileAttributesW(wto.c_str(), FILE_ATTRIBUTE_NORMAL);
  }
#endif  // OS_WIN

  ASSERT_OK(FileUtil::UnlinkIfExists(from));
  ASSERT_OK(FileUtil::UnlinkIfExists(to));
}

TEST(FileUtilTest, CreateHardLink) {
  const std::string filename1 =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test1");
  const std::string filename2 =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test2");
  ASSERT_OK(FileUtil::UnlinkIfExists(filename1));
  ASSERT_OK(FileUtil::UnlinkIfExists(filename2));

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

#ifdef OS_WIN
#define SP "\\"
#else  // OS_WIN
#define SP "/"
#endif  // OS_WIN

TEST(FileUtilTest, JoinPath) {
  EXPECT_TRUE(FileUtil::JoinPath({}).empty());
  EXPECT_EQ("foo", FileUtil::JoinPath({"foo"}));
  EXPECT_EQ("foo" SP "bar", FileUtil::JoinPath({"foo", "bar"}));
  EXPECT_EQ("foo" SP "bar" SP "baz", FileUtil::JoinPath({"foo", "bar", "baz"}));

  // Some path components end with delimiter.
  EXPECT_EQ("foo" SP "bar" SP "baz",
            FileUtil::JoinPath({"foo" SP, "bar", "baz"}));
  EXPECT_EQ("foo" SP "bar" SP "baz",
            FileUtil::JoinPath({"foo", "bar" SP, "baz"}));
  EXPECT_EQ("foo" SP "bar" SP "baz" SP,
            FileUtil::JoinPath({"foo", "bar", "baz" SP}));

  // Containing empty strings.
  EXPECT_TRUE(FileUtil::JoinPath({"", "", ""}).empty());
  EXPECT_EQ("foo" SP "bar", FileUtil::JoinPath({"", "foo", "bar"}));
  EXPECT_EQ("foo" SP "bar", FileUtil::JoinPath({"foo", "", "bar"}));
  EXPECT_EQ("foo" SP "bar", FileUtil::JoinPath({"foo", "bar", ""}));
}

TEST(FileUtilTest, Dirname) {
  EXPECT_EQ(SP "foo", FileUtil::Dirname(SP "foo" SP "bar"));
  EXPECT_EQ(SP "foo" SP "bar",
            FileUtil::Dirname(SP "foo" SP "bar" SP "foo.txt"));
  EXPECT_EQ("", FileUtil::Dirname("foo.txt"));
  EXPECT_EQ("", FileUtil::Dirname(SP));
}

TEST(FileUtilTest, Basename) {
  EXPECT_EQ("bar", FileUtil::Basename(SP "foo" SP "bar"));
  EXPECT_EQ("foo.txt", FileUtil::Basename(SP "foo" SP "bar" SP "foo.txt"));
  EXPECT_EQ("foo.txt", FileUtil::Basename("foo.txt"));
  EXPECT_EQ("foo.txt", FileUtil::Basename("." SP "foo.txt"));
  EXPECT_EQ(".foo.txt", FileUtil::Basename("." SP ".foo.txt"));
  EXPECT_EQ("", FileUtil::Basename(SP));
  EXPECT_EQ("", FileUtil::Basename("foo" SP "bar" SP "buz" SP));
}

#undef SP

TEST(FileUtilTest, NormalizeDirectorySeparator) {
#ifdef OS_WIN
  EXPECT_EQ("\\foo\\bar", FileUtil::NormalizeDirectorySeparator("\\foo\\bar"));
  EXPECT_EQ("\\foo\\bar", FileUtil::NormalizeDirectorySeparator("/foo\\bar"));
  EXPECT_EQ("\\foo\\bar", FileUtil::NormalizeDirectorySeparator("\\foo/bar"));
  EXPECT_EQ("\\foo\\bar", FileUtil::NormalizeDirectorySeparator("/foo/bar"));
  EXPECT_EQ("\\foo\\bar\\",
            FileUtil::NormalizeDirectorySeparator("\\foo\\bar\\"));
  EXPECT_EQ("\\foo\\bar\\", FileUtil::NormalizeDirectorySeparator("/foo/bar/"));
  EXPECT_EQ("", FileUtil::NormalizeDirectorySeparator(""));
  EXPECT_EQ("\\", FileUtil::NormalizeDirectorySeparator("/"));
  EXPECT_EQ("\\", FileUtil::NormalizeDirectorySeparator("\\"));
#else   // OS_WIN
  EXPECT_EQ("\\foo\\bar", FileUtil::NormalizeDirectorySeparator("\\foo\\bar"));
  EXPECT_EQ("/foo\\bar", FileUtil::NormalizeDirectorySeparator("/foo\\bar"));
  EXPECT_EQ("\\foo/bar", FileUtil::NormalizeDirectorySeparator("\\foo/bar"));
  EXPECT_EQ("/foo/bar", FileUtil::NormalizeDirectorySeparator("/foo/bar"));
  EXPECT_EQ("\\foo\\bar\\",
            FileUtil::NormalizeDirectorySeparator("\\foo\\bar\\"));
  EXPECT_EQ("/foo/bar/", FileUtil::NormalizeDirectorySeparator("/foo/bar/"));
  EXPECT_EQ("", FileUtil::NormalizeDirectorySeparator(""));
  EXPECT_EQ("/", FileUtil::NormalizeDirectorySeparator("/"));
  EXPECT_EQ("\\", FileUtil::NormalizeDirectorySeparator("\\"));
#endif  // OS_WIN
}

TEST(FileUtilTest, GetModificationTime) {
  EXPECT_FALSE(FileUtil::GetModificationTime("not_existent_file").ok());

  const std::string &path =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "testfile");
  CreateTestFile(path, "content");
  absl::StatusOr<FileTimeStamp> time_stamp1 =
      FileUtil::GetModificationTime(path);
  ASSERT_OK(time_stamp1);
  EXPECT_NE(0, *time_stamp1);

  absl::StatusOr<FileTimeStamp> time_stamp2 =
      FileUtil::GetModificationTime(path);
  ASSERT_OK(time_stamp2);
  EXPECT_EQ(*time_stamp1, *time_stamp2);

  // Cleanup
  ASSERT_OK(FileUtil::Unlink(path));
}

}  // namespace
}  // namespace mozc

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
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/flags/flag.h"

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

class FileUtilTest : public testing::Test {};

namespace {
void CreateTestFile(const std::string &filename, const std::string &data) {
  OutputFileStream ofs(filename.c_str(), std::ios::binary | std::ios::trunc);
  ofs << data;
  EXPECT_TRUE(ofs.good());
}
}  // namespace

TEST_F(FileUtilTest, CreateDirectory) {
  EXPECT_TRUE(FileUtil::DirectoryExists(absl::GetFlag(FLAGS_test_tmpdir)));
  // dirpath = FLAGS_test_tmpdir/testdir
  const std::string dirpath =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "testdir");

  // Delete dirpath, if it exists.
  if (FileUtil::FileExists(dirpath)) {
    FileUtil::RemoveDirectory(dirpath);
  }
  ASSERT_FALSE(FileUtil::FileExists(dirpath));

  // Create the directory.
  EXPECT_TRUE(FileUtil::CreateDirectory(dirpath));
  EXPECT_TRUE(FileUtil::DirectoryExists(dirpath));

  // Delete the directory.
  ASSERT_TRUE(FileUtil::RemoveDirectory(dirpath));
  ASSERT_FALSE(FileUtil::FileExists(dirpath));
}

TEST_F(FileUtilTest, DirectoryExists) {
  EXPECT_TRUE(FileUtil::DirectoryExists(absl::GetFlag(FLAGS_test_tmpdir)));
  const std::string filepath =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "testfile");

  // Delete filepath, if it exists.
  if (FileUtil::FileExists(filepath)) {
    FileUtil::Unlink(filepath);
  }
  ASSERT_FALSE(FileUtil::FileExists(filepath));

  // Create a file.
  CreateTestFile(filepath, "test data");
  EXPECT_TRUE(FileUtil::FileExists(filepath));
  EXPECT_FALSE(FileUtil::DirectoryExists(filepath));

  // Delete the file.
  FileUtil::Unlink(filepath);
  ASSERT_FALSE(FileUtil::FileExists(filepath));
}

TEST_F(FileUtilTest, Unlink) {
  const std::string filepath =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "testfile");
  FileUtil::Unlink(filepath);
  EXPECT_FALSE(FileUtil::FileExists(filepath));

  CreateTestFile(filepath, "simple test");
  EXPECT_TRUE(FileUtil::FileExists(filepath));
  EXPECT_TRUE(FileUtil::Unlink(filepath));
  EXPECT_FALSE(FileUtil::FileExists(filepath));

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
    EXPECT_TRUE(FileUtil::FileExists(filepath));
    EXPECT_TRUE(FileUtil::Unlink(filepath));
    EXPECT_FALSE(FileUtil::FileExists(filepath));
  }
#endif  // OS_WIN

  FileUtil::Unlink(filepath);
}

#ifdef OS_WIN
TEST_F(FileUtilTest, HideFile) {
  const std::string filename =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "testfile");
  FileUtil::Unlink(filename);

  EXPECT_FALSE(FileUtil::HideFile(filename));

  std::wstring wfilename;
  Util::Utf8ToWide(filename.c_str(), &wfilename);

  CreateTestFile(filename, "test data");
  EXPECT_TRUE(FileUtil::FileExists(filename));

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

  FileUtil::Unlink(filename);
}
#endif  // OS_WIN

TEST_F(FileUtilTest, IsEqualFile) {
  const std::string filename1 =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test1");
  const std::string filename2 =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test2");
  FileUtil::Unlink(filename1);
  FileUtil::Unlink(filename2);
  EXPECT_FALSE(FileUtil::IsEqualFile(filename1, filename2));

  CreateTestFile(filename1, "test data1");
  EXPECT_FALSE(FileUtil::IsEqualFile(filename1, filename2));

  CreateTestFile(filename2, "test data1");
  EXPECT_TRUE(FileUtil::IsEqualFile(filename1, filename2));

  CreateTestFile(filename2, "test data1 test data1");
  EXPECT_FALSE(FileUtil::IsEqualFile(filename1, filename2));

  CreateTestFile(filename2, "test data2");
  EXPECT_FALSE(FileUtil::IsEqualFile(filename1, filename2));

  FileUtil::Unlink(filename1);
  FileUtil::Unlink(filename2);
}

TEST_F(FileUtilTest, CopyFile) {
  // just test rename operation works as intended
  const std::string from =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "copy_from");
  const std::string to =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "copy_to");
  FileUtil::Unlink(from);
  FileUtil::Unlink(to);

  CreateTestFile(from, "simple test");
  EXPECT_TRUE(FileUtil::CopyFile(from, to));
  EXPECT_TRUE(FileUtil::IsEqualFile(from, to));

  CreateTestFile(from, "overwrite test");
  EXPECT_TRUE(FileUtil::CopyFile(from, to));
  EXPECT_TRUE(FileUtil::IsEqualFile(from, to));

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

    EXPECT_TRUE(FileUtil::CopyFile(from, to));
    EXPECT_TRUE(FileUtil::IsEqualFile(from, to));
    EXPECT_EQ(kData.from_attributes, ::GetFileAttributesW(wfrom.c_str()));
    EXPECT_EQ(kData.from_attributes, ::GetFileAttributesW(wto.c_str()));

    EXPECT_NE(FALSE,
              ::SetFileAttributesW(wfrom.c_str(), FILE_ATTRIBUTE_NORMAL));
    EXPECT_NE(FALSE, ::SetFileAttributesW(wto.c_str(), FILE_ATTRIBUTE_NORMAL));
  }
#endif  // OS_WIN

  FileUtil::Unlink(from);
  FileUtil::Unlink(to);
}

TEST_F(FileUtilTest, AtomicRename) {
  // just test rename operation works as intended
  const std::string from = FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir),
                                              "atomic_rename_test_from");
  const std::string to = FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir),
                                            "atomic_rename_test_to");
  FileUtil::Unlink(from);
  FileUtil::Unlink(to);

  // |from| is not found
  EXPECT_FALSE(FileUtil::AtomicRename(from, to));
  CreateTestFile(from, "test");
  EXPECT_TRUE(FileUtil::AtomicRename(from, to));

  // from is deleted
  EXPECT_FALSE(FileUtil::FileExists(from));
  EXPECT_TRUE(FileUtil::FileExists(to));

  {
    InputFileStream ifs(to.c_str());
    EXPECT_TRUE(ifs.good());
    std::string line;
    std::getline(ifs, line);
    EXPECT_EQ("test", line);
  }

  EXPECT_FALSE(FileUtil::AtomicRename(from, to));

  FileUtil::Unlink(from);
  FileUtil::Unlink(to);

  // overwrite the file
  CreateTestFile(from, "test");
  CreateTestFile(to, "test");
  EXPECT_TRUE(FileUtil::AtomicRename(from, to));

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

    EXPECT_TRUE(FileUtil::AtomicRename(from, to));
    EXPECT_EQ(kData.from_attributes, ::GetFileAttributesW(wto.c_str()));
    EXPECT_FALSE(FileUtil::FileExists(from));
    EXPECT_TRUE(FileUtil::FileExists(to));

    ::SetFileAttributesW(wfrom.c_str(), FILE_ATTRIBUTE_NORMAL);
    ::SetFileAttributesW(wto.c_str(), FILE_ATTRIBUTE_NORMAL);
  }
#endif  // OS_WIN

  FileUtil::Unlink(from);
  FileUtil::Unlink(to);
}

#ifdef OS_WIN
#define SP "\\"
#else
#define SP "/"
#endif  // OS_WIN

TEST_F(FileUtilTest, JoinPath) {
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

TEST_F(FileUtilTest, Dirname) {
  EXPECT_EQ(SP "foo", FileUtil::Dirname(SP "foo" SP "bar"));
  EXPECT_EQ(SP "foo" SP "bar",
            FileUtil::Dirname(SP "foo" SP "bar" SP "foo.txt"));
  EXPECT_EQ("", FileUtil::Dirname("foo.txt"));
  EXPECT_EQ("", FileUtil::Dirname(SP));
}

TEST_F(FileUtilTest, Basename) {
  EXPECT_EQ("bar", FileUtil::Basename(SP "foo" SP "bar"));
  EXPECT_EQ("foo.txt", FileUtil::Basename(SP "foo" SP "bar" SP "foo.txt"));
  EXPECT_EQ("foo.txt", FileUtil::Basename("foo.txt"));
  EXPECT_EQ("foo.txt", FileUtil::Basename("." SP "foo.txt"));
  EXPECT_EQ(".foo.txt", FileUtil::Basename("." SP ".foo.txt"));
  EXPECT_EQ("", FileUtil::Basename(SP));
  EXPECT_EQ("", FileUtil::Basename("foo" SP "bar" SP "buz" SP));
}

#undef SP

TEST_F(FileUtilTest, NormalizeDirectorySeparator) {
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
#else
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

TEST_F(FileUtilTest, GetModificationTime) {
  FileTimeStamp time_stamp = 0;
  EXPECT_FALSE(FileUtil::GetModificationTime("not_existent_file", &time_stamp));

  const std::string &path =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "testfile");
  CreateTestFile(path, "content");
  EXPECT_TRUE(FileUtil::GetModificationTime(path, &time_stamp));
  EXPECT_NE(0, time_stamp);

  FileTimeStamp time_stamp2 = 0;
  EXPECT_TRUE(FileUtil::GetModificationTime(path, &time_stamp2));
  EXPECT_EQ(time_stamp, time_stamp2);

  // Cleanup
  FileUtil::Unlink(path);
}

}  // namespace mozc

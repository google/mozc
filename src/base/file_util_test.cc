// Copyright 2010-2013, Google Inc.
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

#include <fstream>

#include "base/file_stream.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_srcdir);
DECLARE_string(test_tmpdir);

namespace mozc {

class FileUtilTest : public testing::Test {
};

#ifndef MOZC_USE_PEPPER_FILE_IO

TEST_F(FileUtilTest, CreateDirectory) {
  EXPECT_TRUE(FileUtil::DirectoryExists(FLAGS_test_tmpdir));
  // dirpath = FLAGS_test_tmpdir/testdir
  const string dirpath = FileUtil::JoinPath(FLAGS_test_tmpdir, "testdir");

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
  EXPECT_TRUE(FileUtil::DirectoryExists(FLAGS_test_tmpdir));
  const string filepath = FileUtil::JoinPath(FLAGS_test_tmpdir, "testfile");

  // Delete filepath, if it exists.
  if (FileUtil::FileExists(filepath)) {
    FileUtil::Unlink(filepath);
  }
  ASSERT_FALSE(FileUtil::FileExists(filepath));

  // Create a file.
  ofstream file(filepath.c_str());
  file << "test data" << endl;
  file.close();

  EXPECT_TRUE(FileUtil::FileExists(filepath));
  EXPECT_FALSE(FileUtil::DirectoryExists(filepath));

  // Delete the file.
  FileUtil::Unlink(filepath);
  ASSERT_FALSE(FileUtil::FileExists(filepath));
}

TEST_F(FileUtilTest, CopyFile) {
  // just test rename operation works as intended
  const string from = FileUtil::JoinPath(FLAGS_test_tmpdir, "copy_from");
  const string to = FileUtil::JoinPath(FLAGS_test_tmpdir, "copy_to");
  FileUtil::Unlink(from);
  FileUtil::Unlink(to);

  const char kData[] = "This is a test";

  {
    OutputFileStream ofs(from.c_str(), ios::binary);
    ofs.write(kData, arraysize(kData));
  }

  EXPECT_TRUE(FileUtil::CopyFile(from, to));
  Mmap mmap;
  ASSERT_TRUE(mmap.Open(to.c_str()));

  EXPECT_EQ(arraysize(kData), mmap.size());
  EXPECT_EQ(0, memcmp(mmap.begin(), kData, mmap.size()));

  FileUtil::Unlink(from);
  FileUtil::Unlink(to);
}

TEST_F(FileUtilTest, IsEqualFile) {
  const string filename1 = FileUtil::JoinPath(FLAGS_test_tmpdir, "test1");
  const string filename2 = FileUtil::JoinPath(FLAGS_test_tmpdir, "test2");
  FileUtil::Unlink(filename1);
  FileUtil::Unlink(filename2);
  EXPECT_FALSE(FileUtil::IsEqualFile(filename1, filename2));

  const char kTestData1[] = "test data1";
  const char kTestData2[] = "test data2";

  {
    OutputFileStream ofs1(filename1.c_str());
    ofs1 << kTestData1;
  }
  EXPECT_FALSE(FileUtil::IsEqualFile(filename1, filename2));

  {
    OutputFileStream ofs2(filename2.c_str());
    ofs2 << kTestData1;
  }

  EXPECT_TRUE(FileUtil::IsEqualFile(filename1, filename2));

  {
    OutputFileStream ofs2(filename2.c_str());
    ofs2 << kTestData1;
    ofs2 << kTestData1;
  }
  EXPECT_FALSE(FileUtil::IsEqualFile(filename1, filename2));

  {
    OutputFileStream ofs2(filename2.c_str());
    ofs2 << kTestData2;
  }
  EXPECT_FALSE(FileUtil::IsEqualFile(filename1, filename2));

  FileUtil::Unlink(filename1);
  FileUtil::Unlink(filename2);
}

TEST_F(FileUtilTest, AtomicRename) {
  // just test rename operation works as intended
  const string from = FileUtil::JoinPath(FLAGS_test_tmpdir,
                                         "atomic_rename_test_from");
  const string to = FileUtil::JoinPath(FLAGS_test_tmpdir,
                                       "atomic_rename_test_to");
  FileUtil::Unlink(from);
  FileUtil::Unlink(to);

  // |from| is not found
  EXPECT_FALSE(FileUtil::AtomicRename(from, to));
  {
    OutputFileStream ofs(from.c_str());
    EXPECT_TRUE(ofs.good());
    ofs << "test" << endl;
  }

  EXPECT_TRUE(FileUtil::AtomicRename(from, to));

  // from is deleted
  EXPECT_FALSE(FileUtil::FileExists(from));
  EXPECT_FALSE(!FileUtil::FileExists(to));

  {
    InputFileStream ifs(to.c_str());
    EXPECT_TRUE(ifs.good());
    string line;
    getline(ifs, line);
    EXPECT_EQ("test", line);
  }

  EXPECT_FALSE(FileUtil::AtomicRename(from, to));

  FileUtil::Unlink(from);
  FileUtil::Unlink(to);

  // overwrite the file
  {
    OutputFileStream ofs1(from.c_str());
    ofs1 << "test";
    OutputFileStream ofs2(to.c_str());
    ofs2 << "test";
  }
  EXPECT_TRUE(FileUtil::AtomicRename(from, to));

  FileUtil::Unlink(from);
  FileUtil::Unlink(to);
}

#endif  // MOZC_USE_PEPPER_FILE_IO

TEST_F(FileUtilTest, Dirname) {
#ifdef OS_WIN
  EXPECT_EQ("\\foo", FileUtil::Dirname("\\foo\\bar"));
  EXPECT_EQ("\\foo\\bar", FileUtil::Dirname("\\foo\\bar\\foo.txt"));
  EXPECT_EQ("", FileUtil::Dirname("foo.txt"));
  EXPECT_EQ("", FileUtil::Dirname("\\"));
#else
  EXPECT_EQ("/foo", FileUtil::Dirname("/foo/bar"));
  EXPECT_EQ("/foo/bar", FileUtil::Dirname("/foo/bar/foo.txt"));
  EXPECT_EQ("", FileUtil::Dirname("foo.txt"));
  EXPECT_EQ("", FileUtil::Dirname("/"));
#endif  // OS_WIN
}

TEST_F(FileUtilTest, Basename) {
#ifdef OS_WIN
  EXPECT_EQ("bar", FileUtil::Basename("\\foo\\bar"));
  EXPECT_EQ("foo.txt", FileUtil::Basename("\\foo\\bar\\foo.txt"));
  EXPECT_EQ("foo.txt", FileUtil::Basename("foo.txt"));
  EXPECT_EQ("foo.txt", FileUtil::Basename(".\\foo.txt"));
  EXPECT_EQ(".foo.txt", FileUtil::Basename(".\\.foo.txt"));
  EXPECT_EQ("", FileUtil::Basename("\\"));
  EXPECT_EQ("", FileUtil::Basename("foo\\bar\\buz\\"));
#else
  EXPECT_EQ("bar", FileUtil::Basename("/foo/bar"));
  EXPECT_EQ("foo.txt", FileUtil::Basename("/foo/bar/foo.txt"));
  EXPECT_EQ("foo.txt", FileUtil::Basename("foo.txt"));
  EXPECT_EQ("foo.txt", FileUtil::Basename("./foo.txt"));
  EXPECT_EQ(".foo.txt", FileUtil::Basename("./.foo.txt"));
  EXPECT_EQ("", FileUtil::Basename("/"));
  EXPECT_EQ("", FileUtil::Basename("foo/bar/buz/"));
#endif  // OS_WIN
}

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

}  // namespace mozc

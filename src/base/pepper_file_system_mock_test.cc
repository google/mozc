// Copyright 2010-2018, Google Inc.
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

#ifdef OS_NACL

#include "base/pepper_file_system_mock.h"

#include "testing/base/public/gunit.h"

namespace mozc {

TEST(PepperFileSystemMockTest, ReadWriteDeleteBinaryFile) {
  PepperFileSystemMock file_system;
  const char *kFilename = "/test.dat";
  const string kTestData1("1_foo\0bar\nbaz");
  const string kTestData2("2_foo\0bar\nbaz");

  EXPECT_TRUE(file_system.DirectoryExists("/"));
  EXPECT_FALSE(file_system.FileExists(kFilename));
  EXPECT_FALSE(file_system.DirectoryExists(kFilename));

  string buffer;
  EXPECT_FALSE(file_system.ReadBinaryFile(kFilename, &buffer));
  EXPECT_FALSE(file_system.FileExists(kFilename));
  EXPECT_FALSE(file_system.DirectoryExists(kFilename));

  EXPECT_TRUE(file_system.WriteBinaryFile(kFilename, kTestData1));
  EXPECT_TRUE(file_system.ReadBinaryFile(kFilename, &buffer));
  EXPECT_EQ(kTestData1, buffer);
  EXPECT_TRUE(file_system.FileExists(kFilename));
  EXPECT_FALSE(file_system.DirectoryExists(kFilename));

  EXPECT_TRUE(file_system.WriteBinaryFile(kFilename, kTestData2));
  EXPECT_TRUE(file_system.ReadBinaryFile(kFilename, &buffer));
  EXPECT_EQ(kTestData2, buffer);
  EXPECT_TRUE(file_system.FileExists(kFilename));
  EXPECT_FALSE(file_system.DirectoryExists(kFilename));

  EXPECT_TRUE(file_system.Delete(kFilename));
  EXPECT_FALSE(file_system.ReadBinaryFile(kFilename, &buffer));
  EXPECT_FALSE(file_system.FileExists(kFilename));
  EXPECT_FALSE(file_system.DirectoryExists(kFilename));
}

TEST(PepperFileSystemMockTest, DirectoryTest) {
  PepperFileSystemMock file_system;

  EXPECT_TRUE(file_system.DirectoryExists("/"));
  EXPECT_FALSE(file_system.FileExists("/foo"));

  EXPECT_TRUE(file_system.CreateDirectory("/foo"));
  EXPECT_TRUE(file_system.FileExists("/foo"));
  EXPECT_TRUE(file_system.DirectoryExists("/foo"));
  EXPECT_FALSE(file_system.CreateDirectory("/foo"));

  EXPECT_TRUE(file_system.WriteBinaryFile("/foo/bar.txt", "abc"));
  EXPECT_TRUE(file_system.FileExists("/foo/bar.txt"));
  EXPECT_FALSE(file_system.DirectoryExists("/foo/bar.txt"));
  string buffer;
  EXPECT_TRUE(file_system.ReadBinaryFile("/foo/bar.txt", &buffer));
  EXPECT_EQ("abc", buffer);

  EXPECT_TRUE(file_system.WriteBinaryFile("/bar", ""));
  EXPECT_FALSE(file_system.CreateDirectory("/bar"));
  EXPECT_TRUE(file_system.FileExists("/bar"));
  EXPECT_FALSE(file_system.DirectoryExists("/bar"));

  EXPECT_FALSE(file_system.CreateDirectory("/a/b"));
  EXPECT_FALSE(file_system.FileExists("/a"));
  EXPECT_FALSE(file_system.FileExists("/a/b"));
}

TEST(PepperFileSystemMockTest, RenameTest) {
  PepperFileSystemMock file_system;

  EXPECT_TRUE(file_system.CreateDirectory("/foo"));
  EXPECT_TRUE(file_system.CreateDirectory("/foo/bar"));
  EXPECT_TRUE(file_system.CreateDirectory("/foo/baz"));
  EXPECT_TRUE(file_system.CreateDirectory("/a"));

  EXPECT_FALSE(file_system.Rename("/aaa", "/bbb"));
  EXPECT_TRUE(file_system.Rename("/a", "/a"));
  EXPECT_FALSE(file_system.Rename("/foo", "/a"));

  EXPECT_TRUE(file_system.Rename("/foo", "/a/b"));
  EXPECT_FALSE(file_system.DirectoryExists("/foo"));
  EXPECT_FALSE(file_system.DirectoryExists("/foo/bar"));
  EXPECT_FALSE(file_system.DirectoryExists("/foo/baz"));
  EXPECT_TRUE(file_system.DirectoryExists("/a"));
  EXPECT_TRUE(file_system.DirectoryExists("/a/b"));
  EXPECT_TRUE(file_system.DirectoryExists("/a/b/bar"));
  EXPECT_TRUE(file_system.DirectoryExists("/a/b/baz"));
}

}  // namespace mozc

#endif  // OS_NACL

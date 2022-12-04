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

#include "base/multifile.h"

#include <ostream>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/util.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

namespace mozc {

TEST(InputMultiFileTest, OpenNonexistentFilesTest) {
  // Empty string
  {
    InputMultiFile multfile("");
    std::string line;
    EXPECT_FALSE(multfile.ReadLine(&line));
    EXPECT_FALSE(multfile.ReadLine(&line));
    EXPECT_FALSE(multfile.ReadLine(&line));
  }

  // Single path
  {
    const std::string path = FileUtil::JoinPath(
        absl::GetFlag(FLAGS_test_tmpdir), "this_file_does_not_exist");
    InputMultiFile multfile(path);
    std::string line;
    EXPECT_FALSE(multfile.ReadLine(&line));
    EXPECT_FALSE(multfile.ReadLine(&line));
    EXPECT_FALSE(multfile.ReadLine(&line));
  }

  // Multiple paths
  {
    std::vector<std::string> filenames;
    filenames.push_back(
        FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "these_files"));
    filenames.push_back(
        FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "do_not"));
    filenames.push_back(
        FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "exists"));

    std::string joined_path = absl::StrJoin(filenames, ",");
    InputMultiFile multfile(joined_path);
    std::string line;
    EXPECT_FALSE(multfile.ReadLine(&line));
    EXPECT_FALSE(multfile.ReadLine(&line));
    EXPECT_FALSE(multfile.ReadLine(&line));
  }
}

TEST(InputMultiFileTest, ReadSingleFileTest) {
  EXPECT_OK(FileUtil::DirectoryExists(absl::GetFlag(FLAGS_test_tmpdir)));
  const std::string path =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "i_am_a_test_file");

  // Create a test file
  std::vector<std::string> expected_lines;
  constexpr int kNumLines = 10;
  {
    OutputFileStream ofs(path);
    for (int i = 0; i < kNumLines; ++i) {
      std::string line = absl::StrFormat("Hi, line %d", i);
      expected_lines.push_back(line);
      ofs << line << std::endl;
    }
  }
  EXPECT_EQ(kNumLines, expected_lines.size());

  // Read lines
  InputMultiFile multfile(path);
  std::string line;
  for (int i = 0; i < kNumLines; ++i) {
    EXPECT_TRUE(multfile.ReadLine(&line));
    EXPECT_EQ(expected_lines[i], line);
  }
  EXPECT_FALSE(multfile.ReadLine(&line));  // Check if no more lines remain
  EXPECT_FALSE(multfile.ReadLine(&line));
  EXPECT_FALSE(multfile.ReadLine(&line));
}

TEST(InputMultiFileTest, ReadMultipleFilesTest) {
  EXPECT_OK(FileUtil::DirectoryExists(absl::GetFlag(FLAGS_test_tmpdir)));

  constexpr int kNumFile = 3;
  constexpr int kNumLinesPerFile = 10;

  // Create test files
  std::vector<std::string> paths;
  std::vector<std::string> expected_lines;
  {
    int serial_line_no = 0;
    for (int fileno = 0; fileno < kNumFile; ++fileno) {
      std::string filename = absl::StrFormat("testfile%d", fileno);
      std::string path =
          FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), filename);
      paths.push_back(path);

      OutputFileStream ofs(path);
      for (int i = 0; i < kNumLinesPerFile; ++i) {
        std::string line = absl::StrFormat("Hi, line %d", ++serial_line_no);
        expected_lines.push_back(line);
        ofs << line << std::endl;
      }
    }
  }
  EXPECT_EQ(kNumLinesPerFile * kNumFile, expected_lines.size());

  // Read lines
  std::string joined_path = absl::StrJoin(paths, ",");
  InputMultiFile multfile(joined_path);
  std::string line;
  for (int i = 0; i < kNumFile * kNumLinesPerFile; ++i) {
    EXPECT_TRUE(multfile.ReadLine(&line));
    EXPECT_EQ(expected_lines[i], line);
  }
  EXPECT_FALSE(multfile.ReadLine(&line));  // Check if no more lines remain
  EXPECT_FALSE(multfile.ReadLine(&line));
  EXPECT_FALSE(multfile.ReadLine(&line));
}

}  // namespace mozc

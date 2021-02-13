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

#include "base/config_file_stream.h"

#include <cstddef>
#include <ios>
#include <istream>
#include <memory>

#include "base/file_util.h"
#include "base/system_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/flags/flag.h"

namespace mozc {

namespace {
// Returns all data of the filename.
std::string GetFileData(const std::string &filename) {
  InputFileStream ifs(filename.c_str(), std::ios::binary);
  char c = '\0';
  std::string data;
  while (!ifs.get(c).fail()) {
    data.append(1, c);
  }
  return data;
}

// Returns true if |input_stream| is at the end of stream. This function
// peeks one more character in case the current position is actually at
// the end of the stream but |input_stream| instance has not observed it
// yet. In other words, this method may change the internal state of
// |input_stream| as a side effect.
bool IsEof(std::istream *input_stream) {
  return (input_stream->peek() == std::istream::traits_type::eof() &&
          // On some environment (e.g. Mac OS 10.8 w/ Xcode 4.5),
          // peek() does not flip eofbit.  So calling get() is also
          // required.
          input_stream->get() == std::istream::traits_type::eof() &&
          input_stream->eof());
}

}  // namespace

class ConfigFileStreamTest : public testing::Test {
 protected:
  void SetUp() override {
    default_profile_directory_ = SystemUtil::GetUserProfileDirectory();
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
  }

  void TearDown() override {
    SystemUtil::SetUserProfileDirectory(default_profile_directory_);
  }

 private:
  std::string default_profile_directory_;
};

TEST_F(ConfigFileStreamTest, OnMemoryFiles) {
  const std::string kData = "data";
  const std::string kPath = "memory://test";
  EXPECT_TRUE(ConfigFileStream::GetFileName(kPath).empty());
  ConfigFileStream::AtomicUpdate(kPath, kData);

  {
    std::unique_ptr<std::istream> ifs(ConfigFileStream::LegacyOpen(kPath));
    ASSERT_NE(nullptr, ifs.get());
    std::unique_ptr<char[]> buf(new char[kData.size() + 1]);
    ifs->read(buf.get(), kData.size());
    buf.get()[kData.size()] = '\0';
    EXPECT_EQ(kData, buf.get());
    EXPECT_TRUE(IsEof(ifs.get()));
  }

  ConfigFileStream::ClearOnMemoryFiles();

  {
    std::unique_ptr<std::istream> ifs(ConfigFileStream::LegacyOpen(kPath));
    ASSERT_NE(nullptr, ifs.get());
    EXPECT_TRUE(IsEof(ifs.get()));
  }
}

TEST_F(ConfigFileStreamTest, AtomicUpdate) {
  const std::string prefixed_filename = "user://atomic_update_test";
  const std::string filename = ConfigFileStream::GetFileName(prefixed_filename);
  const std::string tmp_filename = filename + ".tmp";

  EXPECT_FALSE(FileUtil::FileExists(filename));
  EXPECT_FALSE(FileUtil::FileExists(tmp_filename));

  const std::string contents = "123\n2\n3";
  ConfigFileStream::AtomicUpdate(prefixed_filename, contents);
  EXPECT_TRUE(FileUtil::FileExists(filename));
  EXPECT_FALSE(FileUtil::FileExists(tmp_filename));
  EXPECT_EQ(contents, GetFileData(filename));

  const std::string new_contents = "246\n4\n6";
  ConfigFileStream::AtomicUpdate(prefixed_filename, new_contents);
  EXPECT_TRUE(FileUtil::FileExists(filename));
  EXPECT_FALSE(FileUtil::FileExists(tmp_filename));
  EXPECT_EQ(new_contents, GetFileData(filename));

  if (FileUtil::FileExists(filename)) {
    FileUtil::Unlink(filename);
  }
}

TEST_F(ConfigFileStreamTest, OpenReadBinary) {
  // At first, generate a binary data file in (temporary) user directory
  // so that we can load it as "user://my_binary_file.dat"
  const char kTestFileName[] = "my_binary_file.dat";
  const std::string &test_file_path =
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), kTestFileName);

  const char kBinaryData[] = {
      ' ', ' ', '\r', ' ', '\n', ' ', '\r', '\n', ' ', '\0', ' ',
  };
  const size_t kBinaryDataSize = sizeof(kBinaryData);
  {
    OutputFileStream ofs(test_file_path.c_str(),
                         std::ios::out | std::ios::binary);
    ofs.write(kBinaryData, kBinaryDataSize);
  }

  ASSERT_TRUE(FileUtil::FileExists(test_file_path));

  {
    std::unique_ptr<std::istream> ifs(ConfigFileStream::OpenReadBinary(
        "user://" + std::string(kTestFileName)));
    ASSERT_NE(nullptr, ifs.get());
    std::unique_ptr<char[]> buf(new char[kBinaryDataSize]);
    ifs->read(buf.get(), kBinaryDataSize);
    // Check if all the data are loaded as binary mode.
    for (size_t i = 0; i < kBinaryDataSize; ++i) {
      EXPECT_EQ(static_cast<int>(kBinaryData[i]), static_cast<int>(buf[i]));
    }
    EXPECT_TRUE(IsEof(ifs.get()));
  }

  // Remove test file just in case.
  EXPECT_TRUE(FileUtil::Unlink(test_file_path));
  EXPECT_FALSE(FileUtil::FileExists(test_file_path));
}

TEST_F(ConfigFileStreamTest, OpenReadText) {
  // At first, generate a binary data file in (temporary) user directory
  // so that we can load it as "user://my_binary_file.dat"
  const char kTestFileName[] = "my_text_file.dat";
  const std::string &test_file_path =
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), kTestFileName);

  const char kSourceTextData[] = {
      'a', 'b', '\r', 'c', '\n', 'd', '\r', '\n', 'e',
  };
  {
    // Use |ios::binary| to preserve the line-end character.
    OutputFileStream ofs(test_file_path.c_str(),
                         std::ios::out | std::ios::binary);
    ofs.write(kSourceTextData, sizeof(kSourceTextData));
  }

  ASSERT_TRUE(FileUtil::FileExists(test_file_path));

#ifdef OS_WIN
#define TRAILING_CARRIAGE_RETURN ""
#else
#define TRAILING_CARRIAGE_RETURN "\r"
#endif
  const char *kExpectedLines[] = {
      "ab\rc",
      "d" TRAILING_CARRIAGE_RETURN,
      "e",
  };
#undef TRAILING_CARRIAGE_RETURN

  {
    std::unique_ptr<std::istream> ifs(
        ConfigFileStream::OpenReadText("user://" + std::string(kTestFileName)));
    ASSERT_NE(nullptr, ifs.get());
    std::string line;
    int line_number = 0;  // note that this is 1-origin.
    while (!std::getline(*ifs, line).fail()) {
      ++line_number;
      ASSERT_LE(line_number, arraysize(kExpectedLines));
      EXPECT_EQ(line, kExpectedLines[line_number - 1])
          << "failed at line: " << line_number;
    }
  }

  // Remove test file just in case.
  EXPECT_TRUE(FileUtil::Unlink(test_file_path));
  EXPECT_FALSE(FileUtil::FileExists(test_file_path));
}

}  // namespace mozc

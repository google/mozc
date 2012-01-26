// Copyright 2010-2012, Google Inc.
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

#include <ios>  // for char_traits

#include "base/base.h"
#include "base/config_file_stream.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

namespace {
// Returns all data of the filename.
string GetFileData(const string &filename) {
  InputFileStream ifs(filename.c_str(), ios::binary);
  char c;
  string data;
  while (ifs.get(c)) {
    data.append(1, c);
  }
  return data;
}
}  // anonymous namespace

TEST(ConfigFileStreamTest, OnMemoryFiles) {
  const string kData = "data";
  const string kPath = "memory://test";
  EXPECT_TRUE(ConfigFileStream::GetFileName(kPath).empty());
  ConfigFileStream::AtomicUpdate(kPath, kData);

  {
    scoped_ptr<istream> ifs(ConfigFileStream::Open(kPath));
    CHECK(ifs.get());
    scoped_array<char> buf(new char[kData.size() + 1]);
    ifs->read(buf.get(), kData.size());
    buf.get()[kData.size()] = '\0';
    EXPECT_EQ(kData, buf.get());
    // eof() does not work well for istringstream for some reasons, so
    // here just uses peek() instead.
    EXPECT_EQ(-1, ifs->peek());
  }

  ConfigFileStream::ClearOnMemoryFiles();

  {
    scoped_ptr<istream> ifs(ConfigFileStream::Open(kPath));
    CHECK(ifs.get());
    // eof() does not work well for istringstream for some reasons, so
    // here just uses peek() instead.
    EXPECT_EQ(-1, ifs->peek());
  }
}

TEST(ConfigFileStreamTest, AtomicUpdate) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  const string prefixed_filename = "user://atomic_update_test";
  const string filename = ConfigFileStream::GetFileName(prefixed_filename);
  const string tmp_filename = filename + ".tmp";

  EXPECT_FALSE(Util::FileExists(filename));
  EXPECT_FALSE(Util::FileExists(tmp_filename));

  const string contents = "123\n2\n3";
  ConfigFileStream::AtomicUpdate(prefixed_filename, contents);
  EXPECT_TRUE(Util::FileExists(filename));
  EXPECT_FALSE(Util::FileExists(tmp_filename));
  EXPECT_EQ(contents, GetFileData(filename));

  const string new_contents = "246\n4\n6";
  ConfigFileStream::AtomicUpdate(prefixed_filename, new_contents);
  EXPECT_TRUE(Util::FileExists(filename));
  EXPECT_FALSE(Util::FileExists(tmp_filename));
  EXPECT_EQ(new_contents, GetFileData(filename));

  if (Util::FileExists(filename)) {
    Util::Unlink(filename);
  }
}

}  // namespace mozc

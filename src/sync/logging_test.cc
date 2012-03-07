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

#include <string>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "sync/logging.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);
DECLARE_int32(sync_verbose_level);

namespace mozc {
namespace sync {

class LoggingTest : public testing::Test {
 public:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    sync::Logging::Reset();
  }

  virtual void SetVerboseLevel(int vlevel) {
    FLAGS_sync_verbose_level = vlevel;
  }

  virtual size_t GetLines() {
    const string filename = sync::Logging::GetLogFileName();
    InputFileStream ifs(filename.c_str());
    string line;
    size_t lines = 0;
    while (getline(ifs, line)) {
      ++lines;
    }
    return lines;
  }

  virtual string GetLastLogLine() {
    const string filename = sync::Logging::GetLogFileName();
    InputFileStream ifs(filename.c_str());
    string line;
    string result;
    while (getline(ifs, line)) {
      result = line;
    }
    return result;
  }

  virtual bool ContainsInLastLine(const string &pattern) {
    const string line = GetLastLogLine();
    return line.find(pattern) != string::npos;
  }
};

TEST_F(LoggingTest, BasicTest) {
  {
    SetVerboseLevel(0);
    SYNC_VLOG(1) << "test1";
    SYNC_VLOG(2) << "test2";
    EXPECT_EQ(0, GetLines());
  }

  {
    SetVerboseLevel(1);
    SYNC_VLOG(1) << "test1";
    SYNC_VLOG(2) << "test2";
    EXPECT_EQ(1, GetLines());
    EXPECT_TRUE(ContainsInLastLine("test1"));
  }

  {
    SetVerboseLevel(2);
    SYNC_VLOG(1) << "test1";
    SYNC_VLOG(2) << "test2";
    EXPECT_EQ(3, GetLines());
    EXPECT_TRUE(ContainsInLastLine("test2"));
  }

  {
    SetVerboseLevel(0);
    SYNC_VLOG(1) << "test1";
    SYNC_VLOG(2) << "test2";
    EXPECT_EQ(3, GetLines());
  }
}

namespace {
int g_counter = 0;
string DebugString() {
  ++g_counter;
  return "debug string";
}
}  // namespace

TEST_F(LoggingTest, RightHandSideTest) {
  SetVerboseLevel(0);
  SYNC_VLOG(1) << DebugString();
  SYNC_VLOG(2) << DebugString();
  EXPECT_EQ(0, g_counter);

  SetVerboseLevel(1);
  SYNC_VLOG(1) << DebugString();
  SYNC_VLOG(2) << DebugString();
  EXPECT_EQ(1, g_counter);

  SetVerboseLevel(2);
  SYNC_VLOG(1) << DebugString();
  SYNC_VLOG(2) << DebugString();
  EXPECT_EQ(3, g_counter);
}

TEST_F(LoggingTest, TruncateTest) {
  SetVerboseLevel(1);
  size_t old_cur = 0;
  for (int i = 0; i < 500000; ++i) {
    const string pattern = Util::StringPrintf("Logging lines: %d", i);
    // pattern.size() is about 20byte.
    // 5 * 1024 * 1024 / 20 =~ 250000 lines will be emitted.
    EXPECT_GT(250000, i);
    SYNC_VLOG(1) << pattern;
    const size_t cur = sync::Logging::GetLogStream().tellp();
    if (old_cur > cur) {
      EXPECT_TRUE(ContainsInLastLine(pattern));
      break;
    }
    old_cur = cur;
  }
}
}  // sync
}  // mozc

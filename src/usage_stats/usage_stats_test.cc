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

#include "usage_stats/usage_stats.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/util.h"
#include "storage/registry.h"
#include "storage/tiny_storage.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.pb.h"


DECLARE_string(test_tmpdir);

namespace mozc {
namespace usage_stats {

class UsageStatsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    EXPECT_TRUE(storage::Registry::Clear());
  }
  virtual void TearDown() {
    EXPECT_TRUE(storage::Registry::Clear());
  }
};

namespace {
class TestStorage: public storage::StorageInterface {
 public:
  bool Open(const string &filename) { return true; }
  bool Sync() { return true; }
  bool Lookup(const string &key, string *value) const { return false; }
  // return false
  bool Insert(const string &key, const string &value) { return false; }
  bool Erase(const string &key) { return true; }
  bool Clear() { return true; }
  size_t Size() const { return 0; }
  TestStorage() {}
  virtual ~TestStorage() {}
};

}  // namespace

// for windows only
TEST_F(UsageStatsTest, GetMsctfVerTest) {
#ifdef OS_WINDOWS
  int major, minor, build, revision;
  const wchar_t kDllName[] = L"msctf.dll";
  wstring path = Util::GetSystemDir();
  path += L"\\";
  path += kDllName;
  EXPECT_TRUE(Util::GetFileVersion(path, &major, &minor, &build, &revision));
#endif
}

TEST_F(UsageStatsTest, IsListedTest) {
  EXPECT_TRUE(UsageStats::IsListed("Commit"));
  EXPECT_FALSE(UsageStats::IsListed("WeDoNotDefinedThisStats"));
}


}  // namespace mozc::usage_stats
}  // namespace mozc

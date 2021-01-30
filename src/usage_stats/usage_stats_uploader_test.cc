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

#include "usage_stats/usage_stats_uploader.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/flags.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/version.h"
#include "storage/registry.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.h"

namespace mozc {
namespace usage_stats {
namespace {

void SetUpMetaDataWithMozcVersion(uint32 last_upload_time,
                                  const std::string &mozc_version) {
  EXPECT_TRUE(
      storage::Registry::Insert("usage_stats.last_upload", last_upload_time));
  EXPECT_TRUE(
      storage::Registry::Insert("usage_stats.mozc_version", mozc_version));
}

void SetUpMetaData(uint32 last_upload_time) {
  SetUpMetaDataWithMozcVersion(last_upload_time, Version::GetMozcVersion());
}

class UsageStatsUploaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(mozc::GetFlag(FLAGS_test_tmpdir));
    EXPECT_TRUE(storage::Registry::Clear());
  }

  void TearDown() override { EXPECT_TRUE(storage::Registry::Clear()); }
};

TEST_F(UsageStatsUploaderTest, SendTest) {
  SetUpMetaData(100000);

  EXPECT_TRUE(UsageStatsUploader::Send(nullptr));

  // Metadata should be deleted
  uint32 recorded_sec;
  std::string recorded_version;
  EXPECT_FALSE(
      storage::Registry::Lookup("usage_stats.last_upload", &recorded_sec));
  EXPECT_FALSE(
      storage::Registry::Lookup("usage_stats.mozc_version", &recorded_version));
}

TEST_F(UsageStatsUploaderTest, SendTest_DeleteExistingClientId) {
  const std::string store_value = "some_value";
  storage::Registry::Insert("usage_stats.client_id", store_value);
  std::string client_id;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.client_id", &client_id));
  EXPECT_EQ("some_value", client_id);

  EXPECT_TRUE(UsageStatsUploader::Send(nullptr));

  // Metadata should be deleted
  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.client_id", &client_id));
}

}  // namespace
}  // namespace usage_stats
}  // namespace mozc

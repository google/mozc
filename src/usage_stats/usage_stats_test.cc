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

#include "usage_stats/usage_stats.h"

#include <cstdint>
#include <map>
#include <string>

#include "base/port.h"
#include "base/system_util.h"
#include "config/stats_config_util.h"
#include "config/stats_config_util_mock.h"
#include "storage/registry.h"
#include "storage/storage_interface.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.pb.h"
#include "absl/flags/flag.h"

namespace mozc {
namespace usage_stats {

class UsageStatsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
    EXPECT_TRUE(storage::Registry::Clear());
    mozc::config::StatsConfigUtil::SetHandler(&stats_config_util_);
  }
  void TearDown() override {
    mozc::config::StatsConfigUtil::SetHandler(nullptr);
    EXPECT_TRUE(storage::Registry::Clear());
  }

 private:
  mozc::config::StatsConfigUtilMock stats_config_util_;
};

TEST_F(UsageStatsTest, IsListedTest) {
  EXPECT_TRUE(UsageStats::IsListed("Commit"));
  EXPECT_FALSE(UsageStats::IsListed("WeDoNotDefinedThisStats"));
}

TEST_F(UsageStatsTest, StoreTest) {
  // Use actual items, but they do not matter with practical usages.
  const char kCountKey[] = "ShutDown";
  const char kIntegerKey[] = "UserRegisteredWord";
  const char kBooleanKey[] = "ConfigUseDictionarySuggest";
  const char kTimingKey[] = "ElapsedTimeUSec";
  const char kVirtualKeyboardKey[] = "VirtualKeyboardStats";

  uint32_t count_val = 0;
  int32_t integer_val = -1;
  bool boolean_val = false;
  uint64_t total_time = 0;
  uint32_t num_timings = 0;
  uint32_t avg_time = 0;
  uint32_t min_time = 0;
  uint32_t max_time = 0;
  Stats virtual_keyboard_val;
  Stats stats_val;
  // Cannot get values when no values are registered.
  EXPECT_FALSE(UsageStats::GetCountForTest(kCountKey, &count_val));
  EXPECT_FALSE(UsageStats::GetIntegerForTest(kIntegerKey, &integer_val));
  EXPECT_FALSE(UsageStats::GetBooleanForTest(kBooleanKey, &boolean_val));
  EXPECT_FALSE(UsageStats::GetTimingForTest(
      kTimingKey, &total_time, &num_timings, &avg_time, &min_time, &max_time));
  EXPECT_FALSE(UsageStats::GetVirtualKeyboardForTest(kVirtualKeyboardKey,
                                                     &virtual_keyboard_val));
  EXPECT_FALSE(UsageStats::GetStatsForTest(kCountKey, &stats_val));

  // Update stats.
  UsageStats::IncrementCount(kCountKey);
  UsageStats::SetInteger(kIntegerKey, 10);
  UsageStats::SetBoolean(kBooleanKey, true);
  UsageStats::UpdateTiming(kTimingKey, 5);
  UsageStats::UpdateTiming(kTimingKey, 7);
  std::map<std::string, TouchEventStatsMap> inserted_virtual_keyboard_val;
  inserted_virtual_keyboard_val["test1"] = TouchEventStatsMap();
  UsageStats::StoreTouchEventStats(kVirtualKeyboardKey,
                                   inserted_virtual_keyboard_val);

  // Cannot get with wrong types.
  EXPECT_FALSE(UsageStats::GetCountForTest(kIntegerKey, &count_val));
  EXPECT_FALSE(UsageStats::GetIntegerForTest(kBooleanKey, &integer_val));
  EXPECT_FALSE(UsageStats::GetBooleanForTest(kTimingKey, &boolean_val));
  EXPECT_FALSE(UsageStats::GetTimingForTest(
      kVirtualKeyboardKey, nullptr, nullptr, nullptr, nullptr, nullptr));
  EXPECT_FALSE(
      UsageStats::GetVirtualKeyboardForTest(kCountKey, &virtual_keyboard_val));

  // Check values.
  EXPECT_FALSE(UsageStats::GetCountForTest(kCountKey, &count_val));
  EXPECT_FALSE(UsageStats::GetIntegerForTest(kIntegerKey, &integer_val));
  EXPECT_FALSE(UsageStats::GetBooleanForTest(kBooleanKey, &boolean_val));
  EXPECT_FALSE(UsageStats::GetTimingForTest(kTimingKey, &total_time, nullptr,
                                            nullptr, nullptr, nullptr));
  EXPECT_FALSE(UsageStats::GetTimingForTest(kTimingKey, nullptr, &num_timings,
                                            nullptr, nullptr, nullptr));
  EXPECT_FALSE(UsageStats::GetTimingForTest(kTimingKey, nullptr, nullptr,
                                            &avg_time, nullptr, nullptr));
  EXPECT_FALSE(UsageStats::GetTimingForTest(kTimingKey, nullptr, nullptr,
                                            nullptr, &min_time, nullptr));
  EXPECT_FALSE(UsageStats::GetTimingForTest(kTimingKey, nullptr, nullptr,
                                            nullptr, nullptr, &max_time));
  EXPECT_FALSE(UsageStats::GetStatsForTest(kCountKey, &stats_val));
  EXPECT_FALSE(UsageStats::GetVirtualKeyboardForTest(kVirtualKeyboardKey,
                                                     &virtual_keyboard_val));
}

namespace {
void SetDoubleValueStats(uint32_t num, double total, double square_total,
                         usage_stats::Stats::DoubleValueStats *double_stats) {
  double_stats->set_num(num);
  double_stats->set_total(total);
  double_stats->set_square_total(square_total);
}

void SetEventStats(uint32_t source_id, uint32_t sx_num, double sx_total,
                   double sx_square_total, uint32_t sy_num, double sy_total,
                   double sy_square_total, uint32_t dx_num, double dx_total,
                   double dx_square_total, uint32_t dy_num, double dy_total,
                   double dy_square_total, uint32_t tl_num, double tl_total,
                   double tl_square_total,
                   usage_stats::Stats::TouchEventStats *event_stats) {
  event_stats->set_source_id(source_id);
  SetDoubleValueStats(sx_num, sx_total, sx_square_total,
                      event_stats->mutable_start_x_stats());
  SetDoubleValueStats(sy_num, sy_total, sy_square_total,
                      event_stats->mutable_start_y_stats());
  SetDoubleValueStats(dx_num, dx_total, dx_square_total,
                      event_stats->mutable_direction_x_stats());
  SetDoubleValueStats(dy_num, dy_total, dy_square_total,
                      event_stats->mutable_direction_y_stats());
  SetDoubleValueStats(tl_num, tl_total, tl_square_total,
                      event_stats->mutable_time_length_stats());
}
}  // namespace

TEST_F(UsageStatsTest, StoreTouchEventStats) {
  std::string stats_str;
  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.VirtualKeyboardStats",
                                         &stats_str));
  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.VirtualKeyboardMissStats",
                                         &stats_str));
  std::map<std::string, std::map<uint32_t, Stats::TouchEventStats> >
      touch_stats;

  Stats::TouchEventStats &event_stats1 = touch_stats["KEYBOARD_01"][10];
  SetEventStats(10, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                &event_stats1);

  Stats::TouchEventStats &event_stats2 = touch_stats["KEYBOARD_01"][20];
  SetEventStats(20, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
                114, 115, 116, &event_stats2);

  Stats::TouchEventStats &event_stats3 = touch_stats["KEYBOARD_02"][10];
  SetEventStats(10, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213,
                214, 215, 216, &event_stats3);

  UsageStats::StoreTouchEventStats("VirtualKeyboardStats", touch_stats);

  // Does not store usage stats anymore.
  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.VirtualKeyboardStats",
                                         &stats_str));
  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.VirtualKeyboardMissStats",
                                         &stats_str));
}

}  // namespace usage_stats
}  // namespace mozc

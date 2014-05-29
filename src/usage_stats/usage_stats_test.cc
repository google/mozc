// Copyright 2010-2014, Google Inc.
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

#include <map>
#include <string>

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "base/system_util.h"
#include "config/stats_config_util.h"
#include "config/stats_config_util_mock.h"
#include "storage/registry.h"
#include "storage/storage_interface.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.pb.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace usage_stats {

class UsageStatsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    EXPECT_TRUE(storage::Registry::Clear());
    mozc::config::StatsConfigUtil::SetHandler(&stats_config_util_);
  }
  virtual void TearDown() {
    mozc::config::StatsConfigUtil::SetHandler(NULL);
    EXPECT_TRUE(storage::Registry::Clear());
  }

 private:
  mozc::config::StatsConfigUtilMock stats_config_util_;
};

TEST_F(UsageStatsTest, IsListedTest) {
  EXPECT_TRUE(UsageStats::IsListed("Commit"));
  EXPECT_FALSE(UsageStats::IsListed("WeDoNotDefinedThisStats"));
}

TEST_F(UsageStatsTest, GetterTest) {
  // Use actual items, but they do not matter with practical usages.
  const char kCountKey[] = "ShutDown";
  const char kIntegerKey[] = "UserRegisteredWord";
  const char kBooleanKey[] = "CuasEnabled";
  const char kTimingKey[] = "ElapsedTimeUSec";
  const char kVirtualKeyboardKey[] = "VirtualKeyboardStats";

  uint32 count_val = 0;
  int32 integer_val = -1;
  bool boolean_val = false;
  uint64 total_time = 0;
  uint32 num_timings = 0;
  uint32 avg_time = 0;
  uint32 min_time = 0;
  uint32 max_time = 0;
  Stats virtual_keyboard_val;
  Stats stats_val;
  // Cannot get values when no values are registered.
  EXPECT_FALSE(UsageStats::GetCountForTest(kCountKey, &count_val));
  EXPECT_FALSE(UsageStats::GetIntegerForTest(kIntegerKey, &integer_val));
  EXPECT_FALSE(UsageStats::GetBooleanForTest(kBooleanKey, &boolean_val));
  EXPECT_FALSE(UsageStats::GetTimingForTest(kTimingKey, &total_time,
                   &num_timings, &avg_time, &min_time, &max_time));
  EXPECT_FALSE(UsageStats::GetVirtualKeyboardForTest(kVirtualKeyboardKey,
                                                     &virtual_keyboard_val));
  EXPECT_FALSE(UsageStats::GetStatsForTest(kCountKey, &stats_val));

  // Update stats.
  UsageStats::IncrementCount(kCountKey);
  UsageStats::SetInteger(kIntegerKey, 10);
  UsageStats::SetBoolean(kBooleanKey, true);
  UsageStats::UpdateTiming(kTimingKey, 5);
  UsageStats::UpdateTiming(kTimingKey, 7);
  map<string, TouchEventStatsMap> inserted_virtual_keyboard_val;
  inserted_virtual_keyboard_val["test1"] = TouchEventStatsMap();
  UsageStats::StoreTouchEventStats(kVirtualKeyboardKey,
                                   inserted_virtual_keyboard_val);

  // Cannot get with wrong types.
  EXPECT_FALSE(UsageStats::GetCountForTest(kIntegerKey, &count_val));
  EXPECT_FALSE(UsageStats::GetIntegerForTest(kBooleanKey, &integer_val));
  EXPECT_FALSE(UsageStats::GetBooleanForTest(kTimingKey, &boolean_val));
  EXPECT_FALSE(UsageStats::GetTimingForTest(kVirtualKeyboardKey, NULL, NULL,
                                            NULL, NULL, NULL));
  EXPECT_FALSE(UsageStats::GetVirtualKeyboardForTest(kCountKey,
                                                     &virtual_keyboard_val));

  // Check values.
  EXPECT_TRUE(UsageStats::GetCountForTest(kCountKey, &count_val));
  EXPECT_TRUE(UsageStats::GetIntegerForTest(kIntegerKey, &integer_val));
  EXPECT_TRUE(UsageStats::GetBooleanForTest(kBooleanKey, &boolean_val));
  EXPECT_TRUE(UsageStats::GetTimingForTest(kTimingKey, &total_time,
                                           NULL, NULL, NULL, NULL));
  EXPECT_TRUE(UsageStats::GetTimingForTest(kTimingKey, NULL, &num_timings,
                                           NULL, NULL, NULL));
  EXPECT_TRUE(UsageStats::GetTimingForTest(kTimingKey, NULL, NULL, &avg_time,
                                           NULL, NULL));
  EXPECT_TRUE(UsageStats::GetTimingForTest(kTimingKey, NULL, NULL, NULL,
                                           &min_time, NULL));
  EXPECT_TRUE(UsageStats::GetTimingForTest(kTimingKey, NULL, NULL, NULL,
                                           NULL, &max_time));
  EXPECT_TRUE(UsageStats::GetStatsForTest(kCountKey, &stats_val));
  EXPECT_TRUE(UsageStats::GetVirtualKeyboardForTest(kVirtualKeyboardKey,
                                                    &virtual_keyboard_val));
  EXPECT_EQ(1, count_val);
  EXPECT_EQ(10, integer_val);
  EXPECT_TRUE(boolean_val);
  EXPECT_EQ(12, total_time);
  EXPECT_EQ(2, num_timings);
  EXPECT_EQ(6, avg_time);
  EXPECT_EQ(5, min_time);
  EXPECT_EQ(7, max_time);
  EXPECT_EQ(Stats::COUNT, stats_val.type());
  EXPECT_EQ(1, virtual_keyboard_val.virtual_keyboard_stats_size());
  EXPECT_EQ(1, stats_val.count());

  // Update stats.
  UsageStats::IncrementCount(kCountKey);
  UsageStats::SetInteger(kIntegerKey, 15);
  UsageStats::SetBoolean(kBooleanKey, false);
  UsageStats::UpdateTiming(kTimingKey, 9);
  inserted_virtual_keyboard_val.clear();
  inserted_virtual_keyboard_val["test2"] = TouchEventStatsMap();
  UsageStats::StoreTouchEventStats(kVirtualKeyboardKey,
                                   inserted_virtual_keyboard_val);

  // Check updated values.
  EXPECT_TRUE(UsageStats::GetCountForTest(kCountKey, &count_val));
  EXPECT_TRUE(UsageStats::GetIntegerForTest(kIntegerKey, &integer_val));
  EXPECT_TRUE(UsageStats::GetBooleanForTest(kBooleanKey, &boolean_val));
  EXPECT_TRUE(UsageStats::GetTimingForTest(kTimingKey, &total_time,
                                           &num_timings, &avg_time, &min_time,
                                           &max_time));
  EXPECT_TRUE(UsageStats::GetStatsForTest(kCountKey, &stats_val));
  EXPECT_TRUE(UsageStats::GetVirtualKeyboardForTest(kVirtualKeyboardKey,
                                                    &virtual_keyboard_val));
  EXPECT_EQ(2, count_val);
  EXPECT_EQ(15, integer_val);
  EXPECT_FALSE(boolean_val);
  EXPECT_EQ(21, total_time);
  EXPECT_EQ(3, num_timings);
  EXPECT_EQ(7, avg_time);
  EXPECT_EQ(5, min_time);
  EXPECT_EQ(9, max_time);
  EXPECT_EQ(Stats::COUNT, stats_val.type());
  EXPECT_EQ(2, virtual_keyboard_val.virtual_keyboard_stats_size());
  EXPECT_EQ(2, stats_val.count());
}

namespace {
void SetDoubleValueStats(
    uint32 num, double total, double square_total,
    usage_stats::Stats::DoubleValueStats *double_stats) {
  double_stats->set_num(num);
  double_stats->set_total(total);
  double_stats->set_square_total(square_total);
}

void SetEventStats(
    uint32 source_id,
    uint32 sx_num, double sx_total, double sx_square_total,
    uint32 sy_num, double sy_total, double sy_square_total,
    uint32 dx_num, double dx_total, double dx_square_total,
    uint32 dy_num, double dy_total, double dy_square_total,
    uint32 tl_num, double tl_total, double tl_square_total,
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
  string stats_str;
  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.VirtualKeyboardStats",
                                         &stats_str));
  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.VirtualKeyboardMissStats",
                                         &stats_str));
  map<string, map<uint32, Stats::TouchEventStats> > touch_stats;

  Stats::TouchEventStats &event_stats1 = touch_stats["KEYBOARD_01"][10];
  SetEventStats(10, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                14, 15, 16, &event_stats1);

  Stats::TouchEventStats &event_stats2 = touch_stats["KEYBOARD_01"][20];
  SetEventStats(20, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
                114, 115, 116, &event_stats2);

  Stats::TouchEventStats &event_stats3 = touch_stats["KEYBOARD_02"][10];
  SetEventStats(10, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213,
                214, 215, 216, &event_stats3);

  UsageStats::StoreTouchEventStats("VirtualKeyboardStats", touch_stats);

  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.VirtualKeyboardStats",
                                         &stats_str));

  Stats stats;
  EXPECT_TRUE(stats.ParseFromString(stats_str));
  EXPECT_EQ(Stats::VIRTUAL_KEYBOARD, stats.type());
  EXPECT_EQ(2, stats.virtual_keyboard_stats_size());
  EXPECT_EQ("KEYBOARD_01",
      stats.virtual_keyboard_stats(0).keyboard_name());
  EXPECT_EQ("KEYBOARD_02",
      stats.virtual_keyboard_stats(1).keyboard_name());
  EXPECT_EQ(2, stats.virtual_keyboard_stats(0).touch_event_stats_size());
  EXPECT_EQ(1, stats.virtual_keyboard_stats(1).touch_event_stats_size());

  EXPECT_EQ(event_stats1.DebugString(),
      stats.virtual_keyboard_stats(0).touch_event_stats(0).DebugString());
  EXPECT_EQ(event_stats2.DebugString(),
      stats.virtual_keyboard_stats(0).touch_event_stats(1).DebugString());
  EXPECT_EQ(event_stats3.DebugString(),
      stats.virtual_keyboard_stats(1).touch_event_stats(0).DebugString());


  map<string, map<uint32, Stats::TouchEventStats> > touch_stats2;
  Stats::TouchEventStats &event_stats4 = touch_stats2["KEYBOARD_01"][20];
  SetEventStats(20, 302, 303, 304, 305, 306, 307, 308, 309, 310, 311, 312, 313,
                314, 315, 316, &event_stats4);
  Stats::TouchEventStats &event_stats5 = touch_stats2["KEYBOARD_02"][20];
  SetEventStats(20, 402, 403, 404, 405, 406, 407, 408, 409, 410, 411, 412, 413,
                414, 415, 416, &event_stats5);

  UsageStats::StoreTouchEventStats("VirtualKeyboardStats", touch_stats2);

  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.VirtualKeyboardStats",
                                         &stats_str));

  Stats stats2;
  EXPECT_TRUE(stats2.ParseFromString(stats_str));
  EXPECT_EQ(Stats::VIRTUAL_KEYBOARD, stats2.type());
  EXPECT_EQ(2, stats2.virtual_keyboard_stats_size());
  EXPECT_EQ("KEYBOARD_01",
      stats2.virtual_keyboard_stats(0).keyboard_name());
  EXPECT_EQ("KEYBOARD_02",
      stats2.virtual_keyboard_stats(1).keyboard_name());
  EXPECT_EQ(2, stats2.virtual_keyboard_stats(0).touch_event_stats_size());
  EXPECT_EQ(2, stats2.virtual_keyboard_stats(1).touch_event_stats_size());

  Stats::TouchEventStats event_stats6;
  SetEventStats(20, 404, 406, 408, 410, 412, 414, 416, 418, 420, 422, 424, 426,
                428, 430, 432, &event_stats6);

  EXPECT_EQ(event_stats1.DebugString(),
      stats2.virtual_keyboard_stats(0).touch_event_stats(0).DebugString());
  EXPECT_EQ(event_stats6.DebugString(),
      stats2.virtual_keyboard_stats(0).touch_event_stats(1).DebugString());
  EXPECT_EQ(event_stats3.DebugString(),
      stats2.virtual_keyboard_stats(1).touch_event_stats(0).DebugString());
  EXPECT_EQ(event_stats5.DebugString(),
      stats2.virtual_keyboard_stats(1).touch_event_stats(1).DebugString());

  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.VirtualKeyboardMissStats",
                                         &stats_str));
}

}  // namespace usage_stats
}  // namespace mozc

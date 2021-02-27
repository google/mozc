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

#ifndef MOZC_USAGE_STATS_USAGE_STATS_H_
#define MOZC_USAGE_STATS_USAGE_STATS_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "base/port.h"
#include "usage_stats/usage_stats.pb.h"

namespace mozc {
namespace usage_stats {
typedef std::map<uint32_t, Stats::TouchEventStats> TouchEventStatsMap;

class UsageStats {
 public:
  // Updates count value
  // Increments val to current value
  static void IncrementCountBy(const std::string &name, uint32_t val);
  static void IncrementCount(const std::string &name) {
    IncrementCountBy(name, 1);
  }

  // Updates timing value
  // Updates current value using given val
  static void UpdateTiming(const std::string &name, uint32_t val);

  // Sets integer value
  // Replaces old value with val
  static void SetInteger(const std::string &name, int val);

  // Sets boolean value
  // Replaces old value with val
  static void SetBoolean(const std::string &name, bool val);

  // Returns true if given stats name is in the stats list
  // (for debugging)
  static bool IsListed(const std::string &name);

  // Stores virtual keyboard touch event stats.
  // The map "touch_stats" structure is as following
  //   (keyboard_name_01 : (source_id_1 : TouchEventStats,
  //                        source_id_2 : TouchEventStats,
  //                        source_id_3 : TouchEventStats),
  //    keyboard_name_02 : (source_id_1 : TouchEventStats,
  //                        source_id_2 : TouchEventStats,
  //                        source_id_3 : TouchEventStats))
  static void StoreTouchEventStats(
      const std::string &name,
      const std::map<std::string, TouchEventStatsMap> &touch_stats);

  // Synchronizes (writes) usage data into disk. Returns false on failure.
  static bool Sync();

  // Clears existing data except for Integer and Boolean stats.
  static void ClearStats();

  // Clears all data.
  static void ClearAllStats();

  static void ClearAllStatsForTest() { ClearAllStats(); }

  // NOTE: These methods are for unit tests.
  // Reads a value from registry, and sets it in the value.
  // Returns true if all steps go successfully.
  // NULL pointers are accetable for the target arguments of GetTimingForTest().
  static bool GetCountForTest(const std::string &name, uint32_t *value);
  static bool GetIntegerForTest(const std::string &name, int32_t *value);
  static bool GetBooleanForTest(const std::string &name, bool *value);
  static bool GetTimingForTest(const std::string &name, uint64_t *total_time,
                               uint32_t *num_timings, uint32_t *avg_time,
                               uint32_t *min_time, uint32_t *max_time);
  static bool GetVirtualKeyboardForTest(const std::string &name, Stats *stats);
  // This method doesn't check type of the stats.
  static bool GetStatsForTest(const std::string &name, Stats *stats);

  UsageStats() = delete;
  UsageStats(const UsageStats &) = delete;
  UsageStats &operator=(const UsageStats &) = delete;
};
}  // namespace usage_stats
}  // namespace mozc
#endif  // MOZC_USAGE_STATS_USAGE_STATS_H_

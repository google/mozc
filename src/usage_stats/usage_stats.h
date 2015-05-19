// Copyright 2010-2013, Google Inc.
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

#include <map>
#include <string>
#include <vector>
#include "base/port.h"

namespace mozc {
namespace usage_stats {
// We cannot use Stats::TouchEventStats because C++ does not allow
// forward declaration of a nested-type.
class Stats;
class Stats_TouchEventStats;

typedef map<uint32, Stats_TouchEventStats> TouchEventStatsMap;

class UsageStats {
 public:
  // Updates count value
  // Increments val to current value
  static void IncrementCountBy(const string &name, uint32 val);
  static void IncrementCount(const string &name) {
    IncrementCountBy(name, 1);
  }

  // Updates timing value
  // Updates current value using given val
  static void UpdateTiming(const string &name, uint32 val);

  // Sets integer value
  // Replaces old value with val
  static void SetInteger(const string &name, int val);

  // Sets boolean value
  // Replaces old value with val
  static void SetBoolean(const string &name, bool val);

  // Returns true if given stats name is in the stats list
  // (for debugging)
  static bool IsListed(const string &name);

  // Stores virtual keyboard touch event stats.
  // The map "touch_stats" structure is as following
  //   (keyboard_name_01 : (source_id_1 : TouchEventStats,
  //                        source_id_2 : TouchEventStats,
  //                        source_id_3 : TouchEventStats),
  //    keyboard_name_02 : (source_id_1 : TouchEventStats,
  //                        source_id_2 : TouchEventStats,
  //                        source_id_3 : TouchEventStats))
  static void StoreTouchEventStats(
      const string &name, const map<string, TouchEventStatsMap> &touch_stats);

  // Synchronizes (writes) usage data into disk. Returns false on failure.
  static bool Sync();

  // Clears existing data exept for Integer and Boolean stats.
  static void ClearStats();

  // Clears all data.
  static void ClearAllStatsForTest();

  // NOTE: These methods are for unit tests.
  // Reads a value from registry, and sets it in the value.
  // Returns true if all steps go successfully.
  // NULL pointers are accetable for the target arguments of GetTimingForTest().
  static bool GetCountForTest(const string &name, uint32 *value);
  static bool GetIntegerForTest(const string &name, int32 *value);
  static bool GetBooleanForTest(const string &name, bool *value);
  static bool GetTimingForTest(const string &name,
                               uint64 *total_time,
                               uint32 *num_timings,
                               uint32 *avg_time,
                               uint32 *min_time,
                               uint32 *max_time);
  static bool GetVirtualKeyboardForTest(const string &name, Stats *stats);
  // This method doesn't check type of the stats.
  static bool GetStatsForTest(const string &name, Stats *stats);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UsageStats);
};
}  // namespace usage_stats
}  // namespace mozc
#endif  // GOOCLECLIENT_IME_MOZC_USAGE_STATS_USAGE_STATS_H_

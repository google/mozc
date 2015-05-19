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

#include "usage_stats/usage_stats.h"

#include <algorithm>
#include <numeric>

#include "base/logging.h"
#include "config/stats_config_util.h"
#include "storage/registry.h"
#include "usage_stats/usage_stats.pb.h"

namespace mozc {
namespace usage_stats {

namespace {
const char kRegistryPrefix[] = "usage_stats.";

#include "usage_stats/usage_stats_list.h"

void AddDoubleValueStats(
    const Stats::DoubleValueStats &src,
    Stats::DoubleValueStats *dest) {
  dest->set_num(src.num() + dest->num());
  dest->set_total(src.total() + dest->total());
  dest->set_square_total(src.square_total() + dest->square_total());
}

void AddTouchEventStats(
    const Stats::TouchEventStats &src_stats,
    Stats::TouchEventStats *dest_stats) {
  dest_stats->set_source_id(src_stats.source_id());
  AddDoubleValueStats(src_stats.start_x_stats(),
                      dest_stats->mutable_start_x_stats());
  AddDoubleValueStats(src_stats.start_y_stats(),
                      dest_stats->mutable_start_y_stats());
  AddDoubleValueStats(src_stats.direction_x_stats(),
                      dest_stats->mutable_direction_x_stats());
  AddDoubleValueStats(src_stats.direction_y_stats(),
                      dest_stats->mutable_direction_y_stats());
  AddDoubleValueStats(src_stats.time_length_stats(),
                      dest_stats->mutable_time_length_stats());
}

bool LoadStats(const string &name, Stats *stats) {
  DCHECK(UsageStats::IsListed(name)) << name << " is not in the list";
  string stats_str;
  const string key = kRegistryPrefix + name;
  if (!mozc::storage::Registry::Lookup(key, &stats_str)) {
    VLOG(1) << "Usage stats " << name << " is not registered yet.";
    return false;
  }
  if (!stats->ParseFromString(stats_str)) {
    LOG(ERROR) << "Parse error";
    return false;
  }
  return true;
}

bool GetterInternal(const string &name, Stats::Type type, Stats *stats) {
  if (!LoadStats(name, stats)) {
    return false;
  }
  if (stats->type() != type) {
    LOG(ERROR) << "Type of " << name << " is not " << type
               << " but " << stats->type() << ".";
    return false;
  }
  return true;
}

bool SetterInternal(const string &name, const Stats &stats) {
  const string key = kRegistryPrefix + name;
  const string stats_str = stats.SerializeAsString();
  if (!storage::Registry::Insert(key, stats_str)) {
    LOG(ERROR) << "cannot save " << name << " to registry";
    return false;
  }
  return true;
}
}  // namespace

bool UsageStats::IsListed(const string &name) {
  for (size_t i = 0; i < arraysize(kStatsList); ++i) {
    if (name == kStatsList[i]) {
      return true;
    }
  }
  return false;
}

void UsageStats::ClearStats() {
  string stats_str;
  Stats stats;
  for (size_t i = 0; i < arraysize(kStatsList); ++i) {
    const string key = string(kRegistryPrefix) + kStatsList[i];
    if (storage::Registry::Lookup(key, &stats_str)) {
      if (!stats.ParseFromString(stats_str)) {
        storage::Registry::Erase(key);
      }
      if (stats.type() == Stats::INTEGER ||
          stats.type() == Stats::BOOLEAN) {
        // We do not clear integer/boolean stats.
        // These stats do not accumulate.
        // We want send these stats at the next time
        // even if they are not updated.
        continue;
      }
      storage::Registry::Erase(key);
    }
  }
}

void UsageStats::ClearAllStatsForTest() {
  for (size_t i = 0; i < arraysize(kStatsList); ++i) {
    const string key = string(kRegistryPrefix) + kStatsList[i];
    storage::Registry::Erase(key);
  }
}

void UsageStats::IncrementCountBy(const string &name, uint32 val) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  if (!config::StatsConfigUtil::IsEnabled()) {
    return;
  }

  Stats stats;
  if (GetterInternal(name, Stats::COUNT, &stats)) {
    stats.set_count(stats.count() + val);
  } else {
    stats.set_name(name);
    stats.set_type(Stats::COUNT);
    stats.set_count(val);
  }

  SetterInternal(name, stats);
}

void UsageStats::UpdateTiming(const string &name, uint32 val) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  if (!config::StatsConfigUtil::IsEnabled()) {
    return;
  }

  Stats stats;
  if (GetterInternal(name, Stats::TIMING, &stats)) {
    stats.set_num_timings(stats.num_timings() + 1);
    stats.set_total_time(stats.total_time() + val);
    stats.set_avg_time(stats.total_time() / stats.num_timings());
    stats.set_min_time(min(stats.min_time(), val));
    stats.set_max_time(max(stats.max_time(), val));
  } else {
    stats.set_name(name);
    stats.set_type(Stats::TIMING);
    stats.set_num_timings(1);
    stats.set_total_time(val);
    stats.set_avg_time(val);
    stats.set_min_time(val);
    stats.set_max_time(val);
  }

  SetterInternal(name, stats);
}

void UsageStats::SetInteger(const string &name, int val) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  if (!config::StatsConfigUtil::IsEnabled()) {
    return;
  }

  Stats stats;
  stats.set_name(name);
  stats.set_type(Stats::INTEGER);
  stats.set_int_value(val);

  SetterInternal(name, stats);
}

void UsageStats::SetBoolean(const string &name, bool val) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  if (!config::StatsConfigUtil::IsEnabled()) {
    return;
  }

  Stats stats;
  stats.set_name(name);
  stats.set_type(Stats::BOOLEAN);
  stats.set_boolean_value(val);

  SetterInternal(name, stats);
}

bool UsageStats::GetCountForTest(const string &name, uint32 *value) {
  CHECK(value != NULL);
  Stats stats;
  if (!GetterInternal(name, Stats::COUNT, &stats)) {
    return false;
  }
  if (!stats.has_count()) {
    LOG(WARNING) << name << " has no counts.";
    return false;
  }

  *value = stats.count();
  return true;
}

bool UsageStats::GetIntegerForTest(const string &name, int32 *value) {
  CHECK(value != NULL);
  Stats stats;
  if (!GetterInternal(name, Stats::INTEGER, &stats)) {
    return false;
  }
  if (!stats.has_int_value()) {
    LOG(WARNING) << name << " has no integer values.";
    return false;
  }

  *value = stats.int_value();
  return true;
}

bool UsageStats::GetBooleanForTest(const string &name, bool *value) {
  CHECK(value != NULL);
  Stats stats;
  if (!GetterInternal(name, Stats::BOOLEAN, &stats)) {
    return false;
  }
  if (!stats.has_boolean_value()) {
    LOG(WARNING) << name << " has no boolean values.";
    return false;
  }

  *value = stats.boolean_value();
  return true;
}

bool UsageStats::GetTimingForTest(const string &name,
                                  uint64 *total_time,
                                  uint32 *num_timings,
                                  uint32 *avg_time,
                                  uint32 *min_time,
                                  uint32 *max_time) {
  Stats stats;
  if (!GetterInternal(name, Stats::TIMING, &stats)) {
    return false;
  }

  if ((total_time != NULL && !stats.has_total_time()) ||
      (num_timings != NULL && !stats.has_num_timings()) ||
      (avg_time != NULL && !stats.has_avg_time()) ||
      (min_time != NULL && !stats.has_min_time()) ||
      (max_time != NULL && !stats.has_max_time())) {
    LOG(WARNING) << "cannot import stats of " << name << ".";
    return false;
  }

  if (total_time != NULL) {
    *total_time = stats.total_time();
  }
  if (num_timings != NULL) {
    *num_timings = stats.num_timings();
  }
  if (avg_time != NULL) {
    *avg_time = stats.avg_time();
  }
  if (min_time != NULL) {
    *min_time = stats.min_time();
  }
  if (max_time != NULL) {
    *max_time = stats.max_time();
  }

  return true;
}

bool UsageStats::GetVirtualKeyboardForTest(const string &name, Stats *stats) {
  if (!GetterInternal(name, Stats::VIRTUAL_KEYBOARD, stats)) {
    return false;
  }

  if (stats->virtual_keyboard_stats_size() == 0) {
    LOG(WARNING) << name << " has no virtual keyboard values.";
    stats->Clear();
    return false;
  }

  return true;
}

bool UsageStats::GetStatsForTest(const string &name, Stats *stats) {
  return LoadStats(name, stats);
}

void UsageStats::StoreTouchEventStats(
    const string &name, const map<string, TouchEventStatsMap> &touch_stats) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  if (touch_stats.empty()) {
    return;
  }

  Stats stats;
  map<string, TouchEventStatsMap> tmp_stats(touch_stats);
  if (GetterInternal(name, Stats::VIRTUAL_KEYBOARD, &stats)) {
    for (size_t i = 0; i < stats.virtual_keyboard_stats_size(); ++i) {
      const Stats::VirtualKeyboardStats &virtual_keyboard_stats =
          stats.virtual_keyboard_stats(i);
      const string &keyboard_name = virtual_keyboard_stats.keyboard_name();
      TouchEventStatsMap &stats_map = tmp_stats[keyboard_name];

      for (size_t j = 0; j < virtual_keyboard_stats.touch_event_stats_size();
          ++j) {
        const Stats::TouchEventStats &src_stats =
            virtual_keyboard_stats.touch_event_stats(j);
        Stats::TouchEventStats &dest_stats = stats_map[src_stats.source_id()];
        AddTouchEventStats(src_stats, &dest_stats);
      }
    }
  } else {
    stats.set_name(name);
    stats.set_type(Stats::VIRTUAL_KEYBOARD);
  }

  stats.clear_virtual_keyboard_stats();
  for (map<string, TouchEventStatsMap>::const_iterator iter =
           tmp_stats.begin();
       iter != tmp_stats.end(); ++iter) {
    Stats::VirtualKeyboardStats *virtual_keyboard_stats =
        stats.add_virtual_keyboard_stats();
    virtual_keyboard_stats->set_keyboard_name(iter->first);
    for (TouchEventStatsMap::const_iterator it = iter->second.begin();
        it != iter->second.end(); ++it) {
      Stats::TouchEventStats *touch_event_stats =
          virtual_keyboard_stats->add_touch_event_stats();
      touch_event_stats->CopyFrom(it->second);
    }
  }

  SetterInternal(name, stats);
}

bool UsageStats::Sync() {
  if (!storage::Registry::Sync()) {
    LOG(ERROR) << "sync failed";
    return false;
  }
  return true;
}

}  // namespace usage_stats
}  // namespace mozc

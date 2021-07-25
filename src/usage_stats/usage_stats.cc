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

#include <algorithm>
#include <cstdint>
#include <numeric>

#include "base/logging.h"
#include "config/stats_config_util.h"
#include "storage/registry.h"
#include "usage_stats/usage_stats.pb.h"
#include "usage_stats/usage_stats_uploader.h"

namespace mozc {
namespace usage_stats {

namespace {
constexpr char kRegistryPrefix[] = "usage_stats.";

#include "usage_stats/usage_stats_list.h"

bool LoadStats(const std::string &name, Stats *stats) {
  DCHECK(UsageStats::IsListed(name)) << name << " is not in the list";
  std::string stats_str;
  const std::string key = kRegistryPrefix + name;
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

bool GetterInternal(const std::string &name, Stats::Type type, Stats *stats) {
  if (!LoadStats(name, stats)) {
    return false;
  }
  if (stats->type() != type) {
    LOG(ERROR) << "Type of " << name << " is not " << type << " but "
               << stats->type() << ".";
    return false;
  }
  return true;
}

}  // namespace

bool UsageStats::IsListed(const std::string &name) {
  for (size_t i = 0; i < arraysize(kStatsList); ++i) {
    if (name == kStatsList[i]) {
      return true;
    }
  }
  return false;
}

void UsageStats::ClearStats() {
  std::string stats_str;
  Stats stats;
  for (size_t i = 0; i < arraysize(kStatsList); ++i) {
    const std::string key = std::string(kRegistryPrefix) + kStatsList[i];
    if (storage::Registry::Lookup(key, &stats_str)) {
      if (!stats.ParseFromString(stats_str)) {
        storage::Registry::Erase(key);
      }
      if (stats.type() == Stats::INTEGER || stats.type() == Stats::BOOLEAN) {
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

void UsageStats::ClearAllStats() {
  for (size_t i = 0; i < arraysize(kStatsList); ++i) {
    const std::string key = std::string(kRegistryPrefix) + kStatsList[i];
    storage::Registry::Erase(key);
  }
}

void UsageStats::IncrementCountBy(const std::string &name, uint32_t val) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  // Does nothing
}

void UsageStats::UpdateTiming(const std::string &name, uint32_t val) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  // Does nothing
}

void UsageStats::SetInteger(const std::string &name, int val) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  // Does nothing
}

void UsageStats::SetBoolean(const std::string &name, bool val) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  // Does nothing
}

bool UsageStats::GetCountForTest(const std::string &name, uint32_t *value) {
  CHECK(value != nullptr);
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

bool UsageStats::GetIntegerForTest(const std::string &name, int32_t *value) {
  CHECK(value != nullptr);
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

bool UsageStats::GetBooleanForTest(const std::string &name, bool *value) {
  CHECK(value != nullptr);
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

bool UsageStats::GetTimingForTest(const std::string &name, uint64_t *total_time,
                                  uint32_t *num_timings, uint32_t *avg_time,
                                  uint32_t *min_time, uint32_t *max_time) {
  Stats stats;
  if (!GetterInternal(name, Stats::TIMING, &stats)) {
    return false;
  }

  if ((total_time != nullptr && !stats.has_total_time()) ||
      (num_timings != nullptr && !stats.has_num_timings()) ||
      (avg_time != nullptr && !stats.has_avg_time()) ||
      (min_time != nullptr && !stats.has_min_time()) ||
      (max_time != nullptr && !stats.has_max_time())) {
    LOG(WARNING) << "cannot import stats of " << name << ".";
    return false;
  }

  if (total_time != nullptr) {
    *total_time = stats.total_time();
  }
  if (num_timings != nullptr) {
    *num_timings = stats.num_timings();
  }
  if (avg_time != nullptr) {
    *avg_time = stats.avg_time();
  }
  if (min_time != nullptr) {
    *min_time = stats.min_time();
  }
  if (max_time != nullptr) {
    *max_time = stats.max_time();
  }

  return true;
}

bool UsageStats::GetVirtualKeyboardForTest(const std::string &name,
                                           Stats *stats) {
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

bool UsageStats::GetStatsForTest(const std::string &name, Stats *stats) {
  return LoadStats(name, stats);
}

void UsageStats::StoreTouchEventStats(
    const std::string &name,
    const std::map<std::string, TouchEventStatsMap> &touch_stats) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  // Does nothing
}

bool UsageStats::Sync() {
  ClearAllStats();                      // Clears accumulated data.
  UsageStatsUploader::ClearMetaData();  // Clears meta data to send usage stats.
  if (!storage::Registry::Sync()) {
    LOG(ERROR) << "sync failed";
    return false;
  }
  return true;
}

}  // namespace usage_stats
}  // namespace mozc

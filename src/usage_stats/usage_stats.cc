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

#include "base/util.h"
#include "storage/registry.h"
#include "usage_stats/usage_stats.pb.h"

namespace mozc {
namespace usage_stats {

namespace {
const char kRegistryPrefix[] = "usage_stats.";

#include "usage_stats/usage_stats_list.h"

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
    const string key = string(kRegistryPrefix) + string(kStatsList[i]);
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

void UsageStats::IncrementCountBy(const string &name, uint32 val) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  string stats_str;
  Stats stats;
  const string key = kRegistryPrefix + name;
  if (!storage::Registry::Lookup(key, &stats_str)) {
    stats.set_name(name);
    stats.set_type(Stats::COUNT);
    stats.set_count(val);
  } else {
    if (!stats.ParseFromString(stats_str)) {
      storage::Registry::Erase(key);
      LOG(ERROR) << "ParseFromString failed";
      return;
    }
    DCHECK(stats.type() == Stats::COUNT);
    stats.set_count(stats.count() + val);
  }
  stats_str.clear();
  stats.AppendToString(&stats_str);
  if (!storage::Registry::Insert(key, stats_str)) {
    LOG(ERROR) << "cannot save " << name << " to registry";
  }
}

void UsageStats::UpdateTimingBy(const string &name,
                                const vector<uint32> &values) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  string stats_str;
  Stats stats;
  const string key = kRegistryPrefix + name;
  if (!storage::Registry::Lookup(key, &stats_str)) {
    const uint32 val_min = *min_element(values.begin(), values.end());
    const uint32 val_max = *max_element(values.begin(), values.end());
    uint64 val_total = 0;
    for (size_t i = 0; i < values.size(); ++i) {
      val_total += values[i];
    }
    const uint32 val_avg = val_total / values.size();

    stats.set_name(name);
    stats.set_type(Stats::TIMING);
    stats.set_num_timings(values.size());
    stats.set_avg_time(val_avg);
    stats.set_min_time(val_min);
    stats.set_max_time(val_max);
    stats.set_total_time(val_total);
  } else {
    if (!stats.ParseFromString(stats_str)) {
      storage::Registry::Erase(key);
      LOG(ERROR) << "ParseFromString failed";
      return;
    }
    DCHECK(stats.type() == Stats::TIMING);
    const uint32 num = stats.num_timings();
    uint64 val_total = stats.total_time();
    uint32 val_min = stats.min_time();
    uint32 val_max = stats.max_time();
    for (size_t i = 0; i < values.size(); ++i) {
      const uint32 val = values[i];
      val_total += val;
      if (val < val_min) {
        val_min = val;
      }
      if (val > val_max) {
        val_max = val;
      }
    }

    const uint32 val_num = num + values.size();
    stats.set_num_timings(val_num);
    stats.set_avg_time(val_total / val_num);
    stats.set_min_time(val_min);
    stats.set_max_time(val_max);
    stats.set_total_time(val_total);
  }
  stats_str.clear();
  stats.AppendToString(&stats_str);
  if (!storage::Registry::Insert(key, stats_str)) {
    LOG(ERROR) << "cannot save " << name << " to registry";
  }
}

void UsageStats::SetInteger(const string &name, int val) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  string stats_str;
  Stats stats;
  const string key = kRegistryPrefix + name;
  if (!storage::Registry::Lookup(key, &stats_str)) {
    stats.set_name(name);
    stats.set_type(Stats::INTEGER);
    stats.set_int_value(val);
  } else {
    if (!stats.ParseFromString(stats_str)) {
      storage::Registry::Erase(key);
      LOG(ERROR) << "ParseFromString failed";
      return;
    }
    DCHECK(stats.type() == Stats::INTEGER);
    stats.set_int_value(val);
  }
  stats_str.clear();
  stats.AppendToString(&stats_str);
  if (!storage::Registry::Insert(key, stats_str)) {
    LOG(ERROR) << "cannot save " << name << " to registry";
  }
}

void UsageStats::SetBoolean(const string &name, bool val) {
  DCHECK(IsListed(name)) << name << " is not in the list";
  string stats_str;
  Stats stats;
  const string key = kRegistryPrefix + name;
  if (!storage::Registry::Lookup(key, &stats_str)) {
    stats.set_name(name);
    stats.set_type(Stats::BOOLEAN);
    stats.set_boolean_value(val);
  } else {
    if (!stats.ParseFromString(stats_str)) {
      storage::Registry::Erase(key);
      LOG(ERROR) << "ParseFromString failed";
      return;
    }
    DCHECK(stats.type() == Stats::BOOLEAN);
    stats.set_boolean_value(val);
  }
  stats_str.clear();
  stats.AppendToString(&stats_str);
  if (!storage::Registry::Insert(key, stats_str)) {
    LOG(ERROR) << "cannot save " << name << " to registry";
  }
}


void UsageStats::Sync() {
  if (!storage::Registry::Sync()) {
    LOG(ERROR) << "sync failed";
  }
}

}  // namespace usage_stats
}  // namespace mozc

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

#include "base/encryptor.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "base/util.h"
#include "base/version.h"
#include "config/stats_config_util.h"
#include "storage/registry.h"
#include "usage_stats/usage_stats.pb.h"
#include "usage_stats/upload_util.h"

#ifdef OS_WINDOWS
#include "win32/base/imm_util.h"  // IsCuasEnabled
#endif

namespace mozc {
namespace usage_stats {

namespace {
const char kRegistryPrefix[] = "usage_stats.";
const char kLastUploadKey[] = "last_upload";
const char kClientIdKey[] = "client_id";
const uint32 kSendInterval = 23 * 60 * 60;  // 23 hours

#include "usage_stats/usage_stats_list.h"

// creates randomly generated new client id and insert it to registry
void CreateAndInsertClientId(string *output) {
  DCHECK(output);
  const size_t kClientIdSize = 16;
  char rand_str[kClientIdSize + 1];
  Util::GetSecureRandomAsciiSequence(rand_str, sizeof(rand_str));
  rand_str[kClientIdSize] = '\0';
  *output = rand_str;

  string encrypted;
  if (!Encryptor::ProtectData(*output, &encrypted)) {
    LOG(ERROR) << "cannot encrypt client_id";
    return;
  }
  const string key = string(kRegistryPrefix) + string(kClientIdKey);
  if (!storage::Registry::Insert(key, encrypted)) {
    LOG(ERROR) << "cannot insert client_id to registry";
    return;
  }
  return;
}

// default implementation for client id
// this refers the registry and returns stored id if the id is found.
class ClientIdImpl : public ClientIdInterface {
 public:
  ClientIdImpl();
  virtual ~ClientIdImpl();
  void GetClientId(string *output);
};

ClientIdImpl::ClientIdImpl() {}

ClientIdImpl::~ClientIdImpl() {}

void ClientIdImpl::GetClientId(string *output) {
  DCHECK(output);
  string encrypted;
  const string key = string(kRegistryPrefix) + string(kClientIdKey);
  if (!storage::Registry::Lookup(key, &encrypted)) {
    LOG(ERROR) << "clientid lookup failed";
    CreateAndInsertClientId(output);
    return;
  }
  if (!Encryptor::UnprotectData(encrypted, output)) {
    LOG(ERROR) << "unprotect clientid failed";
    CreateAndInsertClientId(output);
    return;
  }

  // lookup and unprotect succeeded
  return;
}

ClientIdInterface *g_client_id_handler = NULL;
Mutex g_mutex;

ClientIdInterface &GetClientIdHandler() {
  scoped_lock l(&g_mutex);
  if (g_client_id_handler == NULL) {
    return *(Singleton<ClientIdImpl>::get());
  } else {
    return *g_client_id_handler;
  }
}

}  // namespace

const uint32 UsageStats::kDefaultSchedulerDelay = 60*1000;  // 1 min
const uint32 UsageStats::kDefaultSchedulerRandomDelay = 5*60*1000;  // 5 min
const uint32 UsageStats::kDefaultScheduleInterval = 5*60*1000;  // 5 min
const uint32 UsageStats::kDefaultScheduleMaxInterval = 2*60*60*1000;  // 2 hours

UsageStats::UsageStats() {
}

UsageStats::~UsageStats() {
}

void UsageStats::SetClientIdHandler(ClientIdInterface *client_id_handler) {
  scoped_lock l(&g_mutex);
  g_client_id_handler = client_id_handler;
}

bool UsageStats::IsListed(const string &name) {
  for (size_t i = 0; i < arraysize(kStatsList); ++i) {
    if (name == kStatsList[i]) {
      return true;
    }
  }
  return false;
}

void UsageStats::LoadStats(UploadUtil *uploader) {
  DCHECK(uploader);
  string stats_str;
  Stats stats;
  for (size_t i = 0; i < arraysize(kStatsList); ++i) {
    const string key = string(kRegistryPrefix) + string(kStatsList[i]);
    if (!storage::Registry::Lookup(key, &stats_str)) {
      VLOG(4) << "stats not found: " << kStatsList[i];
      continue;
    }
    if (!stats.ParseFromString(stats_str)) {
      LOG(ERROR) << "ParseFromString failed.";
      continue;
    }
    const string &name = stats.name();
    switch (stats.type()) {
      case Stats::COUNT:
        DCHECK(stats.has_count()) << name;
        uploader->AddCountValue(name, stats.count());
        break;
      case Stats::TIMING:
        DCHECK(stats.has_num_timings()) << name;
        DCHECK(stats.has_avg_time()) << name;
        DCHECK(stats.has_min_time()) << name;
        DCHECK(stats.has_max_time()) << name;
        uploader->AddTimingValue(name, stats.num_timings(), stats.avg_time(),
                                 stats.min_time(), stats.max_time());
        break;
      case Stats::INTEGER:
        DCHECK(stats.has_int_value()) << name;
        uploader->AddIntegerValue(name, stats.int_value());
        break;
      case Stats::BOOLEAN:
        DCHECK(stats.has_boolean_value()) << name;
        uploader->AddBooleanValue(name, stats.boolean_value());
        break;
      default:
        VLOG(3) << "stats " << name << " has no type";
        break;
    }
  }
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

void UsageStats::GetClientId(string *output) {
  GetClientIdHandler().GetClientId(output);
}

bool UsageStats::Send(void *data) {
  Sync();
  const uint32 current_sec = static_cast<uint32>(Util::GetTime());
  uint32 last_upload_sec;
  const string upload_key = string(kRegistryPrefix) + string(kLastUploadKey);
  if (!storage::Registry::Lookup(upload_key, &last_upload_sec) ||
      last_upload_sec > current_sec) {
    // invalid value: time zone changed etc.

    // quit here just saving current time and clear stats
    ClearStats();
    if (!storage::Registry::Insert(upload_key, current_sec)) {
      LOG(ERROR) << "cannot save current_time to registry";
      return false;
    } else {
      VLOG(2) << "saved current_time to registry";
      return true;
    }
  }

  // if no upload_usage_stats, we simply clear stats here.
  if (!StatsConfigUtil::IsEnabled()) {
    ClearStats();
    LOG(WARNING) << "no send_stats";
    return false;
  }

  const uint32 elapsed_sec = current_sec - last_upload_sec;
  DCHECK_GE(elapsed_sec, 0);

  if (elapsed_sec < kSendInterval) {
    LOG(WARNING) << "not enough time has passed yet";
    return false;
  }

  vector<pair<string, string> > params;
  params.push_back(make_pair("hl", "ja"));
  params.push_back(make_pair("v", Version::GetMozcVersion()));
  string client_id;
  GetClientId(&client_id);
  DCHECK(!client_id.empty());
  params.push_back(make_pair("client_id", client_id));
  params.push_back(make_pair("os_ver", Util::GetOSVersionString()));
  // Get total memory in MB.
  const uint32 memory_in_mb = Util::GetTotalPhysicalMemory() / (1024 * 1024);
  UsageStats::SetInteger("TotalPhysicalMemory", memory_in_mb);

  UploadUtil uploader;
  uploader.SetHeader("Daily", elapsed_sec, params);

#ifdef OS_WINDOWS
  UsageStats::SetBoolean("WindowsX64", Util::IsWindowsX64());
  {
    // get msctf version
    int major, minor, build, revision;
    const wchar_t kDllName[] = L"msctf.dll";
    wstring path = Util::GetSystemDir();
    path += L"\\";
    path += kDllName;
    if (Util::GetFileVersion(path, &major, &minor, &build, &revision)) {
      UsageStats::SetInteger("MsctfVerMajor", major);
      UsageStats::SetInteger("MsctfVerMinor", minor);
      UsageStats::SetInteger("MsctfVerBuild", build);
      UsageStats::SetInteger("MsctfVerRevision", revision);
    } else {
      LOG(ERROR) << "get file version for msctf.dll failed";
    }
  }
  UsageStats::SetBoolean("CuasEnabled", win32::ImeUtil::IsCuasEnabled());
#endif

  LoadStats(&uploader);

  // Just check for confirming that we can insert the value to upload_key.
  if (!storage::Registry::Insert(upload_key, last_upload_sec)) {
    LOG(ERROR) << "cannot save to registry";
    return false;
  }
  if (!uploader.Upload()) {
    LOG(ERROR) << "usagestats upload failed";
    UsageStats::IncrementCount("UsageStatsUploadFailed");
    return false;
  }
  ClearStats();

  // Actual insersion to upload_key
  if (!storage::Registry::Insert(upload_key, current_sec)) {
    // This case is the worst case, but we will not send the data
    // at the next trial, because next checking for insertion to upload_key
    // should be failed.
    LOG(ERROR) << "cannot save current_time to registry";
    return false;
  }
  Sync();
  VLOG(2) << "send success";
  return true;
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

}  // namespace mozc::usage_stats
}  // namespace mozc

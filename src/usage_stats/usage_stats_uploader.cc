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

#include "usage_stats/usage_stats_uploader.h"

#include <utility>
#include <vector>

#ifdef OS_ANDROID
#include "base/android_util.h"
#endif  // OS_ANDROID
#include "base/encryptor.h"
#include "base/mutex.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/version.h"
#include "config/stats_config_util.h"
#include "storage/registry.h"
#include "usage_stats/upload_util.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats.pb.h"
#include "usage_stats/usage_stats_updater.h"

namespace mozc {
namespace usage_stats {

namespace {
const char kRegistryPrefix[] = "usage_stats.";
const char kLastUploadKey[] = "last_upload";
const char kMozcVersionKey[] = "mozc_version";
const char kClientIdKey[] = "client_id";
const uint32 kSendInterval = 23 * 60 * 60;  // 23 hours

#include "usage_stats/usage_stats_list.h"

// creates randomly generated new client id and insert it to registry
void CreateAndInsertClientId(string *output) {
  DCHECK(output);
  const size_t kClientIdSize = 16;
  char rand_str[kClientIdSize + 1];
  Util::GetRandomAsciiSequence(rand_str, sizeof(rand_str));
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
Mutex g_mutex;  // NOLINT

ClientIdInterface &GetClientIdHandler() {
  scoped_lock l(&g_mutex);
  if (g_client_id_handler == NULL) {
    return *(Singleton<ClientIdImpl>::get());
  } else {
    return *g_client_id_handler;
  }
}

void AddDoubleValueStatsToUploadUtil(
    const string &key_name_base,
    const Stats::DoubleValueStats &double_stats,
    double average_scale, double variance_scale,
    UploadUtil *uploader) {
  if (double_stats.num() == 0) {
    return;
  }
  double average = double_stats.total() / double_stats.num();
  double variance =
      double_stats.square_total() / double_stats.num() - average * average;

  uploader->AddIntegerValue(key_name_base + "a",
                            static_cast<int>(average * average_scale));
  uploader->AddIntegerValue(key_name_base + "v",
                            static_cast<int>(variance * variance_scale));
}

void AddVirtualKeyboardStatsToUploadUtil(const Stats &stats,
                                         UploadUtil *uploader) {
  DCHECK(stats.type() == Stats::VIRTUAL_KEYBOARD);

  string stats_name = stats.name();
  // Change stats name to reduce network traffic
  if (stats_name == "VirtualKeyboardStats") {
    stats_name = "vks";
  } else if (stats_name == "VirtualKeyboardMissStats") {
    stats_name = "vkms";
  } else {
    LOG(ERROR) << "Unexpected stats_name: " << stats_name;
    return;
  }

  for (size_t i = 0; i < stats.virtual_keyboard_stats_size(); ++i) {
    const Stats::VirtualKeyboardStats &virtual_keyboard_stats =
        stats.virtual_keyboard_stats(i);

    // Set the keyboard_id
    // example:
    //  vks_name_TWELVE_KEY_TOGGLE_FLICK_KANA : 0
    //  vks_name_TWELVE_KEY_TOGGLE_KANA : 1
    //  vks_name_TWELVE_KEY_TOGGLE_NUMBER : 2
    //  vkms_name_TWELVE_KEY_TOGGLE_FLICK_KANA : 0
    //  vkms_name_TWELVE_KEY_TOGGLE_NUMBER : 1
    uploader->AddIntegerValue(stats_name + "_name_" +
                              virtual_keyboard_stats.keyboard_name(),
                              i);
    // Set the average and the variance of each stat.
    // example:
    //  vks_1_3_sxa (VirtualKeyboardStats_StartXAverage_keyboard1_sourceid3)
    //  ^^^ | | ||| : vks(VirtualKeyboardStats), vkms(VirtualKeyboardMissStats)
    //      ^ | ||| : keyboad_id
    //        ^ ||| : source_id
    //          ^^| : sx(StartX), sx(StartY), dx(DirectionX), dy(DirectionY),
    //            |   tl(TimeLength)
    //            ^ : a(Average), v(Variance)
    for (size_t j = 0; j < virtual_keyboard_stats.touch_event_stats_size();
         ++j) {
      // Calculate average and variance
      //   Average = total / num
      //   Variance = square_total / num - (total / num) ^ 2
      // Because the current log analysis system can only deal with int values,
      // we multiply these values by a scale factor and send them to server.
      //   sxa, sya, dxa, dya : scale = 10000000
      //   sxv, syv, dxv, dyv : scale = 10000000
      //   tla, tlv : scale = 10000000
      const Stats::TouchEventStats &touch =
          virtual_keyboard_stats.touch_event_stats(j);
      const string key_name_base = Util::StringPrintf(
          "%s_%d_%d_", stats_name.c_str(), static_cast<int>(i),
          touch.source_id());

      AddDoubleValueStatsToUploadUtil(key_name_base + "sx",
                                      touch.start_x_stats(),
                                      10000000.0, 10000000.0,
                                      uploader);
      AddDoubleValueStatsToUploadUtil(key_name_base + "sy",
                                      touch.start_y_stats(),
                                      10000000.0, 10000000.0,
                                      uploader);
      AddDoubleValueStatsToUploadUtil(key_name_base + "dx",
                                      touch.direction_x_stats(),
                                      10000000.0, 10000000.0,
                                      uploader);
      AddDoubleValueStatsToUploadUtil(key_name_base + "dy",
                                      touch.direction_y_stats(),
                                      10000000.0, 10000000.0,
                                      uploader);
      AddDoubleValueStatsToUploadUtil(key_name_base + "tl",
                                      touch.time_length_stats(),
                                      10000000.0, 10000000.0,
                                      uploader);
    }
  }
}

}  // namespace

ClientIdInterface::ClientIdInterface() {}

ClientIdInterface::~ClientIdInterface() {}

// 1 min
const uint32 UsageStatsUploader::kDefaultSchedulerDelay = 60*1000;
// 5 min
const uint32 UsageStatsUploader::kDefaultSchedulerRandomDelay = 5*60*1000;
// 5 min
const uint32 UsageStatsUploader::kDefaultScheduleInterval = 5*60*1000;
// 2 hours
const uint32 UsageStatsUploader::kDefaultScheduleMaxInterval = 2*60*60*1000;

void UsageStatsUploader::SetClientIdHandler(
    ClientIdInterface *client_id_handler) {
  scoped_lock l(&g_mutex);
  g_client_id_handler = client_id_handler;
}

void UsageStatsUploader::LoadStats(UploadUtil *uploader) {
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
      case Stats::VIRTUAL_KEYBOARD:
        AddVirtualKeyboardStatsToUploadUtil(stats, uploader);
        break;
      default:
        VLOG(3) << "stats " << name << " has no type";
        break;
    }
  }
}

void UsageStatsUploader::GetClientId(string *output) {
  GetClientIdHandler().GetClientId(output);
}

bool UsageStatsUploader::Send(void *data) {
  const string upload_key = string(kRegistryPrefix) + kLastUploadKey;
  const uint32 current_sec = static_cast<uint32>(Util::GetTime());
  uint32 last_upload_sec = 0;
  const string mozc_version_key = string(kRegistryPrefix) + kMozcVersionKey;
  const string &current_mozc_version = Version::GetMozcVersion();
  string last_mozc_version;
  if (!storage::Registry::Lookup(upload_key, &last_upload_sec) ||
      last_upload_sec > current_sec ||
      !storage::Registry::Lookup(mozc_version_key, &last_mozc_version) ||
      last_mozc_version != current_mozc_version) {
    // quit here just saving current time and clear stats
    UsageStats::ClearStats();
    bool result = true;
    result &= storage::Registry::Insert(upload_key, current_sec);
    result &= storage::Registry::Insert(mozc_version_key, current_mozc_version);

    LOG_IF(ERROR, !result) << "cannot save usage stats metadata to registry";
    return result;
  }

  // if usage stats is disabled, we simply clear stats here.
  if (!mozc::config::StatsConfigUtil::IsEnabled()) {
    UsageStats::ClearStats();
    VLOG(1) << "UsageStats is disabled.";
    return false;
  }

  uint32 elapsed_sec = 0;
  if (current_sec >= last_upload_sec) {
    elapsed_sec = current_sec - last_upload_sec;
  } else {
    LOG(WARNING) << "Timing counter seems to be reversed."
                 << " current_sec: " << current_sec
                 << " last_upload_sec: " << last_upload_sec;
  }

  if (elapsed_sec < kSendInterval) {
    VLOG(1) << "Skip uploading the data as not enough time has passed yet."
            << " current_sec: " << current_sec
            << " last_upload_sec: " << last_upload_sec
            << " elapsed_sec: " << elapsed_sec
            << " kSendInterval: " << kSendInterval;
    return false;
  }

  vector<pair<string, string> > params;
  params.push_back(make_pair("hl", "ja"));
  params.push_back(make_pair("v", Version::GetMozcVersion()));
  string client_id;
  GetClientId(&client_id);
  DCHECK(!client_id.empty());
  params.push_back(make_pair("client_id", client_id));
  params.push_back(make_pair("os_ver", SystemUtil::GetOSVersionString()));
#ifdef OS_ANDROID
  params.push_back(
      make_pair("model",
                AndroidUtil::GetSystemProperty(
                    AndroidUtil::kSystemPropertyModel, "Unknown")));
#endif  // OS_ANDROID

  UsageStatsUpdater::UpdateStats();

  UploadUtil uploader;
  uploader.SetHeader("Daily", elapsed_sec, params);
#ifdef __native_client__
  // In NaCl Mozc we use HTTPS to send usage stats to follow Chrome OS
  // convention. https://code.google.com/p/chromium/issues/detail?id=255327
  uploader.SetUseHttps(true);
#endif  // __native_client__
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
  UsageStats::ClearStats();

  // Actual insersion to upload_key
  if (!storage::Registry::Insert(upload_key, current_sec)) {
    // This case is the worst case, but we will not send the data
    // at the next trial, because next checking for insertion to upload_key
    // should be failed.
    LOG(ERROR) << "cannot save current_time to registry";
    return false;
  }
  if (!UsageStats::Sync()) {
    LOG(ERROR) << "Failed to sync cleared usage stats to disk storage.";
    return false;
  }

  VLOG(2) << "send success";
  return true;
}

}  // namespace usage_stats
}  // namespace mozc

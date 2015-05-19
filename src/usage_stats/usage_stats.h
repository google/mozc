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

#ifndef MOZC_USAGE_STATS_USAGE_STATS_H_
#define MOZC_USAGE_STATS_USAGE_STATS_H_

#include <map>
#include <string>
#include <vector>
#include "base/port.h"
// FRIEND_TEST
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace usage_stats {
class UploadUtil;


class ClientIdInterface {
 public:
  ClientIdInterface() {}
  virtual ~ClientIdInterface() {}

  // gets client id
  virtual void GetClientId(string *output) = 0;
};

class UsageStats {
 public:
  // Default values for scheduler.
  static const uint32 kDefaultSchedulerDelay;
  static const uint32 kDefaultSchedulerRandomDelay;
  static const uint32 kDefaultScheduleInterval;
  static const uint32 kDefaultScheduleMaxInterval;

  UsageStats();
  virtual ~UsageStats();

  // Scheduler callback
  // Sends daily usage stats
  // Is called periodically (is not necessarily called 'daily')
  // parameter 'data' is not used here
  // (for interface compatibility with scheduler)
  //
  // NOTE:
  //  This function is called from another thread.
  //  So this should be thread-safe.
  static bool Send(void *data);

  // Updates count value
  // Increments val to current value
  static void IncrementCountBy(const string &name, uint32 val);
  static void IncrementCount(const string &name) {
    IncrementCountBy(name, 1);
  }

  // Updates timing value
  // Updates current value using given val
  // input 'values' vector consists of each timing values.
  //  for example, [3, 4, 2]
  static void UpdateTimingBy(const string &name, const vector<uint32> &values);
  static void UpdateTiming(const string &name, uint32 val) {
    UpdateTimingBy(name, vector<uint32>(1, val));
  }

  // Sets integer value
  // Replaces old value with val
  static void SetInteger(const string &name, int val);

  // Sets boolean value
  // Replaces old value with val
  static void SetBoolean(const string &name, bool val);

  // Sync usage data into disk
  static void Sync();

  // Sets client id handler
  static void SetClientIdHandler(ClientIdInterface *client_id_handler);

  // Returns true if given stats name is in the stats list
  // (for debugging)
  static bool IsListed(const string &name);


 private:
  FRIEND_TEST(ClientIdTest, CreateClientIdTest);
  FRIEND_TEST(ClientIdTest, GetClientIdTest);
  FRIEND_TEST(ClientIdTest, GetClientIdFailTest);

  static void LoadStats(UploadUtil *uploader);
  static void ClearStats();

  // Gets client id
  static void GetClientId(string *output);

  DISALLOW_COPY_AND_ASSIGN(UsageStats);
};
}  // namespace mozc::usage_stats
}  // namespace mozc
#endif  // GOOCLECLIENT_IME_MOZC_USAGE_STATS_USAGE_STATS_H_

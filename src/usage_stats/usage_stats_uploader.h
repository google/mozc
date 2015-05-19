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

#ifndef MOZC_USAGE_STATS_USAGE_STATS_UPLOADER_H_
#define MOZC_USAGE_STATS_USAGE_STATS_UPLOADER_H_

#include <map>
#include <string>
#include <vector>
#include "base/port.h"

namespace mozc {
namespace usage_stats {
class UploadUtil;

class ClientIdInterface {
 public:
  ClientIdInterface();
  virtual ~ClientIdInterface();

  // gets client id
  virtual void GetClientId(string *output) = 0;
};

class UsageStatsUploader {
 public:
  // Default values for scheduler.
  static const uint32 kDefaultSchedulerDelay;
  static const uint32 kDefaultSchedulerRandomDelay;
  static const uint32 kDefaultScheduleInterval;
  static const uint32 kDefaultScheduleMaxInterval;

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

  // Sets client id handler
  static void SetClientIdHandler(ClientIdInterface *client_id_handler);

 protected:
  static void LoadStats(UploadUtil *uploader);

  // Gets client id
  static void GetClientId(string *output);

  DISALLOW_IMPLICIT_CONSTRUCTORS(UsageStatsUploader);
};

}  // namespace usage_stats
}  // namespace mozc

#endif  // MOZC_USAGE_STATS_USAGE_STATS_UPLOADER_H_

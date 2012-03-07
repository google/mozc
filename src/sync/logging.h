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

#ifndef MOZC_BASE_SYNC_LOGGER_H_
#define MOZC_BASE_SYNC_LOGGER_H_

#include <iostream>
#include <string>
#include "base/base.h"
#include "base/util.h"

namespace mozc {
namespace sync {

// We provide mozc::sync::Logging module separately from the
// mozc::Logging module.
// Syncer is doing lots of complicated operations and users might
// encounter unexceptional behaviors depending on the server status,
// storage status/size, and config settings. We will receive many error
// reports in our U2U. To make a prompt reply and quick investigation of
// the cause of errors happening on the user environments, logging is
// "must" feature.
//
// sync::Logging provides a logging interface for syncer module.
// Useally, the following macro can be used for logging any actions
// of syncer modules
//
// SYNC_VLOG(1) << "Sync started";
// SYNC_VLOG(2) << "Downloaded: " << remote_proto->DebugString();
//
// The log file is generated at <user_profile_dir>/sync.log
// The verbose level is saved in FLAGS_sync_verbose_level now.
// TODO(taku): better to move this parameter to config.

class Logging {
 public:
  // Get logging stream. The log message can be written to the stream
  static ostream &GetLogStream();

  // return config.sync_verbose_level()
  static int GetVerboseLevel();

  // return the filename of sync logging.
  static string GetLogFileName();

  // clear the contents of logging file and recreate it.
  // This method is used in unittesting.
  static void Reset();
};

class LogFinalizer {
 public:
  LogFinalizer() {}
  ~LogFinalizer();

  void operator&(ostream&) {}
};
}  // sync
}  // mozc

#define SYNC_VLOG(verboselevel) \
 (mozc::sync::Logging::GetVerboseLevel() < (verboselevel)) ? (void) 0 : \
 mozc::sync::LogFinalizer() & mozc::sync::Logging::GetLogStream() \
 << mozc::Logging::GetLogMessageHeader() << " " \
 << mozc::Util::Basename(__FILE__) << "(" << __LINE__ << ") "

#endif  // MOZC_BASE_SYNC_LOGGER_H_

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

#include "win32/broker/ime_switcher.h"

#include <windows.h>

#include <sstream>

#include "base/logging.h"
#include "base/system_util.h"
#include "client/client.h"
#include "config/config_handler.h"
#include "protocol/config.pb.h"
#include "win32/base/imm_util.h"
#include "absl/flags/flag.h"

ABSL_FLAG(bool, set_default_do_not_ask_again, false,
          "Set true if SetDefaultDialog should not be displayed again.");

namespace mozc {
namespace win32 {
namespace {

constexpr char kProcessMutexPrefixForPerUserIMESettings[] =
    "mozc_hkcu_manipulation_for_ime.";

// It seems that mozc_tool basically uses the following error levels.
constexpr int kErrorLevelProcessMutexInUse = -1;
constexpr int kErrorLevelSuccess = 0;
constexpr int kErrorLevelGeneralError = 1;

void NotifyFatalMessageImpl(const std::string &msg) {
#ifdef MOZC_NO_LOGGING
  // Explicitly causes crash so that the migration failure will be notified
  // through crash dump.
  LOG(FATAL) << msg;
#else   // MOZC_NO_LOGGING
  ::MessageBoxA(nullptr, msg.c_str(), "GoogleIMEJaBroker",
                MB_OK | MB_ICONERROR);
#endif  // MOZC_NO_LOGGING
}

void NotifyFatalMessage(const std::string &msg, int line) {
  std::ostringstream ostr;
  ostr << msg << " (line: " << line << ")";
  NotifyFatalMessageImpl(ostr.str());
}

std::string GetMutexName() {
  return (kProcessMutexPrefixForPerUserIMESettings +
          SystemUtil::GetDesktopNameAsString());
}

bool ClearCheckDefault() {
  client::Client client;
  if (!client.PingServer() && !client.EnsureConnection()) {
    LOG(ERROR) << "Cannot connect to server";
    return false;
  }
  config::Config config;
  config::ConfigHandler::GetConfig(&config);
  config.set_check_default(false);
  client.SetConfig(config);
  return true;
}

}  // namespace

int RunSetDefault(int argc, char *argv[]) {
  if (!ImeUtil::SetDefault()) {
    NotifyFatalMessage("SetDefault() failed.", __LINE__);
    return kErrorLevelGeneralError;
  }

  if (absl::GetFlag(FLAGS_set_default_do_not_ask_again)) {
    if (!ClearCheckDefault()) {
      // Notify the error to user but never treat this as an error.
      NotifyFatalMessage("ClearCheckDefault() failed.", __LINE__);
    }
  }
  return kErrorLevelSuccess;
}

}  // namespace win32
}  // namespace mozc

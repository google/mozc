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

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <InputMethodKit/IMKServer.h>

#import "mac/GoogleJapaneseInputController.h"
#import "mac/GoogleJapaneseInputServer.h"

#include "base/const.h"
#include "base/crash_report_handler.h"
#include "base/logging.h"
#include "base/run_level.h"
#include "client/client.h"
#include "config/stats_config_util.h"

int main(int argc, char *argv[]) {
  if (!mozc::RunLevel::IsValidClientRunLevel()) {
    return -1;
  }

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  if (mozc::config::StatsConfigUtil::IsEnabled()) {
    mozc::CrashReportHandler::Initialize(false);
  }
#endif
  InitGoogle(argv[0], &argc, &argv, false);

  IMKServer *imkServer = [GoogleJapaneseInputServer getServer];
  if (!imkServer) {
    LOG(FATAL) << mozc::kProductNameInEnglish << " failed to initialize";
    return -1;
  }
  DLOG(INFO) << mozc::kProductNameInEnglish << " initialized";

  [GoogleJapaneseInputController initializeConstants];

  // Start the converter server at this time explicitly to prevent the
  // slow-down of the response for initial key event.
  {
    scoped_ptr<mozc::client::Client> client(new mozc::client::Client);
    client->PingServer();
  }
  NSApplicationMain(argc, (const char **)argv);
  [pool drain];
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  mozc::CrashReportHandler::Uninitialize();
#endif
  return 0;
}

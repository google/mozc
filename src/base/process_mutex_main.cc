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

#include "base/process_mutex.h"

#ifdef OS_WIN
#include <windows.h>
#endif  // OS_WIN

#include <cstdint>
#include <string>

#include "base/init_mozc.h"
#include "base/logging.h"
#include "absl/flags/flag.h"

ABSL_FLAG(int32_t, sleep_time, 30, "sleep 30 sec");
ABSL_FLAG(std::string, name, "named_event_test", "name for named event");

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  mozc::ProcessMutex mutex(absl::GetFlag(FLAGS_name).c_str());

  if (!mutex.Lock()) {
    LOG(INFO) << "Process " << absl::GetFlag(FLAGS_name)
              << " is already running";
    return -1;
  }

#ifdef OS_WIN
  ::Sleep(absl::GetFlag(FLAGS_sleep_time) * 1000);
#else   // OS_WIN
  ::sleep(absl::GetFlag(FLAGS_sleep_time));
#endif  // OS_WIN

  mutex.UnLock();

  return 0;
}

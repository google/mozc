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

#include <string>

#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/port.h"
#include "ipc/named_event.h"
#include "absl/flags/flag.h"

ABSL_FLAG(bool, listener, true, "listener mode");
ABSL_FLAG(bool, notifier, false, "notifier mode");
ABSL_FLAG(int32, timeout, -1, "timeout (msec)");
ABSL_FLAG(int32, pid, -1, "process id");
ABSL_FLAG(std::string, name, "named_event_test", "name for named event");

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if (absl::GetFlag(FLAGS_notifier)) {
    mozc::NamedEventNotifier notifier(absl::GetFlag(FLAGS_name).c_str());
    CHECK(notifier.IsAvailable()) << "NamedEventNotifier is not available";

    notifier.Notify();
    LOG(INFO) << "Notification has been sent";

  } else if (absl::GetFlag(FLAGS_listener)) {
    mozc::NamedEventListener listener(absl::GetFlag(FLAGS_name).c_str());
    CHECK(listener.IsAvailable()) << "NamedEventListener is not available";

    LOG_IF(INFO, listener.IsOwner()) << "This instance owns event handle";

    LOG(INFO) << "Waiting event " << absl::GetFlag(FLAGS_name);
    if (absl::GetFlag(FLAGS_pid) != -1) {
      switch (listener.WaitEventOrProcess(
          absl::GetFlag(FLAGS_timeout),
          static_cast<size_t>(absl::GetFlag(FLAGS_pid)))) {
        case mozc::NamedEventListener::TIMEOUT:
          LOG(INFO) << "timeout";
          break;
        case mozc::NamedEventListener::EVENT_SIGNALED:
          LOG(INFO) << "event singaled";
          break;
        case mozc::NamedEventListener::PROCESS_SIGNALED:
          LOG(INFO) << "process signaled";
          break;
        default:
          LOG(INFO) << "unknown result";
          break;
      }
    } else {
      if (listener.Wait(absl::GetFlag(FLAGS_timeout))) {
        LOG(INFO) << "Event comes";
      }
    }

  } else {
    LOG(FATAL) << "please specify --listener or --notifier";
  }

  return 0;
}

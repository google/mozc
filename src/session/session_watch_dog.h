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

// Session WatchDog timer

#ifndef MOZC_SESSION_SESSION_WATCH_DOG_H_
#define MOZC_SESSION_SESSION_WATCH_DOG_H_

#include <memory>

#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "base/cpu_stats.h"
#include "base/thread.h"
#include "client/client_interface.h"

namespace mozc {

// SessionWatchDog class sends Cleanup command to Sessionhandler
// for every some specified seconds.
class SessionWatchDog {
 public:
  // Constructs and starts the watch dog by spawning a background thread.
  explicit SessionWatchDog(absl::Duration interval);

  // DI constructor for testing. Use `Create()` in production code.
  SessionWatchDog(absl::Duration interval,
                  std::unique_ptr<client::ClientInterface> client,
                  std::unique_ptr<CPUStatsInterface> cpu_stats);

  SessionWatchDog(const SessionWatchDog&) = delete;
  SessionWatchDog& operator=(const SessionWatchDog&) = delete;

  // Stops the watchdog and joins the background thread.
  ~SessionWatchDog();

  // Returns the interval of this watch dog.
  absl::Duration interval() const { return interval_; }

  // Returns true if watch dog can send CleanupCommand:
  // |cpu_loads|: An array of cpu loads.
  // |cpu_load_index|: the size of cpu loads
  // |last_cleanup_time|: the last UTC time cleanup is executed
  // TODO(taku): want to define it inside private with FRIEND_TEST
  bool CanSendCleanupCommand(absl::Span<const float> cpu_loads,
                             absl::Time current_cleanup_time,
                             absl::Time last_cleanup_time) const;

 private:
  void ThreadMain();

  absl::Duration interval_;
  std::unique_ptr<client::ClientInterface> client_;
  std::unique_ptr<CPUStatsInterface> cpu_stats_;
  absl::Notification stop_;
  Thread thread_;
};

}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_WATCH_DOG_H_

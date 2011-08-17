// Copyright 2010-2011, Google Inc.
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

#include "base/base.h"
#include "base/scoped_handle.h"
#include "base/thread.h"

namespace mozc {
class CPUStatsInterface;
class UnnamedEvent;
namespace client {
class ClientInterface;
}

// SessionWatchDog class sends Cleanup command to Sessionhandler
// for every some specified seconds.
class SessionWatchDog : public Thread {
 public:
  // return the interval sec of watch dog timer
  int32 interval() const {
    return interval_sec_;
  }

  // Set client interface. This method doesn't take the owership.
  // mainly for unittesting.
  void SetClientInterface(client::ClientInterface *client);

  // Set CPUStats interface. This method doesn't take the owership.
  // mainly for unittesting.
  void SetCPUStatsInterface(CPUStatsInterface *cpu_stats);

  explicit SessionWatchDog(int32 interval_sec);
  virtual ~SessionWatchDog();

  // inherited from Thread class
  void Terminate();

  // return false if the watch dog thread receives stop signal
  bool Wait(int32 timeout);

  // inherited from Thread class
  // start watch dog timer and return immediately
  // virtual void Start();


  // return true if watch dog can send CleanupCommand:
  // |cpu_loads|: An array of cpu loads.
  // |cpu_load_index|: the size of cpu loads
  // |last_cleanup_time|: the last UTC time cleanup is executed
  // TODO(taku): want to define it inside private with FRIEND_TEST
  bool CanSendCleanupCommand(const volatile float *cpu_loads,
                             int cpu_load_index,
                             uint64 current_time,
                             uint64 last_cleanup_time) const;

 private:
  virtual void Run();

  int32 interval_sec_;
  client::ClientInterface *client_;
  CPUStatsInterface *cpu_stats_;
  scoped_ptr<UnnamedEvent> event_;
};
}  // namespace mozc
#endif  // MOZC_SESSION_SESSION_WATCH_DOG_H_

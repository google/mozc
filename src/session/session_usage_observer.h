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

#ifndef MOZC_SESSION_SESSION_USAGE_OBSERVER_H_
#define MOZC_SESSION_SESSION_USAGE_OBSERVER_H_

#include <map>
#include <vector>
#include "base/base.h"
#include "session/session_observer_interface.h"
#include "session/state.pb.h"
#include "session/commands.pb.h"
#include "usage_stats/usage_stats.pb.h"

namespace mozc {
namespace commands {
class Command;
}  // namespace mozc::commands

namespace session {

class SessionUsageObserver : public SessionObserverInterface {
 public:
  SessionUsageObserver();
  virtual ~SessionUsageObserver();

  void EvalCommandHandler(const commands::Command &command);
  void Reload();
  // Sets interval times for cache flash
  void SetInterval(uint32 val);

 private:
  // Save cached stats.
  void SaveStats();
  void EvalCreateSession(const commands::Input &input,
                        const commands::Output &output,
                        map<uint64, SessionState> *states);
  void EvalSendKey(const commands::Input &input,
                   const commands::Output &output);
  // Update state and update stats using input and output.
  void UpdateState(const commands::Input &input,
                   const commands::Output &output,
                   SessionState *state);
  // Update selected indices of |state|.
  void UpdateSelectedIndices(const commands::Input &input,
                             const commands::Output &output,
                             SessionState *state) const;
  // Update mode of |state|.
  void UpdateMode(const commands::Input &input, const commands::Output &output,
                  SessionState *state) const;
  // Check output and update stats.
  void CheckOutput(const commands::Input &input,
                   const commands::Output &output,
                   const SessionState *state);
  // Utility function for updating stats of candidates.
  void UpdateCandidateStats(const string &base_name, uint32 index);
  // Update client side stats.
  void UpdateClientSideStats(const commands::Input &input,
                             SessionState *state);

  // Update stats. Values are cached.
  void IncrementCount(const string &name);
  void IncrementCountBy(const string &name, uint64 count);
  void UpdateTiming(const string &name, uint64 val);
  void SetInteger(const string &name, int val);
  void SetBoolean(const string &name, bool val);

  uint32 update_count_;
  uint32 save_interval_;
  map<uint64, SessionState> states_;
  map<string, uint32> count_cache_;
  map<string, vector<uint32> > timing_cache_;
  map<string, int> integer_cache_;
  map<string, bool> boolean_cache_;


  DISALLOW_COPY_AND_ASSIGN(SessionUsageObserver);
};
}  // namespace mozc::session
}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_USAGE_OBSERVER_H_

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

#ifndef MOZC_SESSION_SESSION_USAGE_OBSERVER_H_
#define MOZC_SESSION_SESSION_USAGE_OBSERVER_H_

#include <map>
#include <string>
#include <vector>

#include "base/port.h"
#include "protocol/state.pb.h"
#include "session/session_observer_interface.h"
#include "usage_stats/usage_stats.h"

namespace mozc {
namespace commands {
class Command;
class Input;
// We cannot use Input::TouchEvent because C++ does not allow forward
// declaration of a nested-type.
class Input_TouchEvent;
class Output;
}  // namespace commands

namespace session {
class SessionUsageObserver : public SessionObserverInterface {
 public:
  SessionUsageObserver();
  ~SessionUsageObserver() override;

  void EvalCommandHandler(const commands::Command &command) override;

 private:
  struct UsageCache {
    // The structure of touch_event_stat_cache_ and miss_touch_event_stat_cache_
    // is as following:
    //   (keyboard_name_01 : (source_id_1 : TouchEventStats,
    //                        source_id_2 : TouchEventStats,
    //                        source_id_3 : TouchEventStats),
    //    keyboard_name_02 : (source_id_1 : TouchEventStats,
    //                        source_id_2 : TouchEventStats,
    //                        source_id_3 : TouchEventStats))
    // Memory usage estimation: TouchEventStat message uses 240 bytes and
    // the number of source_id can be a few hundreds, so these variables seem to
    // use less than 100 KBytes.
    std::map<std::string, usage_stats::TouchEventStatsMap> touch_event;
    std::map<std::string, usage_stats::TouchEventStatsMap> miss_touch_event;
    void Clear();
  };

  // Save cached stats.
  // This should be thread safe, as this will be called from scheduler.
  // Function type should be |bool Func(void *)| for using scheduler.
  static bool SaveCachedStats(void *data);

  void EvalCreateSession(const commands::Input &input,
                         const commands::Output &output,
                         std::map<uint64, protocol::SessionState> *states);
  // Update state and update stats using input and output.
  void UpdateState(const commands::Input &input, const commands::Output &output,
                   protocol::SessionState *state);
  // Update client side stats.
  void UpdateClientSideStats(const commands::Input &input,
                             protocol::SessionState *state);
  // Evals touch events and saves touch event stats.
  void LogTouchEvent(const commands::Input &input,
                     const commands::Output &output,
                     const protocol::SessionState &state);
  // Stores KeyTouch message to TouchEventStats.
  void StoreTouchEventStats(
      const commands::Input_TouchEvent &touch_event,
      usage_stats::TouchEventStatsMap *touch_event_stats_map);

  std::map<uint64, protocol::SessionState> states_;
  UsageCache usage_cache_;

  // last_touchevents_ is used to keep the touch_events of last SEND_KEY
  // message.
  // When the subsequent command will be received, if the command is BACKSPACE
  // last_touchevents_ will be aggregated to miss_touch_event_stat_cache_,
  // if the command is not BACKSPACE last_touchevents_ will be aggregated to
  // touch_event_stat_cache_.
  // A vector is used for storing multi touch event.
  // Because it will not be so large, reallocation will rarely happen.
  std::vector<commands::Input_TouchEvent> last_touchevents_;

  DISALLOW_COPY_AND_ASSIGN(SessionUsageObserver);
};
}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_USAGE_OBSERVER_H_

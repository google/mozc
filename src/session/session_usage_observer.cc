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

#include "session/session_usage_observer.h"

#include <climits>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/clock.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/port.h"
#include "config/stats_config_util.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/state.pb.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats.pb.h"
#include "absl/synchronization/mutex.h"

namespace mozc {
namespace session {
namespace {

using ::mozc::protocol::SessionState;
using ::mozc::usage_stats::UsageStats;

ABSL_CONST_INIT absl::Mutex g_stats_cache_mutex(absl::kConstInit);

constexpr size_t kMaxSession = 64;

// Adds double value to DoubleValueStats.
// DoubleValueStats contains (num, total, square_total).
void AddToDoubleValueStats(double value,
                           usage_stats::Stats::DoubleValueStats *double_stats) {
  DCHECK(double_stats);
  double_stats->set_num(double_stats->num() + 1);
  double_stats->set_total(double_stats->total() + value);
  double_stats->set_square_total(double_stats->square_total() + value * value);
}

uint64_t GetTimeInMilliSecond() {
  uint64_t second = 0;
  uint32_t micro_second = 0;
  Clock::GetTimeOfDay(&second, &micro_second);
  return second * 1000 + micro_second / 1000;
}

uint32_t GetDuration(uint64_t base_value) {
  const uint64_t result = GetTimeInMilliSecond() - base_value;
  if (result != static_cast<uint32_t>(result)) {
    return std::numeric_limits<uint32_t>::max();
  }
  return result;
}

bool IsSessionIndependentCommand(commands::Input::CommandType type) {
  switch (type) {
    // LINT.IfChange(session_independent_command)
    case commands::Input::NO_OPERATION:
    case commands::Input::SET_CONFIG:
    case commands::Input::GET_CONFIG:
    case commands::Input::SET_IMPOSED_CONFIG:
    case commands::Input::CLEAR_USER_HISTORY:
    case commands::Input::CLEAR_USER_PREDICTION:
    case commands::Input::CLEAR_UNUSED_USER_PREDICTION:
    case commands::Input::RELOAD:
    case commands::Input::SEND_USER_DICTIONARY_COMMAND:
    case commands::Input::SYNC_DATA:
    case commands::Input::CHECK_SPELLING:
    case commands::Input::SET_REQUEST:
    // LINT.ThenChange()
      return true;
    default:
      return false;
  }
}
}  // namespace

SessionUsageObserver::SessionUsageObserver() {
  // Explicitly calls Sync() to delete usage stats existing data including
  // metadata for migration.
  UsageStats::Sync();
}

SessionUsageObserver::~SessionUsageObserver() {
  // Explicitly calls Sync() to delete usage stats existing data including
  // metadata for migration.
  UsageStats::Sync();
}

void SessionUsageObserver::UsageCache::Clear() {
  touch_event.clear();
  miss_touch_event.clear();
}

// static
bool SessionUsageObserver::SaveCachedStats(void *data) {
  UsageCache *cache = reinterpret_cast<UsageCache *>(data);

  {
    absl::MutexLock l(&g_stats_cache_mutex);
    if (!cache->touch_event.empty()) {
      UsageStats::StoreTouchEventStats("VirtualKeyboardStats",
                                       cache->touch_event);
    }
    if (!cache->miss_touch_event.empty()) {
      UsageStats::StoreTouchEventStats("VirtualKeyboardMissStats",
                                       cache->miss_touch_event);
    }
    cache->Clear();
  }

  if (!UsageStats::Sync()) {
    LOG(ERROR) << "Updated internal cache of UsageStats but "
               << "failed to sync its data to disk";
    return false;
  } else {
    VLOG(3) << "Save Stats";
    return true;
  }
}

void SessionUsageObserver::EvalCreateSession(
    const commands::Input &input, const commands::Output &output,
    std::map<uint64_t, SessionState> *states) {
  // Number of create session
  SessionState state;
  state.set_id(output.id());
  state.set_created_time(GetTimeInMilliSecond());
  // TODO(toshiyuki): LRU?
  if (states->size() <= kMaxSession) {
    states->insert(std::make_pair(output.id(), state));
  }
}

void SessionUsageObserver::UpdateState(const commands::Input &input,
                                       const commands::Output &output,
                                       SessionState *state) {
  // Preedit
  if (!state->has_preedit() && output.has_preedit()) {
    // Start preedit
    state->set_start_preedit_time(GetTimeInMilliSecond());
  } else if (state->has_preedit() && output.has_preedit()) {
    // Continue preedit
  } else if (state->has_preedit() && !output.has_preedit()) {
    // Finish preedit
    UsageStats::UpdateTiming("PreeditDurationMSec",
                             GetDuration(state->start_preedit_time()));
  } else {
    // no preedit
  }

  // Candidates
  if (!state->has_candidates() && output.has_candidates()) {
    const commands::Candidates &cands = output.candidates();
    switch (cands.category()) {
      case commands::CONVERSION:
        state->set_start_conversion_window_time(GetTimeInMilliSecond());
        break;
      case commands::PREDICTION:
        state->set_start_prediction_window_time(GetTimeInMilliSecond());
        break;
      case commands::SUGGESTION:
        state->set_start_suggestion_window_time(GetTimeInMilliSecond());
        break;
      default:
        LOG(WARNING) << "candidate window has invalid category";
        break;
    }
  } else if (state->has_candidates() &&
             state->candidates().category() == commands::SUGGESTION) {
    if (!output.has_candidates() ||
        output.candidates().category() != commands::SUGGESTION) {
      const uint32_t suggestion_duration =
          GetDuration(state->start_suggestion_window_time());
      UsageStats::UpdateTiming("SuggestionWindowDurationMSec",
                               suggestion_duration);
    }
    if (output.has_candidates()) {
      switch (output.candidates().category()) {
        case commands::CONVERSION:
          state->set_start_conversion_window_time(GetTimeInMilliSecond());
          break;
        case commands::PREDICTION:
          state->set_start_prediction_window_time(GetTimeInMilliSecond());
          break;
        case commands::SUGGESTION:
          // continue suggestion
          break;
        default:
          LOG(WARNING) << "candidate window has invalid category";
          break;
      }
    }
  } else if (state->has_candidates() &&
             state->candidates().category() == commands::PREDICTION) {
    if (!output.has_candidates() ||
        output.candidates().category() != commands::PREDICTION) {
      const uint64_t predict_duration =
          GetDuration(state->start_prediction_window_time());
      UsageStats::UpdateTiming("PredictionWindowDurationMSec",
                               predict_duration);
    }
    // no transition
  } else if (state->has_candidates() &&
             state->candidates().category() == commands::CONVERSION) {
    if (!output.has_candidates() ||
        output.candidates().category() != commands::CONVERSION) {
      const uint32_t conversion_duration =
          GetDuration(state->start_conversion_window_time());
      UsageStats::UpdateTiming("ConversionWindowDurationMSec",
                               conversion_duration);
    }
    // no transition
  }

  // Cascading window
  if ((!state->has_candidates() ||
       (state->has_candidates() && !state->candidates().has_subcandidates())) &&
      output.has_candidates() && output.candidates().has_subcandidates()) {
    UsageStats::IncrementCount("ShowCascadingWindow");
  }

  // Update Preedit
  if (output.has_preedit()) {
    *state->mutable_preedit() = output.preedit();
  } else {
    state->clear_preedit();
  }

  // Update Candidates
  if (output.has_candidates()) {
    *state->mutable_candidates() = output.candidates();
  } else {
    state->clear_candidates();
  }

  if ((!state->has_result() ||
       state->result().type() != commands::Result::STRING) &&
      output.has_result() &&
      output.result().type() == commands::Result::STRING) {
    state->set_committed(true);
  }

  // Update Result
  if (output.has_result()) {
    *state->mutable_result() = output.result();
  } else {
    state->clear_result();
  }
}

void SessionUsageObserver::UpdateClientSideStats(const commands::Input &input,
                                                 SessionState *state) {
  // TODO(hsumita): Extract GetEnumValueName and CamelCaseString as public
  //                method from SessionUsageStatsUtil and use it.

  switch (input.command().usage_stats_event()) {
    case commands::SessionCommand::INFOLIST_WINDOW_SHOW:
      if (!state->has_start_infolist_window_time()) {
        state->set_start_infolist_window_time(GetTimeInMilliSecond());
      }
      break;
    case commands::SessionCommand::INFOLIST_WINDOW_HIDE:
      if (state->has_start_infolist_window_time()) {
        const uint64_t infolist_duration =
            GetDuration(state->start_infolist_window_time());
        UsageStats::UpdateTiming("InfolistWindowDurationMSec",
                                 infolist_duration);
        state->clear_start_infolist_window_time();
      }
      break;
    case commands::SessionCommand::HANDWRITING_OPEN_EVENT:
      UsageStats::IncrementCount("HandwritingOpen");
      break;
    case commands::SessionCommand::HANDWRITING_COMMIT_EVENT:
      UsageStats::IncrementCount("HandwritingCommit");
      break;
    case commands::SessionCommand::CHARACTER_PALETTE_OPEN_EVENT:
      UsageStats::IncrementCount("CharacterPaletteOpen");
      break;
    case commands::SessionCommand::CHARACTER_PALETTE_COMMIT_EVENT:
      UsageStats::IncrementCount("CharacterPaletteCommit");
      break;
    case commands::SessionCommand::SOFTWARE_KEYBOARD_LAYOUT_LANDSCAPE:
      LOG_IF(DFATAL, !input.command().has_usage_stats_event_int_value())
          << "SOFTWARE_KEYBOARD_LAYOUT_LANDSCAPE stats must have int value.";
      UsageStats::SetInteger("SoftwareKeyboardLayoutLandscape",
                             input.command().usage_stats_event_int_value());
      break;
    case commands::SessionCommand::SOFTWARE_KEYBOARD_LAYOUT_PORTRAIT:
      LOG_IF(DFATAL, !input.command().has_usage_stats_event_int_value())
          << "SOFTWARE_KEYBOARD_LAYOUT_PORTRAIT stats must have int value.";
      UsageStats::SetInteger("SoftwareKeyboardLayoutPortrait",
                             input.command().usage_stats_event_int_value());
      break;
    case commands::SessionCommand::SUBMITTED_CANDIDATE_ROW_0:
      UsageStats::IncrementCount("SubmittedCandidateRow0");
      break;
    case commands::SessionCommand::SUBMITTED_CANDIDATE_ROW_1:
      UsageStats::IncrementCount("SubmittedCandidateRow1");
      break;
    case commands::SessionCommand::SUBMITTED_CANDIDATE_ROW_2:
      UsageStats::IncrementCount("SubmittedCandidateRow2");
      break;
    case commands::SessionCommand::SUBMITTED_CANDIDATE_ROW_3:
      UsageStats::IncrementCount("SubmittedCandidateRow3");
      break;
    case commands::SessionCommand::SUBMITTED_CANDIDATE_ROW_4:
      UsageStats::IncrementCount("SubmittedCandidateRow4");
      break;
    case commands::SessionCommand::SUBMITTED_CANDIDATE_ROW_5:
      UsageStats::IncrementCount("SubmittedCandidateRow5");
      break;
    case commands::SessionCommand::SUBMITTED_CANDIDATE_ROW_6:
      UsageStats::IncrementCount("SubmittedCandidateRow6");
      break;
    case commands::SessionCommand::SUBMITTED_CANDIDATE_ROW_7:
      UsageStats::IncrementCount("SubmittedCandidateRow7");
      break;
    case commands::SessionCommand::SUBMITTED_CANDIDATE_ROW_8:
      UsageStats::IncrementCount("SubmittedCandidateRow8");
      break;
    case commands::SessionCommand::SUBMITTED_CANDIDATE_ROW_9:
      UsageStats::IncrementCount("SubmittedCandidateRow9");
      break;
    case commands::SessionCommand::SUBMITTED_CANDIDATE_ROW_GE10:
      UsageStats::IncrementCount("SubmittedCandidateRowGE10");
      break;
    case commands::SessionCommand::KEYBOARD_FOLD_EVENT:
      UsageStats::IncrementCount("KeyboardFoldEvent");
      break;
    case commands::SessionCommand::KEYBOARD_EXPAND_EVENT:
      UsageStats::IncrementCount("KeyboardExpandEvent");
      break;
    case commands::SessionCommand::MUSHROOM_SELECTION_DIALOG_OPEN_EVENT:
      UsageStats::IncrementCount("MushroomSelectionDialogOpen");
      break;
    case commands::SessionCommand::
        SOFTWARE_KEYBOARD_LAYOUT_ADJUSTMENT_ENABLED_LANDSCAPE:
      LOG_IF(DFATAL, !input.command().has_usage_stats_event_int_value())
          << "SOFTWARE_KEYBOARD_LAYOUT_ADJUSTMENT_ENABLED_LANDSCAPE stats"
          << " must have int value.";
      UsageStats::SetBoolean("SoftwareKeyboardLayoutAdjustmentEnabledLandscape",
                             input.command().usage_stats_event_int_value() > 0);
      break;
    case commands::SessionCommand::
        SOFTWARE_KEYBOARD_LAYOUT_ADJUSTMENT_ENABLED_PORTRAIT:
      LOG_IF(DFATAL, !input.command().has_usage_stats_event_int_value())
          << "SOFTWARE_KEYBOARD_LAYOUT_ADJUSTMENT_ENABLED_PORTRAIT stats"
          << " must have int value.";
      UsageStats::SetBoolean("SoftwareKeyboardLayoutAdjustmentEnabledPortrait",
                             input.command().usage_stats_event_int_value() > 0);
      break;
    case commands::SessionCommand::SOFTWARE_KEYBOARD_LAYOUT_ENGLISH_LANDSCAPE:
      LOG_IF(DFATAL, !input.command().has_usage_stats_event_int_value())
          << "SOFTWARE_KEYBOARD_LAYOUT_ENGLISH_LANDSCAPE stats"
          << " must have int value.";
      UsageStats::SetInteger("SoftwareKeyboardLayoutEnglishLandscape",
                             input.command().usage_stats_event_int_value());
      break;
    case commands::SessionCommand::SOFTWARE_KEYBOARD_LAYOUT_ENGLISH_PORTRAIT:
      LOG_IF(DFATAL, !input.command().has_usage_stats_event_int_value())
          << "SOFTWARE_KEYBOARD_LAYOUT_ENGLISH_PORTRAIT stats"
          << " must have int value.";
      UsageStats::SetInteger("SoftwareKeyboardLayoutEnglishPortrait",
                             input.command().usage_stats_event_int_value());
      break;
    default:
      LOG(DFATAL) << "client side usage stats event has invalid category: "
                  << input.command().usage_stats_event();
      break;
  }
}

void SessionUsageObserver::StoreTouchEventStats(
    const commands::Input::TouchEvent &touch_event,
    usage_stats::TouchEventStatsMap *touch_event_stats_map) {
  if (!config::StatsConfigUtil::IsEnabled()) {
    return;
  }
  absl::MutexLock l(&g_stats_cache_mutex);

  usage_stats::Stats::TouchEventStats *touch_event_stats =
      &(*touch_event_stats_map)[touch_event.source_id()];
  if (!touch_event_stats->has_source_id()) {
    touch_event_stats->set_source_id(touch_event.source_id());
  }
  if (touch_event.stroke_size() > 0) {
    const commands::Input::TouchPosition &first_pos = touch_event.stroke(0);
    const commands::Input::TouchPosition &last_pos =
        touch_event.stroke(touch_event.stroke_size() - 1);
    AddToDoubleValueStats(first_pos.x(),
                          touch_event_stats->mutable_start_x_stats());
    AddToDoubleValueStats(last_pos.x() - first_pos.x(),
                          touch_event_stats->mutable_direction_x_stats());
    AddToDoubleValueStats(first_pos.y(),
                          touch_event_stats->mutable_start_y_stats());
    AddToDoubleValueStats(last_pos.y() - first_pos.y(),
                          touch_event_stats->mutable_direction_y_stats());
    AddToDoubleValueStats(
        (last_pos.timestamp() - first_pos.timestamp()) / 1000.0,
        touch_event_stats->mutable_time_length_stats());
  }
}

void SessionUsageObserver::LogTouchEvent(const commands::Input &input,
                                         const commands::Output &output,
                                         const SessionState &state) {
  // When the input field type is PASSWORD, do not log the touch events.
  if (state.has_input_field_type() &&
      (state.input_field_type() == commands::Context::PASSWORD)) {
    return;
  }

  if (!state.has_request() || !state.request().has_keyboard_name()) {
    return;
  }
  const std::string &keyboard_name = state.request().keyboard_name();

  // When last_touchevents_ is not empty and BACKSPACE is pressed,
  // save last_touchevents_ as miss_touch_event.
  if (!last_touchevents_.empty() && input.has_key() &&
      input.key().has_special_key() &&
      input.key().special_key() == commands::KeyEvent::BACKSPACE &&
      state.has_preedit()) {
    for (size_t i = 0; i < last_touchevents_.size(); ++i) {
      StoreTouchEventStats(last_touchevents_[i],
                           &usage_cache_.miss_touch_event[keyboard_name]);
    }
    last_touchevents_.clear();
  }

  if (input.touch_events_size() > 0) {
    if (input.type() == commands::Input::SEND_KEY) {
      // When the input command contains TouchEvent and the type is SEND_KEY,
      // save the TouchEvent to last_touchevents_.
      // This last_touchevents_ will be aggregated to touch_event_stat_cache_
      // or miss_touch_event_stat_cache_ and be cleared when the subsequent
      // command will be received.
      for (size_t i = 0; i < input.touch_events_size(); ++i) {
        last_touchevents_.push_back(input.touch_events(i));
      }
    } else {
      // When the input command contains TouchEvent and the type isn't SEND_KEY,
      // save the TouchEvent to touch_event_stat_cache_.
      for (size_t i = 0; i < input.touch_events_size(); ++i) {
        StoreTouchEventStats(input.touch_events(i),
                             &usage_cache_.touch_event[keyboard_name]);
      }
    }
  }
}

void SessionUsageObserver::EvalCommandHandler(
    const commands::Command &command) {
  const commands::Input &input = command.input();
  const commands::Output &output = command.output();

  if (input.type() == commands::Input::CREATE_SESSION) {
    EvalCreateSession(input, output, &states_);
    SaveCachedStats(&usage_cache_);
    return;
  }

  // Session independent command usually has no session ID.
  if (IsSessionIndependentCommand(input.type())) {
    return;
  } else if (!input.has_id()) {
    LOG(WARNING) << "no id. input type: " << input.type();
    // Should have id
    return;
  }

  if (input.id() == 0) {
    VLOG(3) << "id == 0";
    return;
  }

  std::map<uint64_t, SessionState>::iterator iter = states_.find(input.id());
  if (iter == states_.end()) {
    LOG(WARNING) << "unknown session";
    // Unknown session
    return;
  }
  SessionState *state = &iter->second;
  DCHECK(state);

  if (input.type() == commands::Input::DELETE_SESSION) {
    const uint32_t session_duration = GetDuration(state->created_time());
    UsageStats::UpdateTiming("SessionDurationMSec", session_duration);

    states_.erase(iter);
    SaveCachedStats(&usage_cache_);
    return;
  }

  // Backspace key after commit
  if (state->committed() &&
      // for Applications supporting TEST_SEND_KEY
      (input.type() == commands::Input::TEST_SEND_KEY ||
       // other Applications
       input.type() == commands::Input::SEND_KEY)) {
    if (input.has_key() && input.key().has_special_key() &&
        input.key().special_key() == commands::KeyEvent::BACKSPACE &&
        state->has_result() &&
        state->result().type() == commands::Result::STRING) {
      UsageStats::IncrementCount("BackSpaceAfterCommit");
      // Count only one for each submitted result.
    }
    state->set_committed(false);
  }

  // Client side event
  if ((input.type() == commands::Input::SEND_COMMAND) &&
      (input.has_command()) &&
      (input.command().type() == commands::SessionCommand::USAGE_STATS_EVENT) &&
      (input.command().has_usage_stats_event())) {
    UpdateClientSideStats(input, state);
  }

  // Evals touch events and saves touch event stats.
  LogTouchEvent(input, output, *state);

  if ((input.type() == commands::Input::SEND_COMMAND ||
       input.type() == commands::Input::SEND_KEY) &&
      output.has_consumed() && output.consumed()) {
    // update states only when input was consumed
    UpdateState(input, output, state);
  }
  if (input.type() == commands::Input::SET_REQUEST) {
    if (input.has_request()) {
      *state->mutable_request() = input.request();
    }
  }
  // Saves input field type
  if ((input.type() == commands::Input::SEND_COMMAND) &&
      (input.has_command()) &&
      (input.command().type() ==
       commands::SessionCommand::SWITCH_INPUT_FIELD_TYPE) &&
      (input.context().has_input_field_type())) {
    state->set_input_field_type(input.context().input_field_type());
  }
}

}  // namespace session
}  // namespace mozc

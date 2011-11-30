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

#include "session/session_usage_observer.h"

#ifdef OS_WINDOWS
#include <time.h>  // time()
#else
#include <sys/time.h>  // time()
#endif

#include <limits.h>

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include "base/base.h"
#include "base/config_file_stream.h"
#include "base/singleton.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "session/commands.pb.h"
#include "session/internal/keymap.h"
#include "session/state.pb.h"
#include "usage_stats/usage_stats.h"

namespace mozc {
namespace session {

namespace {
const size_t kMaxSession = 64;
const size_t kDefaultSaveInterval = 500;
const char kIMEOnCommand[] = "IMEOn";
const char kIMEOffCommand[] = "IMEOff";
// Used for selected_indices
// We use negative integer for transliterated candidates,
// but we have transliterated candidates at most 20 or so.
// So we can use INT_MIN for special meaning.
const int kSelectDirectly = INT_MIN;

void ExtractActivationKeys(istream *ifs, set<string> *keys) {
  DCHECK(keys);
  string line;
  getline(*ifs, line);  // first line is comment.
  while (getline(*ifs, line)) {
    Util::ChopReturns(&line);
    if (line.empty() || line[0] == '#') {
      // empty or comment
      continue;
    }
    vector<string> rules;
    Util::SplitStringUsing(line, "\t", &rules);
    if (rules.size() == 3 &&
        (rules[2] == kIMEOnCommand || rules[2] == kIMEOffCommand)) {
      keys->insert(line);
    }
  }
}

config::Config::SessionKeymap kKeyMaps[] = {
  config::Config::ATOK,
  config::Config::MSIME,
  config::Config::KOTOERI,
};

bool IMEActivationKeyCustomized() {
  const config::Config::SessionKeymap keymap = GET_CONFIG(session_keymap);
  if (keymap != config::Config::CUSTOM) {
    return false;
  }
  const string &custom_keymap_table = GET_CONFIG(custom_keymap_table);
  istringstream ifs_custom(custom_keymap_table);
  set<string> customized;
  ExtractActivationKeys(&ifs_custom, &customized);
  for (size_t i = 0; i < arraysize(kKeyMaps); ++i) {
    const char *keymap_file =
        keymap::KeyMapManager::GetKeyMapFileName(kKeyMaps[i]);
    scoped_ptr<istream> ifs(ConfigFileStream::Open(keymap_file));
    if (ifs.get() == NULL) {
      LOG(ERROR) << "can not open default keymap table " << i;
      continue;
    }
    set<string> keymap_table;
    ExtractActivationKeys(ifs.get(), &keymap_table);
    if (includes(keymap_table.begin(), keymap_table.end(),
                 customized.begin(), customized.end())) {
      // customed keymap is subset of preset keymap
      return false;
    }
  }
  return true;
}

// Set current config data to registry
// This is expected not to be called so often, so we do not cache this.
void SetConfigStats() {
  const uint32 keymap = GET_CONFIG(session_keymap);
  usage_stats::UsageStats::SetInteger("ConfigSessionKeymap", keymap);
  const uint32 preedit_method = GET_CONFIG(preedit_method);
  usage_stats::UsageStats::SetInteger("ConfigPreeditMethod", preedit_method);
  const bool custom_roman = (!GET_CONFIG(custom_roman_table).empty() &&
                             preedit_method == config::Config::ROMAN);
  usage_stats::UsageStats::SetBoolean("ConfigCustomRomanTable", custom_roman);
  const uint32 punctuation_method = GET_CONFIG(punctuation_method);
  usage_stats::UsageStats::SetInteger("ConfigPunctuationMethod",
                                      punctuation_method);
  const uint32 symbol_method = GET_CONFIG(symbol_method);
  usage_stats::UsageStats::SetInteger("ConfigSymbolMethod", symbol_method);
  const uint32 history_level = GET_CONFIG(history_learning_level);
  usage_stats::UsageStats::SetInteger("ConfigHistoryLearningLevel",
                                      history_level);

  const bool use_date = GET_CONFIG(use_date_conversion);
  usage_stats::UsageStats::SetBoolean("ConfigUseDateConversion", use_date);
  const bool use_single_kanji = GET_CONFIG(use_single_kanji_conversion);
  usage_stats::UsageStats::SetBoolean("ConfigUseSingleKanjiConversion",
                                      use_single_kanji);
  const bool use_symbol = GET_CONFIG(use_symbol_conversion);
  usage_stats::UsageStats::SetBoolean("ConfigUseSymbolConversion", use_symbol);
  const bool use_number = GET_CONFIG(use_number_conversion);
  usage_stats::UsageStats::SetBoolean("ConfigUseNumberConversion", use_number);
  const bool use_emoticon = GET_CONFIG(use_emoticon_conversion);
  usage_stats::UsageStats::SetBoolean("ConfigUseEmoticonConversion",
                                      use_emoticon);
  const bool use_calculator = GET_CONFIG(use_calculator);
  usage_stats::UsageStats::SetBoolean("ConfigUseCalculator", use_calculator);
  const bool use_t13n = GET_CONFIG(use_t13n_conversion);
  usage_stats::UsageStats::SetBoolean("ConfigUseT13nConversion", use_t13n);
  const bool use_zip_code = GET_CONFIG(use_zip_code_conversion);
  usage_stats::UsageStats::SetBoolean("ConfigUseZipCodeConversion",
                                      use_zip_code);
  const bool use_spelling_correction = GET_CONFIG(use_spelling_correction);
  usage_stats::UsageStats::SetBoolean("ConfigUseSpellingCorrection",
                                      use_spelling_correction);
  const bool incognito = GET_CONFIG(incognito_mode);
  usage_stats::UsageStats::SetBoolean("ConfigIncognito", incognito);

  const uint32 selection = GET_CONFIG(selection_shortcut);
  usage_stats::UsageStats::SetInteger("ConfigSelectionShortcut", selection);

  const bool use_history = GET_CONFIG(use_history_suggest);
  usage_stats::UsageStats::SetBoolean("ConfigUseHistorySuggest", use_history);
  const bool use_dictionary = GET_CONFIG(use_dictionary_suggest);
  usage_stats::UsageStats::SetBoolean("ConfigUseDictionarySuggest",
                                      use_dictionary);
  const bool use_realtime_conversion = GET_CONFIG(use_realtime_conversion);
  usage_stats::UsageStats::SetBoolean("ConfigUseRealtimeConversion",
                                      use_realtime_conversion);

  const uint32 suggest_size = GET_CONFIG(suggestions_size);
  usage_stats::UsageStats::SetInteger("ConfigSuggestionsSize", suggest_size);

  const bool use_auto_ime_turn_off = GET_CONFIG(use_auto_ime_turn_off);
  usage_stats::UsageStats::SetBoolean("ConfigUseAutoIMETurnOff",
                                      use_auto_ime_turn_off);
  const bool use_cascading_window = GET_CONFIG(use_cascading_window);
  usage_stats::UsageStats::SetBoolean("ConfigUseCascadingWindow",
                                      use_cascading_window);

  const uint32 shift = GET_CONFIG(shift_key_mode_switch);
  usage_stats::UsageStats::SetInteger("ConfigShiftKeyModeSwitch", shift);
  const uint32 space = GET_CONFIG(space_character_form);
  usage_stats::UsageStats::SetInteger("ConfigSpaceCharacterForm", space);
  const uint32 numpad = GET_CONFIG(numpad_character_form);
  usage_stats::UsageStats::SetInteger("ConfigNumpadCharacterForm", numpad);

  const bool use_auto_conversion = GET_CONFIG(use_auto_conversion);
  usage_stats::UsageStats::SetBoolean("ConfigUseAutoConversion",
                                      use_auto_conversion);
  const uint32 auto_conversion_key = GET_CONFIG(auto_conversion_key);
  usage_stats::UsageStats::SetInteger("ConfigAutoConversionKey",
                                      auto_conversion_key);

  const uint32 yen_sign_character = GET_CONFIG(yen_sign_character);
  usage_stats::UsageStats::SetInteger("ConfigYenSignCharacter",
                                      yen_sign_character);

  const bool use_japanese_layout = GET_CONFIG(use_japanese_layout);
  usage_stats::UsageStats::SetBoolean("ConfigUseJapaneseLayout",
                                       use_japanese_layout);

  const bool ime_activation_key_customized = IMEActivationKeyCustomized();
  usage_stats::UsageStats::SetBoolean("IMEActivationKeyCustomized",
                                      ime_activation_key_customized);
}

// Return true if the value is in the candidate.
bool FindInCandidates(const string &value,
                      const commands::CandidateList &candidates) {
  for (size_t i = 0; i < candidates.candidates_size(); ++i) {
    if (value == candidates.candidates(i).value()) {
      return true;
    }
  }
  return false;
}

// Return session state mode from candidate mode
session::SessionState::Mode GetSessionModeFromCandidateList(
    const commands::CandidateList &candidate_list) {
  switch (candidate_list.category()) {
    case commands::CONVERSION:
      return session::SessionState::CONVERSION;
    case commands::PREDICTION:
      return session::SessionState::PREDICTION;
    case commands::SUGGESTION:
      return session::SessionState::SUGGESTION;
    default:
      DLOG(FATAL) << "invalid candidate category";
      return session::SessionState::COMPOSITION;
  }
}

// Return true if input is mouse select command
bool IsMouseSelect(const commands::Input &input) {
  return (input.type() == commands::Input::SEND_COMMAND &&
          input.has_command() &&
          (input.command().type() ==
              commands::SessionCommand::SELECT_CANDIDATE ||
           input.command().type() ==
              commands::SessionCommand::SUBMIT_CANDIDATE));
}

// Return true if resegmented
bool IsResegmented(const commands::Preedit &prev_preedit,
                   const commands::Preedit &cur_preedit) {
  if (prev_preedit.segment_size() != cur_preedit.segment_size()) {
    return true;
  }
  int changed_segment = 0;
  for (size_t i = 0; i < prev_preedit.segment_size(); ++i) {
    if (prev_preedit.segment(i).key() != cur_preedit.segment(i).key()) {
      ++changed_segment;
    }
  }
  return (changed_segment > 1);
}

// Return true if given preedits are same in value.
bool IsSamePreedit(const commands::Preedit &prev_preedit,
                   const commands::Preedit &cur_preedit) {
  if (prev_preedit.segment_size() != cur_preedit.segment_size()) {
    return false;
  }
  for (size_t i = 0; i < prev_preedit.segment_size(); ++i) {
    if (prev_preedit.segment(i).value() != cur_preedit.segment(i).value()) {
      return false;
    }
  }
  return true;
}

// Find candidate with value and set its index to |idx|.
// Return false if |value| is not found in candidates.
bool GetSelectedIndex(const string &value,
                      const commands::Candidates &candidates, int *idx) {
  DCHECK(idx != NULL);
  *idx = 0;
  if (candidates.has_subcandidates() &&
      (candidates.subcandidates().category() == commands::TRANSLITERATION)) {
    for (size_t i = 0; i < candidates.subcandidates().candidate_size(); ++i) {
      if (value == candidates.subcandidates().candidate(i).value()) {
        const int t13n_idx = candidates.subcandidates().candidate(i).index();
        // t13n candidates
        *idx = (-t13n_idx - 1);
        return true;
      }
    }
  }
  for (size_t i = 0; i < candidates.candidate_size(); ++i) {
    if (value == candidates.candidate(i).value()) {
      *idx = candidates.candidate(i).index();
      return true;
    }
  }
  return false;
}

// Return true if state's candidates have given category
bool CheckCandidateCategory(const SessionState *state,
                            commands::Category category) {
  return (state->has_candidates() &&
          state->candidates().category() == category);
}

}  // namespace

class EventConverter {
 public:
  EventConverter() {
    specialkey_map_[commands::KeyEvent::NO_SPECIALKEY] = "NO_SPECIALKEY";
    specialkey_map_[commands::KeyEvent::DIGIT] = "DIGIT";
    specialkey_map_[commands::KeyEvent::ON] = "ON";
    specialkey_map_[commands::KeyEvent::OFF] = "OFF";
    specialkey_map_[commands::KeyEvent::SPACE] = "SPACE";
    specialkey_map_[commands::KeyEvent::ENTER] = "ENTER";
    specialkey_map_[commands::KeyEvent::LEFT] = "LEFT";
    specialkey_map_[commands::KeyEvent::RIGHT] = "RIGHT";
    specialkey_map_[commands::KeyEvent::UP] = "UP";
    specialkey_map_[commands::KeyEvent::DOWN] = "DOWN";
    specialkey_map_[commands::KeyEvent::ESCAPE] = "ESCAPE";
    specialkey_map_[commands::KeyEvent::DEL] = "DEL";
    specialkey_map_[commands::KeyEvent::BACKSPACE] = "BACKSPACE";
    specialkey_map_[commands::KeyEvent::HENKAN] = "HENKAN";
    specialkey_map_[commands::KeyEvent::MUHENKAN] = "MUHENKAN";
    specialkey_map_[commands::KeyEvent::KANA] = "KANA";
    specialkey_map_[commands::KeyEvent::HOME] = "HOME";
    specialkey_map_[commands::KeyEvent::END] = "END";
    specialkey_map_[commands::KeyEvent::TAB] = "TAB";
    specialkey_map_[commands::KeyEvent::F1] = "F1";
    specialkey_map_[commands::KeyEvent::F2] = "F2";
    specialkey_map_[commands::KeyEvent::F3] = "F3";
    specialkey_map_[commands::KeyEvent::F4] = "F4";
    specialkey_map_[commands::KeyEvent::F5] = "F5";
    specialkey_map_[commands::KeyEvent::F6] = "F6";
    specialkey_map_[commands::KeyEvent::F7] = "F7";
    specialkey_map_[commands::KeyEvent::F8] = "F8";
    specialkey_map_[commands::KeyEvent::F9] = "F9";
    specialkey_map_[commands::KeyEvent::F10] = "F10";
    specialkey_map_[commands::KeyEvent::F11] = "F11";
    specialkey_map_[commands::KeyEvent::F12] = "F12";
    specialkey_map_[commands::KeyEvent::PAGE_UP] = "PAGE_UP";
    specialkey_map_[commands::KeyEvent::PAGE_DOWN] = "PAGE_DOWN";
    specialkey_map_[commands::KeyEvent::INSERT] = "INSERT";
    specialkey_map_[commands::KeyEvent::F13] = "F13";
    specialkey_map_[commands::KeyEvent::F14] = "F14";
    specialkey_map_[commands::KeyEvent::F15] = "F15";
    specialkey_map_[commands::KeyEvent::F16] = "F16";
    specialkey_map_[commands::KeyEvent::F17] = "F17";
    specialkey_map_[commands::KeyEvent::F18] = "F18";
    specialkey_map_[commands::KeyEvent::F19] = "F19";
    specialkey_map_[commands::KeyEvent::F20] = "F20";
    specialkey_map_[commands::KeyEvent::F21] = "F21";
    specialkey_map_[commands::KeyEvent::F22] = "F22";
    specialkey_map_[commands::KeyEvent::F23] = "F23";
    specialkey_map_[commands::KeyEvent::F24] = "F24";
    specialkey_map_[commands::KeyEvent::EISU] = "EISU";
    specialkey_map_[commands::KeyEvent::NUMPAD0] = "NUMPAD0";
    specialkey_map_[commands::KeyEvent::NUMPAD1] = "NUMPAD1";
    specialkey_map_[commands::KeyEvent::NUMPAD2] = "NUMPAD2";
    specialkey_map_[commands::KeyEvent::NUMPAD3] = "NUMPAD3";
    specialkey_map_[commands::KeyEvent::NUMPAD4] = "NUMPAD4";
    specialkey_map_[commands::KeyEvent::NUMPAD5] = "NUMPAD5";
    specialkey_map_[commands::KeyEvent::NUMPAD6] = "NUMPAD6";
    specialkey_map_[commands::KeyEvent::NUMPAD7] = "NUMPAD7";
    specialkey_map_[commands::KeyEvent::NUMPAD8] = "NUMPAD8";
    specialkey_map_[commands::KeyEvent::NUMPAD9] = "NUMPAD9";
    specialkey_map_[commands::KeyEvent::MULTIPLY] = "MULTIPLY";
    specialkey_map_[commands::KeyEvent::ADD] = "ADD";
    specialkey_map_[commands::KeyEvent::SEPARATOR] = "SEPARATOR";
    specialkey_map_[commands::KeyEvent::SUBTRACT] = "SUBTRACT";
    specialkey_map_[commands::KeyEvent::DECIMAL] = "DECIMAL";
    specialkey_map_[commands::KeyEvent::DIVIDE] = "DIVIDE";
    specialkey_map_[commands::KeyEvent::EQUALS] = "EQUALS";
    specialkey_map_[commands::KeyEvent::ASCII] = "ASCII";
    specialkey_map_[commands::KeyEvent::HANKAKU] = "HANKAKU";
    specialkey_map_[commands::KeyEvent::KANJI] = "KANJI";
    specialkey_map_[commands::KeyEvent::KATAKANA] = "KATAKANA";
  }

  const map<uint32, string> &GetSpecialKeyMap() const {
    return specialkey_map_;
  }

 private:
  map<uint32, string> specialkey_map_;
};

SessionUsageObserver::SessionUsageObserver() :
    update_count_(0),
    save_interval_(kDefaultSaveInterval) {
  SetConfigStats();
}

SessionUsageObserver::~SessionUsageObserver() {
  SaveStats();
}

void SessionUsageObserver::SetInterval(uint32 val) {
  save_interval_ = val;
}

void SessionUsageObserver::SaveStats() {
  for (map<string, uint32>::const_iterator iter = count_cache_.begin();
       iter != count_cache_.end(); ++iter) {
    usage_stats::UsageStats::IncrementCountBy(iter->first, iter->second);
  }
  count_cache_.clear();

  for (map<string, vector<uint32> >::const_iterator iter
           = timing_cache_.begin();
       iter != timing_cache_.end(); ++iter) {
    usage_stats::UsageStats::UpdateTimingBy(iter->first, iter->second);
  }
  timing_cache_.clear();

  for (map<string, int>::const_iterator iter = integer_cache_.begin();
       iter != integer_cache_.end(); ++iter) {
    usage_stats::UsageStats::SetInteger(iter->first, iter->second);
  }
  integer_cache_.clear();

  for (map<string, bool>::const_iterator iter = boolean_cache_.begin();
       iter != boolean_cache_.end(); ++iter) {
    usage_stats::UsageStats::SetBoolean(iter->first, iter->second);
  }
  boolean_cache_.clear();


  update_count_ = 0;
  usage_stats::UsageStats::Sync();
  VLOG(3) << "Save Stats";
}

void SessionUsageObserver::IncrementCount(const string &name) {
  IncrementCountBy(name, 1);
}

void SessionUsageObserver::IncrementCountBy(const string &name,
                                            uint64 count) {
  DCHECK(usage_stats::UsageStats::IsListed(name))
      << name << " is not in the stats list";
  count_cache_[name] += count;
  ++update_count_;
  if (update_count_ >= save_interval_) {
    SaveStats();
  }
}

void SessionUsageObserver::UpdateTiming(const string &name, uint64 val) {
  DCHECK(usage_stats::UsageStats::IsListed(name))
      << name << " is not in the stats list";
  timing_cache_[name].push_back(val);
  ++update_count_;
  if (update_count_ >= save_interval_) {
    SaveStats();
  }
}

void SessionUsageObserver::SetInteger(const string &name, int val) {
  DCHECK(usage_stats::UsageStats::IsListed(name))
      << name << " is not in the stats list";
  integer_cache_[name] = val;
  ++update_count_;
  if (update_count_ >= save_interval_) {
    SaveStats();
  }
}

void SessionUsageObserver::SetBoolean(const string &name, bool val) {
  DCHECK(usage_stats::UsageStats::IsListed(name))
      << name << " is not in the stats list";
  boolean_cache_[name] = val;
  ++update_count_;
  if (update_count_ >= save_interval_) {
    SaveStats();
  }
}

void SessionUsageObserver::EvalCreateSession(const commands::Input &input,
                                             const commands::Output &output,
                                             map<uint64, SessionState> *states) {
  // Number of create session
  IncrementCount("SessionCreated");
  SessionState state;
  state.set_id(output.id());
  state.set_created_time(time(NULL));
  // TODO(toshiyuki): LRU?
  if (states->size() <= kMaxSession) {
    states->insert(make_pair(output.id(), state));
  }
}

void SessionUsageObserver::UpdateMode(const commands::Input &input,
                                      const commands::Output &output,
                                      SessionState *state) const {
  if (!output.has_preedit()) {
    state->set_mode(session::SessionState::COMPOSITION);
    return;
  }

  bool has_highlighted = false;
  for (size_t i = 0; i < output.preedit().segment_size(); ++i) {
    if (output.preedit().segment(i).annotation() ==
        commands::Preedit::Segment::HIGHLIGHT) {
      has_highlighted = true;
      break;
    }
  }

  if (!has_highlighted) {
    state->set_mode(session::SessionState::COMPOSITION);
    return;
  }

  // Mouse select and no candidate window now.
  if (IsMouseSelect(input)) {
    if (state->has_all_candidate_words()) {
      state->set_mode(
          GetSessionModeFromCandidateList(state->all_candidate_words()));
    }
    return;
  }

  if (output.has_all_candidate_words()) {
    state->set_mode(
        GetSessionModeFromCandidateList(output.all_candidate_words()));
    return;
  }

  if (state->mode() == session::SessionState::COMPOSITION) {
    // First conversion
    state->set_mode(session::SessionState::CONVERSION);
  }
}

// Update selected indices
void SessionUsageObserver::UpdateSelectedIndices(const commands::Input &input,
                                                 const commands::Output &output,
                                                 SessionState *state) const {
  if (!output.has_preedit()) {
    state->clear_selected_indices();
    return;
  }

  if (state->selected_indices_size() == 0) {
    for (size_t i = 0; i < output.preedit().segment_size(); ++i) {
      state->add_selected_indices(0);
    }
  }

  if (IsSamePreedit(state->preedit(), output.preedit())) {
    // no change
    return;
  }

  if (IsResegmented(state->preedit(), output.preedit())) {
    // When the conversion result is resegmented, keep the
    // unchanged indices and set others to '0'.
    vector<int> new_indices;
    int changed_idx = 0;
    for (size_t i = 0; i < output.preedit().segment_size(); ++i) {
      if ((output.preedit().segment(i).annotation() ==
           commands::Preedit::Segment::HIGHLIGHT)) {
        changed_idx = i;
        break;
      }
    }
    for (size_t i = 0; i < changed_idx; ++i) {
      new_indices.push_back(state->selected_indices(i));
    }
    for (size_t i = changed_idx; i < output.preedit().segment_size(); ++i) {
      new_indices.push_back(0);
    }
    state->clear_selected_indices();
    for (size_t i = 0; i < new_indices.size(); ++i) {
      state->add_selected_indices(new_indices[i]);
    }
  } else {
    int changed_idx = 0;
    // Find target segment.
    for (size_t i = 0; i < output.preedit().segment_size(); ++i) {
      if ((output.preedit().segment(i).annotation() ==
           commands::Preedit::Segment::HIGHLIGHT)) {
        changed_idx = i;
        break;
      }
    }
    const string &new_value = output.preedit().segment(changed_idx).value();
    int idx = 0;
    if (output.has_candidates() &&
        GetSelectedIndex(new_value, output.candidates(), &idx)) {
      state->set_selected_indices(changed_idx, idx);
    } else if (IsMouseSelect(input) &&
               state->has_candidates() &&
               GetSelectedIndex(new_value, state->candidates(), &idx)) {
      // Candidate can be selected by mouse
      state->set_selected_indices(changed_idx, idx);
    } else {
      state->set_selected_indices(changed_idx, kSelectDirectly);
    }
  }
}

void SessionUsageObserver::UpdateState(const commands::Input &input,
                                       const commands::Output &output,
                                       SessionState *state) {
  // Preedit
  if (!state->has_preedit() && output.has_preedit()) {
    // Start preedit
    state->set_start_preedit_time(time(NULL));
  } else if (state->has_preedit() && output.has_preedit()) {
    // Continue preedit
  } else if (state->has_preedit() && !output.has_preedit()) {
    // Finish preedit
    const uint64 duration = time(NULL) - state->start_preedit_time();
    UpdateTiming("PreeditDuration", duration);
  } else {
    // no preedit
  }

  // Candidates
  if (!state->has_candidates() && output.has_candidates()) {
    const commands::Candidates &cands = output.candidates();
    switch (cands.category()) {
      case commands::CONVERSION:
        state->set_start_conversion_window_time(time(NULL));
        break;
      case commands::PREDICTION:
        state->set_start_prediction_window_time(time(NULL));
        break;
      case commands::SUGGESTION:
        state->set_start_suggestion_window_time(time(NULL));
        break;
      default:
        LOG(WARNING) << "candidate window has invalid category";
        break;
    }
  } else if (state->has_candidates() &&
             state->candidates().category() == commands::SUGGESTION) {
    if (!output.has_candidates() ||
        output.candidates().category() != commands::SUGGESTION) {
      const uint64 suggest_duration
          = time(NULL) - state->start_suggestion_window_time();
      UpdateTiming("SuggestionWindowDuration",
                   suggest_duration);
    }
    if (output.has_candidates()) {
      switch (output.candidates().category()) {
        case commands::CONVERSION:
          state->set_start_conversion_window_time(time(NULL));
          break;
        case commands::PREDICTION:
          state->set_start_prediction_window_time(time(NULL));
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
      const uint64 predict_duration
          = time(NULL) - state->start_prediction_window_time();
      UpdateTiming("PredictionWindowDuration",
                   predict_duration);
    }
    // no transition
  } else if (state->has_candidates() &&
             state->candidates().category() == commands::CONVERSION) {
    if (!output.has_candidates() ||
        output.candidates().category() != commands::CONVERSION) {
      const uint64 conversion_duration
          = time(NULL) - state->start_conversion_window_time();
      UpdateTiming("ConversionWindowDuration",
                   conversion_duration);
    }
    // no transition
  }

  UpdateSelectedIndices(input, output, state);
  UpdateMode(input, output, state);

  // Cascading window
  if ((!state->has_candidates() ||
       (state->has_candidates() &&
        !state->candidates().has_subcandidates())) &&
      output.has_candidates() && output.candidates().has_subcandidates()) {
    IncrementCount("ShowCascadingWindow");
  }

  // Update Preedit
  if (output.has_preedit()) {
    state->mutable_preedit()->CopyFrom(output.preedit());
  } else {
    state->clear_preedit();
  }

  // Update Candidates
  if (output.has_candidates()) {
    state->mutable_candidates()->CopyFrom(output.candidates());
  } else {
    state->clear_candidates();
  }
  if (output.has_all_candidate_words()) {
    state->mutable_all_candidate_words()->
        CopyFrom(output.all_candidate_words());
  } else {
    state->clear_all_candidate_words();
  }

  if ((!state->has_result() ||
       state->result().type() != commands::Result::STRING) &&
      output.has_result() &&
      output.result().type() == commands::Result::STRING) {
    state->set_committed(true);
  }

  // Update Result
  if (output.has_result()) {
    state->mutable_result()->CopyFrom(output.result());
  } else {
    state->clear_result();
  }
}

void SessionUsageObserver::UpdateClientSideStats(const commands::Input &input,
                                                 SessionState *state) {
  switch (input.command().usage_stats_event()) {
    case commands::SessionCommand::INFOLIST_WINDOW_SHOW:
      if (!state->has_start_infolist_window_time()) {
        state->set_start_infolist_window_time(time(NULL));
      }
      break;
    case commands::SessionCommand::INFOLIST_WINDOW_HIDE:
      if (state->has_start_infolist_window_time()) {
        const uint64 infolist_duration
            = time(NULL) - state->start_infolist_window_time();
        DLOG(INFO) << "infolist_duration:" << infolist_duration;
        UpdateTiming("InfolistWindowDuration", infolist_duration);
        state->clear_start_infolist_window_time();
      }
      break;
    default:
      LOG(WARNING) << "client side usage stats event has invalid category";
      break;
  }
}

void SessionUsageObserver::EvalSendKey(const commands::Input &input,
                                       const commands::Output &output) {
  if (input.has_key() && input.key().has_key_code()) {
    // Number of consumed ASCII(printable) typing
    IncrementCount("ASCIITyping");
  }

  if (input.has_key() && input.key().has_special_key()) {
    // Number of consumed Non-ASCII (special key) typing
    IncrementCount("NonASCIITyping");
    const map<uint32, string> special_key_map =
        Singleton<EventConverter>::get()->GetSpecialKeyMap();
    map<uint32, string>::const_iterator iter =
        special_key_map.find(input.key().special_key());
    if (iter != special_key_map.end()) {
      IncrementCount(iter->second);
    }
  }
}

void SessionUsageObserver::UpdateCandidateStats(const string &base_name,
                                                uint32 index) {
  if (index <= 9) {
    const string stats_name = base_name + Util::SimpleItoa(index);
    IncrementCount(stats_name);
  } else {
    const string stats_name = base_name + "GE10";
    IncrementCount(stats_name);
  }
}

void SessionUsageObserver::CheckOutput(const commands::Input &input,
                                       const commands::Output &output,
                                       const SessionState *state) {
  if (!output.has_result() ||
      output.result().type() != commands::Result::STRING) {
    // No commit string
    return;
  }

  // commit preedit
  IncrementCount("Commit");

  const string &submit_value = output.result().value();

  if (state->mode() == session::SessionState::SUGGESTION ||
      (CheckCandidateCategory(state, commands::SUGGESTION) &&
       FindInCandidates(submit_value, state->all_candidate_words()))) {
    // We should check the candidate contents because suggestion
    // candidates are shown automatically.
    IncrementCount("CommitFromSuggestion");
    if (input.command().type() == commands::SessionCommand::SUBMIT_CANDIDATE ||
        state->selected_indices_size() == 0) {
      // Committed zero-query suggest candidate
      UpdateCandidateStats("SuggestionCandidates", input.command().id());
    } else {
      const uint32 index = state->selected_indices(0);
      if (index == kSelectDirectly) {
        // Treat as top candidate
        UpdateCandidateStats("SuggestionCandidates", 0);
      } else {
        UpdateCandidateStats("SuggestionCandidates", index);
      }
    }
  } else if (state->mode() == session::SessionState::PREDICTION ||
             CheckCandidateCategory(state, commands::PREDICTION)) {
    IncrementCount("CommitFromPrediction");
    DCHECK(state->selected_indices_size() == 1);
    const uint32 index = state->selected_indices(0);
    if (index == kSelectDirectly) {
      // Treat as top candidate
      UpdateCandidateStats("PredictionCandidates", 0);
    } else {
      UpdateCandidateStats("PredictionCandidates", index);
    }
  } else if (state->mode() == session::SessionState::CONVERSION ||
             CheckCandidateCategory(state, commands::CONVERSION)) {
    IncrementCount("CommitFromConversion");
    for (size_t i = 0; i < state->selected_indices_size(); ++i) {
      const int index = state->selected_indices(i);
      if (index == kSelectDirectly) {
        // Treat as top conversion candidate
        // This may treat 'F8' result to 'ConversionCandidates0'.
        UpdateCandidateStats("ConversionCandidates", 0);
      } else  if (index < 0) {
        const int t13n_index = -index-1;
        UpdateCandidateStats("TransliterationCandidates", t13n_index);
      } else {
        UpdateCandidateStats("ConversionCandidates", index);
      }
    }
  } else if (state->has_preedit()) {
    IncrementCount("CommitFromComposition");
  }

  if (state->has_preedit()) {
    uint64 total_len = 0;
    for (size_t i = 0; i < state->preedit().segment_size(); ++i) {
      const uint32 len = state->preedit().segment(i).value_length();
      total_len += len;
      UpdateTiming("SubmittedSegmentLength", len);
    }
    UpdateTiming("SubmittedLength", total_len);
    UpdateTiming("SubmittedSegmentNumber",state->preedit().segment_size());
    IncrementCountBy("SubmittedTotalLength", total_len);
  } else {
    // Zero-query Suggest
    size_t length = Util::CharsLen(submit_value.c_str(), submit_value.size());
    UpdateTiming("SubmittedSegmentLength", length);
    UpdateTiming("SubmittedLength", length);
    UpdateTiming("SubmittedSegmentNumber", 1);
    IncrementCountBy("SubmittedTotalLength", length);
  }
}


void SessionUsageObserver::EvalCommandHandler(
    const commands::Command &command) {
  const commands::Input &input = command.input();
  const commands::Output &output = command.output();

  IncrementCount("SessionAllEvent");
  UpdateTiming("ElapsedTime", output.elapsed_time());

  if (output.has_performed_command() &&
      !output.performed_command().empty()) {
    IncrementCount("Performed_" + output.performed_command());
  }

  if (input.type() == commands::Input::SEND_KEY) {
    if (output.has_consumed() && output.consumed()) {
      IncrementCount("ConsumedSendKey");
    } else {
      IncrementCount("UnconsumedSendKey");
    }
    EvalSendKey(input, output);
  }

  if (input.type() == commands::Input::CREATE_SESSION) {
    EvalCreateSession(input, output, &states_);
    SaveStats();
    return;
  } else if (!input.has_id()) {
    LOG(WARNING) << "no id";
    // Should have id
    return;
  }

  if (input.type() == commands::Input::SET_CONFIG) {
    IncrementCount("SetConfig");
    SetConfigStats();
  }
  if (input.type() == commands::Input::SHUTDOWN) {
    IncrementCount("ShutDown");
  }
  if (input.type() == commands::Input::CLEAR_USER_HISTORY) {
    IncrementCount("ClearUserHistory");
  }
  if (input.type() == commands::Input::CLEAR_USER_PREDICTION) {
    IncrementCount("ClearUserPrediction");
  }
  if (input.type() == commands::Input::CLEAR_UNUSED_USER_PREDICTION) {
    IncrementCount("ClearUnusedUserPrediction");
  }

  if (input.id() == 0) {
    VLOG(3) << "id == 0";
    return;
  }

  map<uint64, SessionState>::iterator iter = states_.find(input.id());
  if (iter == states_.end()) {
    LOG(WARNING) << "unknown session";
    // Unknown session
    return;
  }
  SessionState *state = &iter->second;
  DCHECK(state);

  if (input.type() == commands::Input::DELETE_SESSION) {
    // Session duration sec
    const uint64 duration = time(NULL) - state->created_time();
    UpdateTiming("SessionDuration", duration);

    states_.erase(iter);
    SaveStats();
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
      IncrementCount("BackSpaceAfterCommit");
      // Count only one for each submitted result.
    }
    state->set_committed(false);
  }

  if (IsMouseSelect(input)) {
    IncrementCount("MouseSelect");
  }

  // Client side event
  if ((input.type() == commands::Input::SEND_COMMAND) &&
      (input.has_command()) &&
      (input.command().type() ==
           commands::SessionCommand::USAGE_STATS_EVENT) &&
      (input.command().has_usage_stats_event())) {
    UpdateClientSideStats(input, state);
  }


  if ((input.type() == commands::Input::SEND_COMMAND ||
       input.type() == commands::Input::SEND_KEY) &&
      output.has_consumed() &&
      output.consumed()) {
    // update states only when input was consumed
    CheckOutput(input, output, state);
    UpdateState(input, output, state);
  }
}

void SessionUsageObserver::Reload() {
}

}  // namespace mozc::session
}  // namespace mozc

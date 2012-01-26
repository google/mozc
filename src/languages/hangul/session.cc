// Copyright 2010-2012, Google Inc.
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

// TODO(nona) : Create HangulConversionInterface for testing.

#include "languages/hangul/session.h"

#include <pwd.h>
#include <hangul-1.0/hangul.h>
#include <cctype>
#include <set>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "session/internal/keymap.h"
#include "session/key_parser.h"

using mozc::commands::KeyEvent;
using mozc::config::HangulConfig;

namespace mozc {

namespace {
// TODO(nona): Unify config updating mechanism like session_handler.
uint64 g_last_config_updated = 0;
}  // namespace anonymous

namespace hangul {
namespace {

// TODO(nona) calculate this value by the platform-specific APIs.
const int kCandidatesPerPage = 10;
const char kWonKeyCode = '\\';

// Converts UCS char array to string.
void UcscharToString(const char32 *text, string *output) {
  DCHECK(output);
  output->clear();
  for (size_t i = 0; text[i] != 0x00; ++i) {
    Util::UCS4ToUTF8Append(text[i], output);
  }
}

// Sets Hangul string into output::Result.
bool SetResultToOutput(const string &text, commands::Output *output) {
  if (text.empty()) {
    return false;
  }
  commands::Result *result = output->mutable_result();
  result->set_type(commands::Result::STRING);
  result->set_value(text);
  return true;
}

// Sets Hangul string into output::Preedit.
bool SetPreeditToOutput(const string &text, commands::Output *output) {
  if (text.empty()) {
    return false;
  }
  // In hangul, count of segment is always 1.
  DCHECK(output->preedit().segment_size() == 0);
  const size_t unicode_length = Util::CharsLen(text);
  commands::Preedit::Segment *segment =
      output->mutable_preedit()->add_segment();
  segment->set_annotation(commands::Preedit::Segment::HIGHLIGHT);
  segment->set_value(text);
  segment->set_value_length(unicode_length);
  output->mutable_preedit()->set_cursor(unicode_length);
  return true;
}

// Appends utf8 string into deque<char32>.
void AppendUTF8ToUcscharDeque(const string &utf8str, deque<char32> *output) {
  DCHECK(output);
  const char *begin = utf8str.data();
  const char *end = begin + utf8str.size();
  while (begin < end) {
    size_t mblen = 0;
    const char32 w = Util::UTF8ToUCS4(begin, end, &mblen);
    output->push_back(w);
    begin += mblen;
  }
}

// Converts UCS deque object to UTF8 string.
void UcscharDequeToUTF8(const deque<char32> &deqchar, string *output) {
  DCHECK(output);
  output->clear();
  for (size_t i = 0; i < deqchar.size(); ++i) {
    Util::UCS4ToUTF8Append(deqchar[i], output);
  }
}

string GetSymbolTableFileName() {
  // TODO(nona): Support multi-platform.
  return "/usr/share/ibus-mozc-hangul/korean_symbols.txt";
}

// Holds the mapping between mozc HangulConfig enum and libhangul config string.
class HangulConfigMap {
 public:
  HangulConfigMap() {
    keyboard_types_map_[HangulConfig::KEYBOARD_Dubeolsik] = "2";
    keyboard_types_map_[HangulConfig::KEYBOARD_DubeolsikYetgeul] = "2y";
    keyboard_types_map_[HangulConfig::KEYBOARD_SebeolsikDubeol] = "32";
    keyboard_types_map_[HangulConfig::KEYBOARD_Sebeolsik390] = "39";
    keyboard_types_map_[HangulConfig::KEYBOARD_SebeolsikFinal] = "3f";
    keyboard_types_map_[HangulConfig::KEYBOARD_SebeolsikNoshift] = "3s";
    keyboard_types_map_[HangulConfig::KEYBOARD_SebeolsikYetgeul] = "3y";
    keyboard_types_map_[HangulConfig::KEYBOARD_Romaja] = "ro";
    keyboard_types_map_[HangulConfig::KEYBOARD_Ahnmatae] = "ahn";
  }

  string GetKeyboardTypeId(HangulConfig::KeyboardTypes type) const {
    map<HangulConfig::KeyboardTypes, string>::const_iterator it =
        keyboard_types_map_.find(type);
    if (it != keyboard_types_map_.end()) {
      return it->second;
    } else {
      return "2";
    }
  }

  static bool AddKeySetByKeyString(string key_string,
                                   set<keymap::Key> *key_set) {
    DCHECK(key_set);

    if (key_string.empty()) {
      return false;
    }

    commands::KeyEvent key_event;
    if (!KeyParser::ParseKey(key_string, &key_event)) {
      DLOG(WARNING) << "Cannot parse key string: " << key_string;
      return false;
    }

    keymap::Key key;
    if (!keymap::GetKey(key_event, &key)) {
      return false;
    }

    key_set->insert(key);
    return true;
  }

 private:
  map<HangulConfig::KeyboardTypes, string> keyboard_types_map_;
};
}  // namespace

Session::Session()
    // Initial keyboard setting (following "2" value) is ignored because of
    // overwriting default of config.proto.
    : context_(hangul_ic_new("2")),
      hanja_table_(::hanja_table_load(NULL)),
      hanja_list_(NULL),
      hanja_index_(-1),
      current_mode_(kHangulMode),
      create_session_time_(Util::GetTime()),
      last_command_time_(0),
      last_config_updated_(0) {
  ResetConfig();
  ReloadSymbolDictionary(GetSymbolTableFileName());
}

Session::~Session() {
  ResetHanjaList();
  ::hanja_table_delete(hanja_table_);
  if (symbol_table_ != NULL) {
    ::hanja_table_delete(symbol_table_);
  }
  ::hangul_ic_delete(context_);
}

bool Session::ResetHanjaList() {
  if (hanja_list_ == NULL) {
    return false;
  }
  ::hanja_list_delete(hanja_list_);
  hanja_list_ = NULL;
  hanja_index_ = -1;
  return true;
}

void Session::GetPreeditString(string *result) const {
  string reproducible_preedit;
  const char32 *preedit = ::hangul_ic_get_preedit_string(context_);
  UcscharToString(preedit, &reproducible_preedit);
  if (current_mode_ == kHanjaLockMode) {
    UcscharDequeToUTF8(hanja_lock_preedit_, result);
    result->append(reproducible_preedit);
  } else {
    result->assign(reproducible_preedit);
  }
}

void Session::GetCommitString(string *result) const {
  const char32 *commit = ::hangul_ic_get_commit_string(context_);
  UcscharToString(commit, result);
}

bool Session::FlushPreedit(string *result) {
  bool return_value;
  if (::hangul_ic_is_empty(context_) && hanja_lock_preedit_.empty()) {
    return_value = false;
  } else {
    return_value = true;
  }

  if (result == NULL) {
    ::hangul_ic_flush(context_);
    hanja_lock_preedit_.clear();
    return return_value;
  }

  if (current_mode_ == kHanjaLockMode) {
    UcscharDequeToUTF8(hanja_lock_preedit_, result);
    hanja_lock_preedit_.clear();
  }

  string repro_preedit;
  const char32 *flush = ::hangul_ic_flush(context_);
  UcscharToString(flush, &repro_preedit);
  result->append(repro_preedit);
  return return_value;
}

bool Session::HanjaLookup(commands::Command *command) {
  DCHECK(hanja_table_);
  DCHECK(command);

  commands::Output *output = command->mutable_output();
  ResetHanjaList();

  string preedit;
  GetPreeditString(&preedit);
  hanja_list_ = ::hanja_table_match_prefix(hanja_table_, preedit.c_str());
  if (symbol_table_ != NULL && ::hanja_list_get_size(hanja_list_) == 0) {
    ResetHanjaList();
    hanja_list_ = ::hanja_table_match_prefix(symbol_table_, preedit.c_str());
  }
  const int total_candidates = ::hanja_list_get_size(hanja_list_);
  if (total_candidates == 0) {
    ResetHanjaList();
    SetPreeditToOutput(preedit, output);
    return false;
  }
  UpdateCandidate(INITIAL_SELECTED_CANDIDATE, command);
  return true;
}

bool Session::HasReproduciblePreedit() {
  const char32 *preedit = ::hangul_ic_get_preedit_string(context_);
  return preedit && preedit[0] != '\0';
}

bool Session::IsKeyEventForShortcut(const commands::KeyEvent &key_event) {
  // Event without modifier keys cannot be a shortcut.
  if (key_event.modifier_keys_size() == 0) {
    return false;
  }
  // Single CAPS with a printable key cannot be a shortcut.
  if (key_event.modifier_keys_size() == 1 &&
      key_event.modifier_keys(0) == commands::KeyEvent::CAPS &&
      key_event.has_key_code()) {
    return false;
  }
  return true;
}

// TODO(nona) : Introduce CandidateList.
void Session::UpdateCandidate(CandidateOperation candidate_operation,
                              commands::Command *command) {
  if (!IsHanjaSelectionMode()) {
    DLOG(ERROR) << "Do not call if there are no candidates to show";
    return;
  }

  string preedit;
  GetPreeditString(&preedit);
  SetPreeditToOutput(preedit, command->mutable_output());

  const int total_candidates = ::hanja_list_get_size(hanja_list_);
  if (total_candidates == 0) {
    DLOG(ERROR) << "total_candidates should not be zero";
    return;
  }

  switch (candidate_operation) {
    case NEXT_CANDIDATE:
      ++hanja_index_;
      break;
    case PREV_CANDIDATE:
      hanja_index_ = (hanja_index_ == 0) ?
          total_candidates - 1 : hanja_index_ - 1;
      break;
    case NEXT_PAGE_CANDIDATE:
      // If next page is not available, NEXT_PAGE_CANDIDATE operation set to end
      // candidate.
      hanja_index_ = (hanja_index_ + kCandidatesPerPage >= total_candidates) ?
          total_candidates - 1 : hanja_index_ + kCandidatesPerPage;
      break;
    case PREV_PAGE_CANDIDATE:
      // If previous page is not available, PREV_NEXT_CANDIDATE operation set to
      // head of candidate.
      hanja_index_ = (hanja_index_ - kCandidatesPerPage < 0) ?
          0 : hanja_index_ - kCandidatesPerPage;
      break;
    case INITIAL_SELECTED_CANDIDATE:
      hanja_index_ = 0;
      break;
    case DO_NOT_CHANGE_SELECTED_CANDIDATE:
      // do nothing.
      break;
    default:
      DLOG(FATAL);
  }

  // TODO(nona): Is rounding candidate desired?
  hanja_index_ %= total_candidates;

  // Paging.
  const int current_page_index = hanja_index_ / kCandidatesPerPage;
  const int current_page_start_index = current_page_index * kCandidatesPerPage;
  const int page_border_index =
      min(total_candidates, current_page_start_index + kCandidatesPerPage);

  commands::Candidates *candidates =
      command->mutable_output()->mutable_candidates();
  candidates->Clear();
  candidates->set_size(total_candidates);

  for (int i = current_page_start_index; i < page_border_index; ++i) {
    const Hanja *hanja = ::hanja_list_get_nth(hanja_list_, i);
    commands::Candidates::Candidate *new_candidate =
        candidates->add_candidate();
    new_candidate->set_id(i);
    new_candidate->set_index(i);
    new_candidate->set_value(::hanja_get_value(hanja));
  }

  const char *comment = hanja_list_get_nth_comment(hanja_list_, hanja_index_);
  commands::Footer *footer = candidates->mutable_footer();
  footer->set_label(comment);

  const string kDigits = "1234567890";
  {
    // Logic here is copied from SessionOutput::FillShortcuts.  We
    // can't reuse this at this time because SessionOutput depends on
    // converter/segments.cc, which depends on the Japanese language
    // model.
    // TODO(mukai): extract FillShortcuts() method to another library.
    const size_t num_loop = min(
        static_cast<size_t>(candidates->candidate_size()),
        kDigits.size());
    for (size_t i = 0; i < num_loop; ++i) {
      const string shortcut = kDigits.substr(i, 1);
      candidates->mutable_candidate(i)->mutable_annotation()->
          set_shortcut(shortcut);
    }
  }

  candidates->set_focused_index(hanja_index_);
  candidates->set_direction(commands::Candidates::HORIZONTAL);
  candidates->set_position(0);
}

// TODO(nona): make backspace key customizable.
// The detail of backspace key behavior is available at following URL.
// http://code.google.com/p/chromium-os/issues/detail?id=15192
bool Session::ProcessBSKey(commands::Command *command) {
  commands::Output *output = command->mutable_output();


  if (current_mode_ == kHangulMode) {
    if (IsHanjaSelectionMode()) {
      UpdateCandidate(DO_NOT_CHANGE_SELECTED_CANDIDATE, command);
      return true;
    }
    if (::hangul_ic_is_empty(context_)) {
      return false;
    }

    string preedit;
    hangul_ic_backspace(context_);
    GetPreeditString(&preedit);
    SetPreeditToOutput(preedit, output);
    return true;
  }

  if (!::hangul_ic_is_empty(context_)) {
    hangul_ic_backspace(context_);
  } else {
    if (hanja_lock_preedit_.empty()) {
      return false;
    }
    hanja_lock_preedit_.pop_back();
  }

  if (!HasReproduciblePreedit()) {
    // This flushing is same as ibus-hangul.
    // TODO(nona): holding rest preedit (I think this is more natural).
    CancelContext(output);
  } else {
    HanjaLookup(command);
  }
  return true;
}

bool Session::SelectCandidateByShortcut(const int id) {
  if (!IsHanjaSelectionMode() || hanja_index_ < 0 || id < 0 || id >= 10) {
    return false;
  }

  const int current_page_index = hanja_index_ / kCandidatesPerPage;
  const int current_page_start_index = current_page_index * kCandidatesPerPage;
  const int selected_hanja_index = current_page_start_index + id;

  const int total_candidates = ::hanja_list_get_size(hanja_list_);
  if (selected_hanja_index >= total_candidates) {
    return false;
  }
  hanja_index_ = selected_hanja_index;
  return true;
}

bool Session::GetSelectedCandidate(string *result) {
  if (!IsHanjaSelectionMode() || hanja_index_ < 0) {
    return false;
  }

  const Hanja *hanja = hanja_list_get_nth(hanja_list_, hanja_index_);
  result->assign(::hanja_get_value(hanja));
  return true;
}

bool Session::CommitSelectedCandidate(commands::Command *command) {
  commands::Output *output = command->mutable_output();
  string result;
  if (!GetSelectedCandidate(&result)) {
    return false;
  }

  SetResultToOutput(result, output);
  ResetHanjaList();
  UpdatePreeditWithHanjaString(result, command);
  return true;
}

bool Session::ProcessWonKey(commands::Command *command) {
  // The details of won key behavior can be seen in crosbug.com/15947
  // TODO(nona): make customizable in ChromeOS.
  const KeyEvent &key_event = command->input().key();
  commands::Output *output = command->mutable_output();

  if (key_event.key_code() != kWonKeyCode) {
    return false;
  }

  string hangul_string;
  FlushPreedit(&hangul_string);
  // "₩"
  hangul_string.append("\xE2\x82\xA9");
  SetResultToOutput(hangul_string, output);
  return true;
}

bool Session::ProcessHanjaSelection(commands::Command *command) {
  const KeyEvent &key_event = command->input().key();

  // When hanja selection window is shown, user can only do selecting hanja or
  // canceling conversion. After commit hanja character does not anything.
  if (isdigit(key_event.key_code())) {
    // If candidate window is appeared, commit candidate corresponding
    // index. Following number conversion is required because shortcut
    // number string is {1,2,3,...,9,0}, and candidate index is
    // {0,1,...8,9}.
    const int selected_id = (key_event.key_code() - '0' + 9) % 10;
    if (SelectCandidateByShortcut(selected_id)) {
      CommitSelectedCandidate(command);
    } else {
      UpdateCandidate(DO_NOT_CHANGE_SELECTED_CANDIDATE, command);
    }
    return true;
  }

  if (key_event.has_special_key()) {
    switch (key_event.special_key()) {
      case KeyEvent::BACKSPACE:
        return ProcessBSKey(command);
        break;
      case KeyEvent::LEFT:
        UpdateCandidate(PREV_CANDIDATE, command);
        return true;
        break;
      case KeyEvent::RIGHT:
        UpdateCandidate(NEXT_CANDIDATE, command);
        return true;
        break;
      case KeyEvent::UP:
        UpdateCandidate(PREV_PAGE_CANDIDATE, command);
        return true;
        break;
      case KeyEvent::DOWN:
        UpdateCandidate(NEXT_PAGE_CANDIDATE, command);
        return true;
        break;
      case KeyEvent::ENTER:
        CommitSelectedCandidate(command);
        return true;
        break;
      case KeyEvent::HANJA:
      case KeyEvent::ESCAPE:
        CancelHanjaSelection(command->mutable_output());
        return true;
        break;
      default:
        // Other special key treat as normal keys.
        break;
    }
  }

  if (current_mode_ == kHanjaLockMode && key_event.modifier_keys_size() == 0) {
    return ProcessKeyEvent(command);
  } else {
    // Other keys are not handled when hanja selection window is shown in hangul
    // mode.
    UpdateCandidate(DO_NOT_CHANGE_SELECTED_CANDIDATE, command);
    return true;
  }
}

bool Session::IsHanjaSelectionMode() {
  return hanja_list_ != NULL;
}

// SendKey processes a key event.  Hangul input method does not care special
// keybinds with modifiers (such like Ctrl-N), so here ignores such key events
// in advance, and then treats other key events with libhangul commands.
// However, Won-key is a special case -- it can emit Won character
// (U+FFE6 or U+20A9) with some key combination and libhangul does not care it.
// Thus we need to handle won key at first.
bool Session::ProcessHangulMode(commands::Command *command) {
  const KeyEvent &key_event = command->input().key();
  commands::Output *output = command->mutable_output();

  if (IsHanjaSelectionMode()) {
    // All key is consumed on hanja selection mode.
    ProcessHanjaSelection(command);
    return true;
  }

  // Treat won key specially.
  if (ProcessWonKey(command)) {
    return true;
  }

  // Hangul input does not have special shortcut commands with modifiers.
  // TODO(nona): Support hot key like alternative hanja key.
  // The details of special key events are available at following URL.
  // http://code.google.com/p/chromium-os/issues/detail?id=4319
  // Then, we cancel context if |key_event| has other than printable input.
  if (IsKeyEventForShortcut(key_event)) {
    CancelContext(output);
    return false;
  }

  if (key_event.has_special_key()) {
    switch (key_event.special_key()) {
      // TODO(nona): use user configured key map.
      case KeyEvent::BACKSPACE:
        return ProcessBSKey(command);
        break;
      case KeyEvent::HANJA:
        HanjaLookup(command);
        return true;
        break;
      default:
        // Hangul input treat non hangul key as default after committing.
        CancelContext(output);
        return false;
        break;
    }
  }

  return ProcessKeyEvent(command);
}

void Session::CancelContext(commands::Output *output) {
  DCHECK(output);
  string preedit;
  FlushPreedit(&preedit);
  SetResultToOutput(preedit, output);
  ResetHanjaList();
}

void Session::CancelHanjaSelection(commands::Output *output) {
  DCHECK(output);
  if (!IsHanjaSelectionMode()) {
    return;
  }
  string preedit;
  GetPreeditString(&preedit);
  SetPreeditToOutput(preedit, output);
  ResetHanjaList();
}

void Session::UpdatePreeditWithHanjaString(const string &hanja,
                                           commands::Command *command) {
  if (current_mode_ == kHangulMode) {
    FlushPreedit(NULL);
    return;
  }
  string preedit;
  GetPreeditString(&preedit);
  // The length of preedit and hanja are equal, that is, all preedits
  // are converted.
  if (Util::CharsLen(preedit) == Util::CharsLen(hanja)) {
    FlushPreedit(NULL);
    return;
  }

  for (size_t i = 0; i < Util::CharsLen(hanja); ++i) {
    hanja_lock_preedit_.pop_front();
  }

  HanjaLookup(command);
  return;
}

bool Session::ProcessKeyEvent(commands::Command *command) {
  const KeyEvent &key_event = command->input().key();
  commands::Output *output = command->mutable_output();

  if (!::hangul_ic_process(context_, key_event.key_code())) {
    // Even if key event is not used by hangul_ic_process, it commits current
    // preedit. This behavior is problematic on hanja-lock mode. In hanja-lock
    // mode, we should handle extended preedit(which is hanja_lock_preedit_). So
    // we should append committed string after extended preedit flushed.
    string commit, preedit;
    GetCommitString(&commit);
    FlushPreedit(&preedit);
    preedit.append(commit);
    SetResultToOutput(preedit, output);
    ResetHanjaList();
    return false;
  }
  string preedit, committed;
  GetPreeditString(&preedit);
  GetCommitString(&committed);

  if (current_mode_ == kHangulMode) {
    SetPreeditToOutput(preedit, output);
    SetResultToOutput(committed, output);
    return true;
  }

  AppendUTF8ToUcscharDeque(committed, &hanja_lock_preedit_);

  if (!HasReproduciblePreedit()) {
    CancelContext(output);
    return true;
  }

  HanjaLookup(command);
  return true;
}

// When Hanja-Lock mode is activated, hanja candidate will be shown any time if
// possible. If a part of preedit is commited, automatically shows hanja
// candidates with residual preedit.
bool Session::ProcessHanjaLockMode(commands::Command *command) {
  const KeyEvent &key_event = command->input().key();
  commands::Output *output = command->mutable_output();

  // Treat won key specially.
  if (ProcessWonKey(command)) {
    return true;
  }

  if (IsHanjaSelectionMode()) {
    return ProcessHanjaSelection(command);
  }
  if (key_event.modifier_keys_size() != 0) {
    CancelContext(output);
    return false;
  }
  if (key_event.has_special_key() &&
      key_event.special_key() == KeyEvent::BACKSPACE) {
    return ProcessBSKey(command);
  }
  return ProcessKeyEvent(command);
}

bool Session::SendKey(commands::Command *command) {
  // Hangul IME ignores Caps Lock state.
  // This routine reset caps lock with revesing inputed keys.
  commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
  if (key_event->has_key_code()) {
    int n = key_event->modifier_keys_size();
    for (int i = 0; i < n; ++i) {
      if (key_event->modifier_keys(i) == commands::KeyEvent::CAPS) {
        uint32 keyval = key_event->key_code();
        if (isupper(keyval)) {
          key_event->set_key_code(tolower(keyval));
        } else if (islower(keyval)) {
          key_event->set_key_code(toupper(keyval));
        }
        break;
      }
    }
  }

  // Hangul IME does not concern ordinal number key and numpad number key.
  // Following code replaces numpad event to corresponding ordinal key event.
  COMPILE_ASSERT((KeyEvent::NUMPAD0 < KeyEvent::NUMPAD9) &&
                 (KeyEvent::NUMPAD9 - KeyEvent::NUMPAD0 == 9),
                 NumPad_is_not_numerical_order);
  if (key_event->has_special_key() &&
      (key_event->special_key() >= KeyEvent::NUMPAD0 &&
       key_event->special_key() <= KeyEvent::NUMPAD9)) {
    key_event->set_key_code('0' +
                            (key_event->special_key() - KeyEvent::NUMPAD0));
    key_event->clear_special_key();
  }

  keymap::Key key;
  keymap::GetKey(command->input().key(), &key);
  if (hanja_key_set_.find(key) != hanja_key_set_.end()) {
    key_event->clear_key_code();
    key_event->clear_modifiers();
    key_event->clear_modifier_keys();
    key_event->set_special_key(commands::KeyEvent::HANJA);
  }

  if (current_mode_ == kHanjaLockMode) {
    command->mutable_output()->set_consumed(ProcessHanjaLockMode(command));
  } else {
    command->mutable_output()->set_consumed(ProcessHangulMode(command));
  }
  DLOG(INFO) << command->DebugString();
  return true;
}

bool Session::TestSendKey(commands::Command *command) {
  // TODO(nona): implement this.
  last_command_time_ = Util::GetTime();
  if (g_last_config_updated > last_config_updated_) {
    ResetConfig();
  }
  return true;
}

bool Session::SendCommand(commands::Command *command) {
  last_command_time_ = Util::GetTime();
  if (g_last_config_updated > last_config_updated_) {
    ResetConfig();
  }

  const commands::SessionCommand &session_command = command->input().command();
  bool consumed = false;
  string hangul_string;

  switch (session_command.type()) {
    case commands::SessionCommand::REVERT:
      RenewContext();
      consumed = true;
      break;
    case commands::SessionCommand::SUBMIT:
      CancelContext(command->mutable_output());
      break;
    case commands::SessionCommand::SELECT_CANDIDATE:
      SelectCandidateByShortcut(session_command.id());
      CommitSelectedCandidate(command);
      break;
    case commands::SessionCommand::SWITCH_INPUT_MODE:
      CancelContext(command->mutable_output());
      switch (session_command.composition_mode()) {
        case commands::HIRAGANA:
          current_mode_ = kHangulMode;
          consumed = true;
          break;
        case commands::FULL_ASCII:
          current_mode_ = kHanjaLockMode;
          consumed = true;
          break;
        default:
          // do nothing.
          DLOG(ERROR) << "Unexpected Command:" << command->DebugString();
          break;
      }
      hanja_lock_preedit_.clear();
      // status is never used in hangul, followin value is dummy.
      command->mutable_output()->mutable_status()->set_mode(commands::HIRAGANA);
      command->mutable_output()->mutable_status()->set_activated(true);
      break;
    default:
      // do nothing.
      DLOG(ERROR) << "Unexpected Session Command:" << command->DebugString();
      break;
  }

  DLOG(INFO) << command->DebugString();
  command->mutable_output()->set_consumed(consumed);
  return true;
}

void Session::RenewContext() {
  ResetHanjaList();
  hanja_lock_preedit_.clear();
  const HangulConfigMap *config_map = Singleton<HangulConfigMap>::get();
  const HangulConfig &hangul_config = GET_CONFIG(hangul_config);
  string keyboard_id =
      config_map->GetKeyboardTypeId(hangul_config.keyboard_type());
  hangul_ic_delete(context_);
  context_ = ::hangul_ic_new(keyboard_id.c_str());
}

void Session::ResetConfig() {
  const HangulConfigMap *config_map = Singleton<HangulConfigMap>::get();
  const HangulConfig &hangul_config = GET_CONFIG(hangul_config);

  string keyboard_id =
      config_map->GetKeyboardTypeId(hangul_config.keyboard_type());
  ::hangul_ic_select_keyboard(context_, keyboard_id.c_str());
  last_config_updated_ = Util::GetTime();

  hanja_key_set_.clear();
  for (size_t i = 0; i < hangul_config.hanja_keys_size(); ++i) {
    HangulConfigMap::AddKeySetByKeyString(hangul_config.hanja_keys(i),
                                          &hanja_key_set_);
  }
}

void Session::ReloadConfig() {
  last_command_time_ = Util::GetTime();
  ResetConfig();
}

#ifdef OS_CHROMEOS
void Session::UpdateConfig(const config::HangulConfig &config) {
  config::Config mozc_config;
  mozc_config.mutable_hangul_config()->MergeFrom(config);
  config::ConfigHandler::SetConfig(mozc_config);
  g_last_config_updated = Util::GetTime();
}
#endif  // OS_CHROMEOS

void Session::set_client_capability(const commands::Capability &capability) {
  // Do nothing.  Capability does not make sense with the current hangul.
}

void Session::set_application_info(
    const commands::ApplicationInfo &application_info) {
  application_info_.CopyFrom(application_info);
}

bool Session::ReloadSymbolDictionary(const string &symbol_dictionary_filename) {
  if (Util::FileExists(symbol_dictionary_filename)) {
    symbol_table_ = ::hanja_table_load(symbol_dictionary_filename.c_str());
  } else {
    symbol_table_ = NULL;
  }
  return (hanja_table_ != NULL) && (symbol_table_ != NULL);
}

const commands::ApplicationInfo &Session::application_info() const {
  return application_info_;
}

uint64 Session::create_session_time() const {
  return create_session_time_;
}

uint64 Session::last_command_time() const {
  return last_command_time_;
}

}  // namespace hangul
}  // namespace mozc

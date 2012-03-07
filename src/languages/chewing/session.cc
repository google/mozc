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

#include "languages/chewing/session.h"

#include <cctype>

#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "languages/chewing/scoped_chewing_ptr.h"
#include "session/key_event_util.h"

using mozc::commands::KeyEvent;
using mozc::config::ChewingConfig;

namespace mozc {
namespace {
// Returns the bytes used for the characters in the text.
int BytesForChars(const string &utf8_text, int characters) {
  int result = 0;
  int counted = 0;
  const char *text_data = utf8_text.data();
  while (result < utf8_text.size() && counted < characters) {
    int charLen = Util::OneCharLen(text_data);
    result += charLen;
    text_data += charLen;
    ++counted;
  }
  return result;
}

// Holds the mapping between mozc ChewingConfig enum and libchewing
// config enum.
class ChewingConfigMap {
 public:
  ChewingConfigMap() {
    keyboard_type_map_[ChewingConfig::DEFAULT] =
        ::chewing_KBStr2Num(const_cast<char *>("KB_DEFAULT"));
    keyboard_type_map_[ChewingConfig::HSU] =
        ::chewing_KBStr2Num(const_cast<char *>("KB_HSU"));
    keyboard_type_map_[ChewingConfig::IBM] =
        ::chewing_KBStr2Num(const_cast<char *>("KB_IBM"));
    keyboard_type_map_[ChewingConfig::GIN_YIEH] =
        ::chewing_KBStr2Num(const_cast<char *>("KB_GIN_YIEH"));
    keyboard_type_map_[ChewingConfig::ETEN] =
        ::chewing_KBStr2Num(const_cast<char *>("KB_ET"));
    keyboard_type_map_[ChewingConfig::ETEN26] =
        ::chewing_KBStr2Num(const_cast<char *>("KB_ET26"));
    keyboard_type_map_[ChewingConfig::DVORAK] =
        ::chewing_KBStr2Num(const_cast<char *>("KB_DVORAK"));
    keyboard_type_map_[ChewingConfig::DVORAK_HSU] =
        ::chewing_KBStr2Num(const_cast<char *>("KB_DVORAK_HSU"));
    keyboard_type_map_[ChewingConfig::DACHEN_26] =
        ::chewing_KBStr2Num(const_cast<char *>("KB_DACHEN_CP26"));
    keyboard_type_map_[ChewingConfig::HANYU] =
        ::chewing_KBStr2Num(const_cast<char *>("KB_HANYU_PINYIN"));

    selection_keys_map_[ChewingConfig::SELECTION_1234567890] = "1234567890";
    selection_keys_map_[ChewingConfig::SELECTION_asdfghjkl] = "asdfghjkl;";
    selection_keys_map_[ChewingConfig::SELECTION_asdfzxcv89] = "asdfzxcv89";
    selection_keys_map_[ChewingConfig::SELECTION_asdfjkl789] = "asdfjkl789";
    selection_keys_map_[ChewingConfig::SELECTION_aoeuqjkix] = "aoeu;qjkix";
    selection_keys_map_[ChewingConfig::SELECTION_aoeuhtnsid] = "aoeuhtnsid";
    selection_keys_map_[ChewingConfig::SELECTION_aoeuidhtns] = "aoeuidhtns";
    selection_keys_map_[ChewingConfig::SELECTION_1234qweras] = "1234qweras";

    hsu_selection_keys_map_[ChewingConfig::HSU_asdfjkl789] = HSU_SELKEY_TYPE1;
    hsu_selection_keys_map_[ChewingConfig::HSU_asdfzxcv89] = HSU_SELKEY_TYPE2;
  }

  int GetKeyboardTypeId(ChewingConfig::KeyboardType keyboard_type) const {
    map<ChewingConfig::KeyboardType, int>::const_iterator it =
        keyboard_type_map_.find(keyboard_type);
    if (it != keyboard_type_map_.end()) {
      return it->second;
    }
    return ::chewing_KBStr2Num(const_cast<char *>("KB_DEFAULT"));
  }

  string GetSelectionKeys(ChewingConfig::SelectionKeys selection_keys) const {
    map<ChewingConfig::SelectionKeys, string>::const_iterator it =
        selection_keys_map_.find(selection_keys);
    if (it != selection_keys_map_.end()) {
      return it->second;
    }

    return "";
  }

  int GetHsuSelectionKeys(
      ChewingConfig::HsuSelectionKeys hsu_selection_keys) const {
    map<ChewingConfig::HsuSelectionKeys, int>::const_iterator it =
        hsu_selection_keys_map_.find(hsu_selection_keys);
    if (it != hsu_selection_keys_map_.end()) {
      return it->second;
    }

    return HSU_SELKEY_TYPE1;
  }

 private:
  map<ChewingConfig::KeyboardType, int> keyboard_type_map_;
  map<ChewingConfig::SelectionKeys, string> selection_keys_map_;
  map<ChewingConfig::HsuSelectionKeys, int> hsu_selection_keys_map_;
};

uint64 g_last_config_updated = 0;
}  // anonymous namespace

namespace chewing {
Session::Session()
    : context_(::chewing_new()),
      state_(PRECOMPOSITION),
      create_session_time_(Util::GetTime()),
      last_command_time_(0),
      last_config_updated_(0) {
  ResetConfig();
}

Session::~Session() {
  chewing_delete(context_);
}

#define CHEWING_SET_CONFIG(field, name) \
  ::chewing_set_##name(context_, chewing_config.field())

void Session::ResetConfig() {
  const ChewingConfig &chewing_config = GET_CONFIG(chewing_config);
  CHEWING_SET_CONFIG(automatic_shift_cursor, autoShiftCur);
  CHEWING_SET_CONFIG(add_phrase_direction, addPhraseDirection);
  CHEWING_SET_CONFIG(easy_symbol_input, easySymbolInput);
  CHEWING_SET_CONFIG(escape_cleans_all_buffer, escCleanAllBuf);
  CHEWING_SET_CONFIG(phrase_choice_rearward, phraseChoiceRearward);
  CHEWING_SET_CONFIG(space_as_selection, spaceAsSelection);
  CHEWING_SET_CONFIG(maximum_chinese_character_length, maxChiSymbolLen);
  CHEWING_SET_CONFIG(candidates_per_page, candPerPage);

  const ChewingConfigMap *config_map = Singleton<ChewingConfigMap>::get();
  ::chewing_set_KBType(
      context_, config_map->GetKeyboardTypeId(chewing_config.keyboard_type()));
  ::chewing_set_hsuSelKeyType(
      context_,
      config_map->GetHsuSelectionKeys(chewing_config.hsu_selection_keys()));

  // Set up the selection keys
  string keys = config_map->GetSelectionKeys(chewing_config.selection_keys());

  // We always use a static size of selection keys, MAX_SELKEY (in
  // chewing/global.h) because libchewing needs it.
  if (keys.size() == MAX_SELKEY) {
    int keys_data[MAX_SELKEY];
    for (size_t i = 0; i < keys.size(); ++i) {
      keys_data[i] = static_cast<int>(keys[i]);
    }
    ::chewing_set_selKey(context_, keys_data, MAX_SELKEY);
  } else if (!keys.empty()) {
    LOG(ERROR) << "The size of selection keys has changed in libchewing";
  }
  last_config_updated_ = Util::GetTime();
}

#undef CHEWING_SET_CONFIG

void Session::RenewContext() {
  const int original_chi_eng_mode = ::chewing_get_ChiEngMode(context_);
  const int original_shape_mode = ::chewing_get_ShapeMode(context_);
  ::chewing_delete(context_);
  context_ = ::chewing_new();
  ResetConfig();
  ::chewing_set_ChiEngMode(context_, original_chi_eng_mode);
  ::chewing_set_ShapeMode(context_, original_shape_mode);
}

void Session::FillCandidates(commands::Candidates *candidates) {
  if (::chewing_cand_CheckDone(context_)) {
    return;
  }

  const int total_candidates = ::chewing_cand_TotalChoice(context_);
  if (total_candidates == 0) {
    return;
  }

  candidates->set_size(total_candidates);
  const int page_size = ::chewing_cand_ChoicePerPage(context_);
  ::chewing_cand_Enumerate(context_);
  scoped_chewing_ptr<int> selkeys(::chewing_get_selKey(context_));
  const int base_rank = page_size * ::chewing_cand_CurrentPage(context_);
  for (int i = 0; i < page_size && ::chewing_cand_hasNext(context_); ++i) {
    scoped_chewing_ptr<char> cand_text(::chewing_cand_String(context_));
    commands::Candidates::Candidate *new_candidate =
        candidates->add_candidate();
    new_candidate->set_id(base_rank + i);
    new_candidate->set_index(i);
    new_candidate->set_value(cand_text.get());
    new_candidate->mutable_annotation()->set_shortcut(
        string(1, static_cast<char>(selkeys.get()[i])));
  }
  candidates->set_direction(commands::Candidates::HORIZONTAL);
}

void Session::FillOutput(commands::Command *command) {
  DLOG(INFO) << ::chewing_get_KBString(context_);
  commands::Output *output = command->mutable_output();
  output->mutable_key()->CopyFrom(command->input().key());

  if (!output->consumed() && state_ == PRECOMPOSITION) {
    // Do not fill the result if the key is not consumed and current
    // status is PRECOMPOSITION (means not during the input).  In such
    // case it will fill the output of the previous status.
    return;
  }

  // Fill the result
  if (::chewing_commit_Check(context_)) {
    scoped_chewing_ptr<char> commit_text(::chewing_commit_String(context_));
    commands::Result *result = output->mutable_result();
    result->set_type(commands::Result::STRING);
    result->set_value(commit_text.get());
    if (::chewing_get_ChiEngMode(context_) == SYMBOL_MODE &&
        GET_CONFIG(chewing_config).force_lowercase_english()) {
      Util::LowerString(result->mutable_value());
    }
  }

  // Fill the preedit
  // Buffer means the chinese characters which are not comitted yet.
  string buffer;
  int buffer_len = 0;
  if (::chewing_buffer_Check(context_)) {
    // Buffer len means the number of characters, not bytes.
    buffer_len = ::chewing_buffer_Len(context_);
    scoped_chewing_ptr<char> buffer_text(::chewing_buffer_String(context_));
    buffer.assign(buffer_text.get());
  }

  // Zuin means the user-typed Zhuyin charcters.
  string zuin;
  int zuin_len = 0;
  {
    scoped_chewing_ptr<char> zuin_text(
        ::chewing_zuin_String(context_, &zuin_len));
    // zuin_len also means the number of characters, not bytes.
    if (zuin_len > 0) {
      zuin.assign(zuin_text.get());
    }
  }

  const int cursor = ::chewing_cursor_Current(context_);
  // Constract actual preedit structure.  We need to insert |zuin| at
  // the cursor position -- therefore, we need to split the buffer beforehand.
  {
    string pre_text;
    string trailing_text;
    int pre_len = 0;
    int trailing_len = 0;
    if (buffer_len > 0) {
      if (cursor < buffer_len) {
        int bytes = BytesForChars(buffer, cursor);
        pre_text = buffer.substr(0, bytes);
        trailing_text = buffer.substr(bytes);
        pre_len = cursor;
        trailing_len = buffer_len - cursor;
      } else {
        pre_text = buffer;
        pre_len = buffer_len;
      }
    }
    if (!pre_text.empty()) {
      commands::Preedit::Segment *segment =
          output->mutable_preedit()->add_segment();
      segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
      segment->set_value(pre_text);
      segment->set_value_length(pre_len);
    }
    if (zuin_len > 0) {
      commands::Preedit::Segment *segment =
          output->mutable_preedit()->add_segment();
      segment->set_annotation(commands::Preedit::Segment::HIGHLIGHT);
      segment->set_value(zuin);
      segment->set_value_length(zuin_len);
    }
    if (!trailing_text.empty()) {
      commands::Preedit::Segment *segment =
          output->mutable_preedit()->add_segment();
      segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
      segment->set_value(trailing_text);
      segment->set_value_length(trailing_len);
    }
    if (output->has_preedit() && output->preedit().segment_size() > 0) {
      output->mutable_preedit()->set_cursor(cursor);
    }
  }

  if (output->preedit().segment_size() == 0) {
    state_ = PRECOMPOSITION;
  } else {
    state_ = IN_CONVERSION;
  }

  // Fill the candidates
  // TODO(mukai): Fill the all_candidates too.
  if (!::chewing_cand_CheckDone(context_)) {
    int total_candidates = ::chewing_cand_TotalChoice(context_);
    if (total_candidates > 0) {
      FillCandidates(output->mutable_candidates());
      // Set the cursor here.
      output->mutable_candidates()->set_position(cursor);
    }
  }

  commands::CompositionMode new_mode = commands::NUM_OF_COMPOSITIONS;
  if (::chewing_get_ChiEngMode(context_) == CHINESE_MODE) {
    // Currently we use HIRAGANA for the chewing input but it's not the ideal.
    // TODO(mukai): use a CHEWING mode when we add it.
    new_mode = commands::HIRAGANA;
  } else {
    // English mode: check the full/half width
    if (::chewing_get_ShapeMode(context_) == FULLSHAPE_MODE) {
      new_mode = commands::FULL_ASCII;
    } else {
      new_mode = commands::HALF_ASCII;
    }
  }
  if (new_mode != commands::NUM_OF_COMPOSITIONS) {
    output->mutable_status()->set_mode(new_mode);
    output->mutable_status()->set_activated(true);
  }

  DLOG(INFO) << command->DebugString();
}

bool Session::SendKey(commands::Command *command) {
  last_command_time_ = Util::GetTime();
  if (g_last_config_updated > last_config_updated_) {
    ResetConfig();
  }

  // Check the modifier keys at first.
  const KeyEvent &key_event = command->input().key();
  bool status_updated = false;

  const uint32 modifiers = KeyEventUtil::GetModifiers(key_event);

  if (KeyEventUtil::IsShift(modifiers)) {
    if (key_event.has_special_key()) {
      if (key_event.special_key() == KeyEvent::LEFT) {
        ::chewing_handle_ShiftLeft(context_);
        status_updated = true;
      } else if (key_event.special_key() == KeyEvent::RIGHT) {
        ::chewing_handle_ShiftRight(context_);
        status_updated = true;
      } else if (key_event.special_key() == KeyEvent::SPACE) {
        ::chewing_handle_ShiftSpace(context_);
        status_updated = true;
      }
    }
  } else if (KeyEventUtil::IsCtrl(modifiers)) {
    // CtrlNum is the event just for numeric keys at the top of
    // keyboard, not for the tenkeys.
    if ('0' <= key_event.key_code() && key_event.key_code() <= '9') {
      ::chewing_handle_CtrlNum(context_, key_event.key_code());
      status_updated = true;
    }
  } else {
    // normal key event.
    if (key_event.has_special_key()) {
      // TODO(mukai): write a script to generate following clauses.
      switch (key_event.special_key()) {
        case KeyEvent::SPACE:
          ::chewing_handle_Space(context_);
          status_updated = true;
          break;
        case KeyEvent::ESCAPE:
          ::chewing_handle_Esc(context_);
          status_updated = true;
          break;
        case KeyEvent::ENTER:
          if (!::chewing_cand_CheckDone(context_) &&
              ::chewing_cand_TotalChoice(context_) > 0) {
            // Special hack: if candidate window pops up, Enter-key
            // means the commit of the first candidate instead of
            // doing nothing.
            // other implementations behaviors:
            //   ibus-chewing: do nothing
            //   Mac Zhuyin: select candidate, not commit
            // The current code is same as Mac Zhuyin behavior.
            // TODO(mukai): verify the correct behavior.
            ::chewing_handle_Default(context_, '1');
          } else {
            ::chewing_handle_Enter(context_);
          }
          status_updated = true;
          break;
        case KeyEvent::DEL:
          ::chewing_handle_Del(context_);
          status_updated = true;
          break;
        case KeyEvent::BACKSPACE:
          ::chewing_handle_Backspace(context_);
          status_updated = true;
          break;
        case KeyEvent::TAB:
          ::chewing_handle_Tab(context_);
          status_updated = true;
          break;
        case KeyEvent::LEFT:
          ::chewing_handle_Left(context_);
          status_updated = true;
          break;
        case KeyEvent::RIGHT:
          ::chewing_handle_Right(context_);
          status_updated = true;
          break;
        case KeyEvent::UP:
          ::chewing_handle_Up(context_);
          status_updated = true;
          break;
        case KeyEvent::HOME:
          ::chewing_handle_Home(context_);
          status_updated = true;
          break;
        case KeyEvent::END:
          ::chewing_handle_End(context_);
          status_updated = true;
          break;
        case KeyEvent::PAGE_UP:
          ::chewing_handle_PageUp(context_);
          status_updated = true;
          break;
        case KeyEvent::PAGE_DOWN:
          ::chewing_handle_PageDown(context_);
          status_updated = true;
          break;
        case KeyEvent::CAPS_LOCK:
          ::chewing_handle_Capslock(context_);
          status_updated = true;
          break;
        case KeyEvent::NUMPAD0:
        case KeyEvent::NUMPAD1:
        case KeyEvent::NUMPAD2:
        case KeyEvent::NUMPAD3:
        case KeyEvent::NUMPAD4:
        case KeyEvent::NUMPAD5:
        case KeyEvent::NUMPAD6:
        case KeyEvent::NUMPAD7:
        case KeyEvent::NUMPAD8:
        case KeyEvent::NUMPAD9:
          ::chewing_handle_Numlock(
              context_, '0' + key_event.special_key() - KeyEvent::NUMPAD0);
          status_updated = true;
          break;
        default:
          // do nothing
          // Currently we don't handle DblTab.
          break;
      }
    } else if (key_event.modifier_keys_size() == 0) {
      ::chewing_handle_Default(context_, key_event.key_code());
      status_updated = true;
    }
  }

  command->mutable_output()->set_consumed(
      status_updated && !::chewing_keystroke_CheckIgnore(context_));
  FillOutput(command);
  return true;
}

bool Session::TestSendKey(commands::Command *command) {
  // TODO(mukai): implement this.
  last_command_time_ = Util::GetTime();
  command->mutable_output()->set_consumed(true);
  FillOutput(command);
  return true;
}

bool Session::SendCommand(commands::Command *command) {
  last_command_time_ = Util::GetTime();
  if (g_last_config_updated > last_config_updated_) {
    ResetConfig();
  }

  const commands::SessionCommand &session_command = command->input().command();
  bool consumed = false;
  switch (session_command.type()) {
    case commands::SessionCommand::REVERT:
      RenewContext();
      consumed = true;
      break;
    case commands::SessionCommand::SUBMIT:
      // TODO(mukai): think about the key customization.
      ::chewing_handle_Enter(context_);
      consumed = true;
      break;
    case commands::SessionCommand::SWITCH_INPUT_MODE:
      switch (session_command.composition_mode()) {
        case commands::HIRAGANA:
          ::chewing_set_ChiEngMode(context_, CHINESE_MODE);
          consumed = true;
          break;
        case commands::FULL_ASCII:
          ::chewing_set_ChiEngMode(context_, SYMBOL_MODE);
          ::chewing_set_ShapeMode(context_, FULLSHAPE_MODE);
          consumed = true;
          break;
        case commands::HALF_ASCII:
          ::chewing_set_ChiEngMode(context_, SYMBOL_MODE);
          ::chewing_set_ShapeMode(context_, HALFSHAPE_MODE);
          consumed = true;
          break;
        default:
          // do nothing
          break;
      }
      break;
    case commands::SessionCommand::SELECT_CANDIDATE:
      {
        commands::Candidates candidates;
        FillCandidates(&candidates);
        for (size_t i = 0; i < candidates.candidate_size(); ++i) {
          const commands::Candidates::Candidate &candidate =
              candidates.candidate(i);
          if (candidate.id() == session_command.id() &&
              candidate.annotation().has_shortcut()) {
            ::chewing_handle_Default(
                context_, candidate.annotation().shortcut()[0]);
            consumed = true;
          }
        }
      }
      break;
    case commands::SessionCommand::GET_STATUS:
      // Do nothing here.
      consumed = true;
      break;
    default:
      // do nothing
      // Following commands are ignored:
      //  HIGHLIGHT_CANDIDATE, SWITCH_INPUT_MODE,
      //  SELECT_CANDIDATE_AND_FORWARD, CONVERT_REVERSE, UNDO.
      break;
  }

  command->mutable_output()->set_consumed(consumed);
  FillOutput(command);
  return true;
}

void Session::ReloadConfig() {
  last_command_time_ = Util::GetTime();
  ResetConfig();
}

void Session::set_client_capability(const commands::Capability &capability) {
  // Do nothing.  Capability does not make sense with the current chewing.
}

void Session::set_application_info(
    const commands::ApplicationInfo &application_info) {
  application_info_.CopyFrom(application_info);
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

#ifdef OS_CHROMEOS
void Session::UpdateConfig(const config::ChewingConfig &config) {
  config::Config mozc_config;
  mozc_config.mutable_chewing_config()->MergeFrom(config);
  config::ConfigHandler::SetConfig(mozc_config);
  g_last_config_updated = Util::GetTime();
}
#endif

}  // namespace chewing
}  // namespace mozc

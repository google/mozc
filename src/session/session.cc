// Copyright 2010, Google Inc.
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

// Session class of Mozc server.

#include "session/session.h"

#include <sstream>

#include "base/base.h"
#include "base/crash_report_util.h"
#include "base/port.h"
#include "base/process.h"
#include "base/url.h"
#include "base/util.h"
#include "base/version.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "session/commands.pb.h"
#include "session/config_handler.h"
#include "session/config.pb.h"
#include "session/session_converter_interface.h"
#include "session/internal/keymap-inl.h"
#include "session/internal/session_normalizer.h"
#include "session/internal/session_output.h"

#include "session/session_converter.h"

namespace mozc {

namespace {
// Numpad keys are transformed to normal characters.
static const struct NumpadTransformTable {
  const commands::KeyEvent::SpecialKey key;
  const char code;
  const char *halfwidth_key_string;
  const char *fullwidth_key_string;
} kTransformTable[] = {
  // "０"
  { commands::KeyEvent::NUMPAD0,  '0', "0", "\xef\xbc\x90"},
  // "１"
  { commands::KeyEvent::NUMPAD1,  '1', "1", "\xef\xbc\x91"},
  // "２"
  { commands::KeyEvent::NUMPAD2,  '2', "2", "\xef\xbc\x92"},
  // "３"
  { commands::KeyEvent::NUMPAD3,  '3', "3", "\xef\xbc\x93"},
  // "４"
  { commands::KeyEvent::NUMPAD4,  '4', "4", "\xef\xbc\x94"},
  // "５"
  { commands::KeyEvent::NUMPAD5,  '5', "5", "\xef\xbc\x95"},
  // "６"
  { commands::KeyEvent::NUMPAD6,  '6', "6", "\xef\xbc\x96"},
  // "７"
  { commands::KeyEvent::NUMPAD7,  '7', "7", "\xef\xbc\x97"},
  // "８"
  { commands::KeyEvent::NUMPAD8,  '8', "8", "\xef\xbc\x98"},
  // "９"
  { commands::KeyEvent::NUMPAD9,  '9', "9", "\xef\xbc\x99"},
  // "＊"
  { commands::KeyEvent::MULTIPLY, '*', "*", "\xef\xbc\x8a"},
  // "＋"
  { commands::KeyEvent::ADD,      '+', "+", "\xef\xbc\x8b"},
  // "−"
  { commands::KeyEvent::SUBTRACT, '-', "-", "\xe2\x88\x92"},
  // "．"
  { commands::KeyEvent::DECIMAL,  '.', ".", "\xef\xbc\x8e"},
  // "／"
  { commands::KeyEvent::DIVIDE,   '/', "/", "\xef\xbc\x8f"},
  // "＝"
  { commands::KeyEvent::EQUALS,   '=', "=", "\xef\xbc\x9d"},
};

// Transform the key event base on the rule.  This function is used
// for special treatment with numpad keys.
bool TransformKeyEventForNumpad(commands::KeyEvent *key_event) {
  if (!key_event->has_special_key()) {
    return false;
  }
  const commands::KeyEvent::SpecialKey special_key = key_event->special_key();

  // commands::KeyEvent::SEPARATOR is transformed to Enter.
  if (special_key == commands::KeyEvent::SEPARATOR) {
    key_event->set_special_key(commands::KeyEvent::ENTER);
    return true;
  }

  for (size_t i = 0; i < arraysize(kTransformTable); ++i) {
    if (special_key == kTransformTable[i].key) {
      key_event->clear_special_key();
      key_event->set_key_code(static_cast<uint32>(kTransformTable[i].code));
      switch (GET_CONFIG(numpad_character_form)) {
        case config::Config::NUMPAD_INPUT_MODE:
          key_event->set_key_string(kTransformTable[i].fullwidth_key_string);
          key_event->set_input_style(commands::KeyEvent::FOLLOW_MODE);
          break;
        case config::Config::NUMPAD_FULL_WIDTH:
          key_event->set_key_string(kTransformTable[i].fullwidth_key_string);
          key_event->set_input_style(commands::KeyEvent::AS_IS);
          break;
        case config::Config::NUMPAD_HALF_WIDTH:
          key_event->set_key_string(kTransformTable[i].halfwidth_key_string);
          key_event->set_input_style(commands::KeyEvent::AS_IS);
          break;
        case config::Config::NUMPAD_DIRECT_INPUT:
          key_event->set_key_string(kTransformTable[i].halfwidth_key_string);
          key_event->set_input_style(commands::KeyEvent::DIRECT_INPUT);
          break;
        default:
          LOG(ERROR) << "Unknown numpad character form value.";
          // Use the same behavior with NUMPAD_HALF_WIDTH as a fallback.
          key_event->set_key_string(kTransformTable[i].halfwidth_key_string);
          key_event->set_input_style(commands::KeyEvent::AS_IS);
          break;
      }
      return true;
    }
  }
  return false;
}

// Transform symbols for Kana input.  Character transformation for
// Romanji input is performed in preedit/table.cc
bool TransformKeyEventForKana(const TransformTable &table,
                              commands::KeyEvent *key_event) {
  if (!key_event->has_key_string()) {
    return false;
  }
  if (key_event->modifier_keys_size() > 0) {
    return false;
  }
  if (key_event->has_modifiers() && key_event->modifiers() != 0) {
    return false;
  }

  const TransformTable::const_iterator it = table.find(key_event->key_string());
  if (it == table.end()) {
    return false;
  }

  key_event->CopyFrom(it->second);
  return true;
}

bool TransformKeyEvent(const TransformTable &table,
                       commands::KeyEvent *key_event) {
  if (key_event == NULL) {
    LOG(ERROR) << "key_event is NULL";
    return false;
  }
  if (TransformKeyEventForNumpad(key_event)) {
    return true;
  }
  if (TransformKeyEventForKana(table, key_event)) {
    return true;
  }
  return false;
}

void InitTransformTable(const config::Config &config, TransformTable *table) {
  if (table == NULL) {
    LOG(ERROR) << "table is NULL";
    return;
  }
  table->clear();
  const config::Config::PunctuationMethod punctuation =
      config.punctuation_method();
  if (punctuation == config::Config::COMMA_PERIOD ||
      punctuation == config::Config::COMMA_TOUTEN) {
    commands::KeyEvent key_event;
    key_event.set_key_code(static_cast<uint32>(','));
    // "，"
    key_event.set_key_string("\xef\xbc\x8c");
    // "、"
    table->insert(make_pair("\xe3\x80\x81", key_event));
  }
  if (punctuation == config::Config::COMMA_PERIOD ||
      punctuation == config::Config::KUTEN_PERIOD) {
    commands::KeyEvent key_event;
    key_event.set_key_code(static_cast<uint32>('.'));
    // "．"
    key_event.set_key_string("\xef\xbc\x8e");
    // "。"
    table->insert(make_pair("\xe3\x80\x82", key_event));
  }

  const config::Config::SymbolMethod symbol = config.symbol_method();
  if (symbol == config::Config::SQUARE_BRACKET_SLASH ||
      symbol == config::Config::SQUARE_BRACKET_MIDDLE_DOT) {
    {
      commands::KeyEvent key_event;
      key_event.set_key_code(static_cast<uint32>('['));
      // "［"
      key_event.set_key_string("\xef\xbc\xbb");
      // "「"
      table->insert(make_pair("\xe3\x80\x8c", key_event));
    }
    {
      commands::KeyEvent key_event;
      key_event.set_key_code(static_cast<uint32>(']'));
      // "］"
      key_event.set_key_string("\xef\xbc\xbd");
      // "」"
      table->insert(make_pair("\xe3\x80\x8d", key_event));
    }
  }
  if (symbol == config::Config::SQUARE_BRACKET_SLASH ||
      symbol == config::Config::CORNER_BRACKET_SLASH) {
    commands::KeyEvent key_event;
    key_event.set_key_code(static_cast<uint32>('/'));
    // "／"
    key_event.set_key_string("\xef\xbc\x8f");
    // "・"
    table->insert(make_pair("\xE3\x83\xBB", key_event));
  }
}
}  // namespace

Session::Session(const composer::Table *table,
                 const ConverterInterface *converter,
                 const keymap::KeyMapManager *keymap)
    : create_session_time_(Util::GetTime()),
      last_command_time_(0),
      composer_(composer::Composer::Create(table)),
      converter_(new session::SessionConverter(converter)),
      keymap_(keymap),
#ifdef OS_WINDOWS
      // On Windows session is started with direct mode.
      // TODO(toshiyuki): Ditto for Mac after verifying on Mac.
      state_(SessionState::DIRECT)
#else
      state_(SessionState::PRECOMPOSITION)
#endif
{
  ReloadConfig();
}

Session::~Session() {}

void Session::EnsureIMEIsOn() {
  if (state_ == SessionState::DIRECT) {
    state_ = SessionState::PRECOMPOSITION;
  }
}

bool Session::SendCommand(commands::Command *command) {
  UpdateTime();
  UpdatePreferences(command);
  if (!command->input().has_command()) {
    return false;
  }
  if (command->input().has_key()) {
    TransformKeyEvent(transform_table_,
                      command->mutable_input()->mutable_key());
  }
  const commands::SessionCommand &session_command = command->input().command();

  if (session_command.type() == commands::SessionCommand::SWITCH_INPUT_MODE) {
    if (!session_command.has_composition_mode()) {
      return false;
    }
    switch (session_command.composition_mode()) {
      case commands::DIRECT:
        // TODO(komatsu): Implement here.
        return false;
      case commands::HIRAGANA:
        return InputModeHiragana(command);
      case commands::FULL_KATAKANA:
        return InputModeFullKatakana(command);
      case commands::HALF_ASCII:
        return InputModeHalfASCII(command);
      case commands::FULL_ASCII:
        return InputModeFullASCII(command);
      case commands::HALF_KATAKANA:
        return InputModeHalfKatakana(command);
      default:
        LOG(ERROR) << "Unknown mode: " << session_command.composition_mode();
        return false;
    }
    DCHECK(false) << "Should not come here.";
  }

  bool result = false;
  switch (command->input().command().type()) {
    case commands::SessionCommand::REVERT:
      result = Revert(command);
      break;
    case commands::SessionCommand::SUBMIT:
      result = Commit(command);
      break;
    case commands::SessionCommand::SELECT_CANDIDATE:
      result = SelectCandidate(command);
      break;
    case commands::SessionCommand::HIGHLIGHT_CANDIDATE:
      result = HighlightCandidate(command);
      break;
    case commands::SessionCommand::GET_STATUS:
      result = GetStatus(command);
      break;
    default:
      LOG(WARNING) << "Unkown command" << command->DebugString();
      result = DoNothing(command);
      break;
  }

  return result;
}

bool Session::TestSendKey(commands::Command *command) {
  UpdateTime();
  UpdatePreferences(command);
  if (!keymap_) {
    return false;
  }
  if (command->input().has_key()) {
    TransformKeyEvent(transform_table_,
                      command->mutable_input()->mutable_key());
  }

  if (state_ == SessionState::NONE) {
    // This must be an error.
    LOG(ERROR) << "Invalid state: NONE";
    return false;
  }

  // Direct input
  if (state_ == SessionState::DIRECT) {
    keymap::DirectInputState::Commands key_command;
    if (!keymap_->GetCommandDirect(command->input().key(), &key_command) ||
        key_command == keymap::DirectInputState::NONE) {
      return EchoBack(command);
    }

    if ((key_command == keymap::DirectInputState::INSERT_SPACE ||
         key_command == keymap::DirectInputState::INSERT_ALTERNATE_SPACE) &&
        !IsFullWidthInsertSpace()) {
      return EchoBack(command);
    }
    return DoNothing(command);
  }

  // Precomposition
  if (state_ == SessionState::PRECOMPOSITION) {
    keymap::PrecompositionState::Commands key_command;
    if (!keymap_->GetCommandPrecomposition(command->input().key(),
                                           &key_command) ||
        key_command == keymap::PrecompositionState::NONE) {
      return EchoBack(command);
    }

    // If the input_style is DIRECT_INPUT, KeyEvent is not consumed
    // and done echo back.  It works only when key_string is equal to
    // key_code.  We should fix this limitation when the as_is flag is
    // used for rather than numpad characters.
    if (key_command == keymap::PrecompositionState::INSERT_CHARACTER &&
        command->input().key().input_style() ==
        commands::KeyEvent::DIRECT_INPUT) {
      return EchoBack(command);
    }

    // TODO(komatsu): This is a hack to work around the problem with
    // the inconsistency between TestSendKey and SendKey.
    if (key_command == keymap::PrecompositionState::INSERT_SPACE &&
        !IsFullWidthInsertSpace()) {
      return EchoBack(command);
    }
    if (key_command == keymap::PrecompositionState::INSERT_ALTERNATE_SPACE &&
        IsFullWidthInsertSpace()) {
      return EchoBack(command);
    }

    if (key_command == keymap::PrecompositionState::REVERT) {
      return Revert(command);
    }
    return DoNothing(command);
  }

  // Do nothing.
  return DoNothing(command);
}

bool Session::SendKey(commands::Command *command) {
  UpdateTime();
  UpdatePreferences(command);
  if (!keymap_) {
    return false;
  }
  if (command->input().has_key()) {
    TransformKeyEvent(transform_table_,
                      command->mutable_input()->mutable_key());
  }

  bool result = false;
  switch (state_) {
    case SessionState::DIRECT:
      result = SendKeyDirectInputState(command);
      break;

    case SessionState::PRECOMPOSITION:
      result = SendKeyPrecompositionState(command);
      break;

    case SessionState::COMPOSITION:
      result = SendKeyCompositionState(command);
      break;

    case SessionState::CONVERSION:
      result = SendKeyConversionState(command);
      break;

    case SessionState::NONE:
      result = false;
      break;
  }

  return result;
}

bool Session::SendKeyDirectInputState(commands::Command *command) {
  keymap::DirectInputState::Commands key_command;
  if (!keymap_->GetCommandDirect(command->input().key(), &key_command)) {
    return EchoBack(command);
  }
  switch (key_command) {
    case keymap::DirectInputState::IME_ON:
      return IMEOn(command);
    case keymap::DirectInputState::INSERT_SPACE:
      return InsertSpace(command);
    case keymap::DirectInputState::INSERT_ALTERNATE_SPACE:
      return InsertSpaceToggled(command);
    case keymap::DirectInputState::INPUT_MODE_HIRAGANA:
      return InputModeHiragana(command);
    case keymap::DirectInputState::INPUT_MODE_FULL_KATAKANA:
      return InputModeFullKatakana(command);
    case keymap::DirectInputState::INPUT_MODE_HALF_KATAKANA:
      return InputModeHalfKatakana(command);
    case keymap::DirectInputState::INPUT_MODE_FULL_ALPHANUMERIC:
      return InputModeFullASCII(command);
    case keymap::DirectInputState::INPUT_MODE_HALF_ALPHANUMERIC:
      return InputModeHalfASCII(command);
    case keymap::DirectInputState::NONE:
      return EchoBack(command);
  }
  return false;
}

bool Session::SendKeyPrecompositionState(commands::Command *command) {
  keymap::PrecompositionState::Commands key_command;
  if (!keymap_->GetCommandPrecomposition(command->input().key(),
                                         &key_command)) {
    return EchoBack(command);
  }
  switch (key_command) {
    case keymap::PrecompositionState::INSERT_CHARACTER:
      return InsertCharacter(command);
    case keymap::PrecompositionState::INSERT_SPACE:
      return InsertSpace(command);
    case keymap::PrecompositionState::INSERT_ALTERNATE_SPACE:
      return InsertSpaceToggled(command);
    case keymap::PrecompositionState::INSERT_HALF_SPACE:
      return InsertSpaceHalfWidth(command);
    case keymap::PrecompositionState::INSERT_FULL_SPACE:
      return InsertSpaceFullWidth(command);
    case keymap::PrecompositionState::TOGGLE_ALPHANUMERIC_MODE:
      return ToggleAlphanumericMode(command);
    case keymap::PrecompositionState::REVERT:
      return Revert(command);
    case keymap::PrecompositionState::UNDO:
      return EchoBack(command);   // not implemented yet
    case keymap::PrecompositionState::IME_OFF:
      return IMEOff(command);
    case keymap::PrecompositionState::IME_ON:
      return DoNothing(command);

    case keymap::PrecompositionState::INPUT_MODE_HIRAGANA:
      return InputModeHiragana(command);
    case keymap::PrecompositionState::INPUT_MODE_FULL_KATAKANA:
      return InputModeFullKatakana(command);
    case keymap::PrecompositionState::INPUT_MODE_HALF_KATAKANA:
      return InputModeHalfKatakana(command);
    case keymap::PrecompositionState::INPUT_MODE_FULL_ALPHANUMERIC:
      return InputModeFullASCII(command);
    case keymap::PrecompositionState::INPUT_MODE_HALF_ALPHANUMERIC:
      return InputModeHalfASCII(command);

    case keymap::PrecompositionState::ABORT:
      return Abort(command);
    case keymap::PrecompositionState::NONE:
      return EchoBack(command);
  }
  return false;
}

bool Session::SendKeyCompositionState(commands::Command *command) {
  keymap::CompositionState::Commands key_command;
  const bool result =
    converter_->CheckState(session::SessionConverterInterface::SUGGESTION) ?
    keymap_->GetCommandSuggestion(command->input().key(), &key_command) :
    keymap_->GetCommandComposition(command->input().key(), &key_command);

  if (!result) {
    return DoNothing(command);
  }

  switch (key_command) {
    case keymap::CompositionState::INSERT_CHARACTER:
      return InsertCharacter(command);

    case keymap::CompositionState::COMMIT:
      return Commit(command);

    case keymap::CompositionState::COMMIT_FIRST_SUGGESTION:
      return CommitFirstSuggestion(command);

    case keymap::CompositionState::CONVERT:
      return Convert(command);

    case keymap::CompositionState::CONVERT_WITHOUT_HISTORY:
      return ConvertWithoutHistory(command);

    case keymap::CompositionState::PREDICT_AND_CONVERT:
      return PredictAndConvert(command);

    case keymap::CompositionState::DEL:
      return Delete(command);

    case keymap::CompositionState::BACKSPACE:
      return Backspace(command);

    case keymap::CompositionState::INSERT_HALF_SPACE:
      return InsertSpaceHalfWidth(command);

    case keymap::CompositionState::INSERT_FULL_SPACE:
      return InsertSpaceFullWidth(command);

    case keymap::CompositionState::MOVE_CURSOR_LEFT:
      return MoveCursorLeft(command);

    case keymap::CompositionState::MOVE_CURSOR_RIGHT:
      return MoveCursorRight(command);

    case keymap::CompositionState::MOVE_CURSOR_TO_BEGINNING:
      return MoveCursorToBeginning(command);

    case keymap::CompositionState::MOVE_MOVE_CURSOR_TO_END:
      return MoveCursorToEnd(command);

    case keymap::CompositionState::CANCEL:
      return EditCancel(command);

    case keymap::CompositionState::IME_OFF:
      return IMEOff(command);

    case keymap::CompositionState::IME_ON:
      return DoNothing(command);

    case keymap::CompositionState::CONVERT_TO_HIRAGANA:
      return ConvertToHiragana(command);

    case keymap::CompositionState::CONVERT_TO_FULL_KATAKANA:
      return ConvertToFullKatakana(command);

    case keymap::CompositionState::CONVERT_TO_HALF_KATAKANA:
      return ConvertToHalfKatakana(command);

    case keymap::CompositionState::CONVERT_TO_HALF_WIDTH:
      return ConvertToHalfWidth(command);

    case keymap::CompositionState::CONVERT_TO_FULL_ALPHANUMERIC:
      return ConvertToFullASCII(command);

    case keymap::CompositionState::CONVERT_TO_HALF_ALPHANUMERIC:
      return ConvertToHalfASCII(command);

    case keymap::CompositionState::SWITCH_KANA_TYPE:
      return SwitchKanaType(command);

    case keymap::CompositionState::DISPLAY_AS_HIRAGANA:
      return DisplayAsHiragana(command);

    case keymap::CompositionState::DISPLAY_AS_FULL_KATAKANA:
      return DisplayAsFullKatakana(command);

    case keymap::CompositionState::DISPLAY_AS_HALF_KATAKANA:
      return DisplayAsHalfKatakana(command);

    case keymap::CompositionState::TRANSLATE_HALF_WIDTH:
      return TranslateHalfWidth(command);

    case keymap::CompositionState::TRANSLATE_FULL_ASCII:
      return TranslateFullASCII(command);

    case keymap::CompositionState::TRANSLATE_HALF_ASCII:
      return TranslateHalfASCII(command);

    case keymap::CompositionState::TOGGLE_ALPHANUMERIC_MODE:
      return ToggleAlphanumericMode(command);

    case keymap::CompositionState::INPUT_MODE_HIRAGANA:
      return InputModeHiragana(command);

    case keymap::CompositionState::INPUT_MODE_FULL_KATAKANA:
      return InputModeFullKatakana(command);

    case keymap::CompositionState::INPUT_MODE_HALF_KATAKANA:
      return InputModeHalfKatakana(command);

    case keymap::CompositionState::INPUT_MODE_FULL_ALPHANUMERIC:
      return InputModeFullASCII(command);

    case keymap::CompositionState::INPUT_MODE_HALF_ALPHANUMERIC:
      return InputModeHalfASCII(command);

    case keymap::CompositionState::ABORT:
      return Abort(command);

    case keymap::CompositionState::NONE:
      return DoNothing(command);
  }
  return false;
}

bool Session::SendKeyConversionState(commands::Command *command) {
  keymap::ConversionState::Commands key_command;
  const bool result =
    converter_->CheckState(session::SessionConverterInterface::PREDICTION) ?
    keymap_->GetCommandPrediction(command->input().key(), &key_command) :
    keymap_->GetCommandConversion(command->input().key(), &key_command);

  if (!result) {
    return DoNothing(command);
  }

  switch (key_command) {
    case keymap::ConversionState::INSERT_CHARACTER:
      return InsertCharacter(command);

    case keymap::ConversionState::INSERT_HALF_SPACE:
      return InsertSpaceHalfWidth(command);

    case keymap::ConversionState::INSERT_FULL_SPACE:
      return InsertSpaceFullWidth(command);

    case keymap::ConversionState::COMMIT:
      return Commit(command);

    case keymap::ConversionState::COMMIT_SEGMENT:
      return CommitSegment(command);

    case keymap::ConversionState::CONVERT_NEXT:
      return ConvertNext(command);

    case keymap::ConversionState::CONVERT_PREV:
      return ConvertPrev(command);

    case keymap::ConversionState::CONVERT_NEXT_PAGE:
      return ConvertNextPage(command);

    case keymap::ConversionState::CONVERT_PREV_PAGE:
      return ConvertPrevPage(command);

    case keymap::ConversionState::PREDICT_AND_CONVERT:
      return PredictAndConvert(command);

    case keymap::ConversionState::SEGMENT_FOCUS_LEFT:
      return SegmentFocusLeft(command);

    case keymap::ConversionState::SEGMENT_FOCUS_RIGHT:
      return SegmentFocusRight(command);

    case keymap::ConversionState::SEGMENT_FOCUS_FIRST:
      return SegmentFocusLeftEdge(command);

    case keymap::ConversionState::SEGMENT_FOCUS_LAST:
      return SegmentFocusLast(command);

    case keymap::ConversionState::SEGMENT_WIDTH_EXPAND:
      return SegmentWidthExpand(command);

    case keymap::ConversionState::SEGMENT_WIDTH_SHRINK:
      return SegmentWidthShrink(command);

    case keymap::ConversionState::CANCEL:
      return ConvertCancel(command);

    case keymap::ConversionState::IME_OFF:
      return IMEOff(command);

    case keymap::ConversionState::IME_ON:
      return DoNothing(command);

    case keymap::ConversionState::CONVERT_TO_HIRAGANA:
      return ConvertToHiragana(command);

    case keymap::ConversionState::CONVERT_TO_FULL_KATAKANA:
      return ConvertToFullKatakana(command);

    case keymap::ConversionState::CONVERT_TO_HALF_KATAKANA:
      return ConvertToHalfKatakana(command);

    case keymap::ConversionState::CONVERT_TO_HALF_WIDTH:
      return ConvertToHalfWidth(command);

    case keymap::ConversionState::CONVERT_TO_FULL_ALPHANUMERIC:
      return ConvertToFullASCII(command);

    case keymap::ConversionState::CONVERT_TO_HALF_ALPHANUMERIC:
      return ConvertToHalfASCII(command);

    case keymap::ConversionState::SWITCH_KANA_TYPE:
      return SwitchKanaType(command);

    case keymap::ConversionState::DISPLAY_AS_HIRAGANA:
      return DisplayAsHiragana(command);

    case keymap::ConversionState::DISPLAY_AS_FULL_KATAKANA:
      return DisplayAsFullKatakana(command);

    case keymap::ConversionState::DISPLAY_AS_HALF_KATAKANA:
      return DisplayAsHalfKatakana(command);

    case keymap::ConversionState::TRANSLATE_HALF_WIDTH:
      return TranslateHalfWidth(command);

    case keymap::ConversionState::TRANSLATE_FULL_ASCII:
      return TranslateFullASCII(command);

    case keymap::ConversionState::TRANSLATE_HALF_ASCII:
      return TranslateHalfASCII(command);

    case keymap::ConversionState::TOGGLE_ALPHANUMERIC_MODE:
      return ToggleAlphanumericMode(command);

    case keymap::ConversionState::INPUT_MODE_HIRAGANA:
      return InputModeHiragana(command);

    case keymap::ConversionState::INPUT_MODE_FULL_KATAKANA:
      return InputModeFullKatakana(command);

    case keymap::ConversionState::INPUT_MODE_HALF_KATAKANA:
      return InputModeHalfKatakana(command);

    case keymap::ConversionState::INPUT_MODE_FULL_ALPHANUMERIC:
      return InputModeFullASCII(command);

    case keymap::ConversionState::INPUT_MODE_HALF_ALPHANUMERIC:
      return InputModeHalfASCII(command);

    case keymap::ConversionState::REPORT_BUG:
      return ReportBug(command);

    case keymap::ConversionState::ABORT:
      return Abort(command);

    case keymap::ConversionState::NONE:
      return DoNothing(command);
  }
  return false;
}

bool Session::UpdatePreferences(commands::Command *command) {
  if (command == NULL ||
      !command->input().has_config()) {
    return false;
  }
  converter_->UpdateConfig(command->input().config());
  return true;
}

bool Session::IMEOn(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  state_ = SessionState::PRECOMPOSITION;
  OutputMode(command);
  return true;
}

bool Session::IMEOff(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  EditCancel(command);
  state_ = SessionState::DIRECT;
  OutputMode(command);
  return true;
}

bool Session::EchoBack(commands::Command *command) {
  command->mutable_output()->set_consumed(false);
  converter_->Reset();
  OutputKey(command);
  return true;
}

bool Session::DoNothing(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (state_ == SessionState::COMPOSITION) {
    OutputComposition(command);
  } else if (state_ == SessionState::CONVERSION) {
    Output(command);
  }
  return true;
}

bool Session::Abort(commands::Command *command) {
#ifdef _DEBUG
  // Abort the server without any finalization.  This is only for
  // debugging.
  command->mutable_output()->set_consumed(true);
  CrashReportUtil::Abort();
  return true;
#else
  return DoNothing(command);
#endif
}

bool Session::Revert(commands::Command *command) {
  if (state_ == SessionState::PRECOMPOSITION) {
    converter_->Revert();
    return EchoBack(command);
  }

  if (!(state_ & (SessionState::COMPOSITION | SessionState::CONVERSION))) {
    return DoNothing(command);
  }

  command->mutable_output()->set_consumed(true);
  if (state_ == SessionState::CONVERSION) {
    converter_->Cancel();
  }

  // The first guard clause in this function guarantees that the current state
  // is COMPOSITION or CONVERSION here.
  // In either case, we should reset the composer.
  composer_->Reset();

  state_ = SessionState::PRECOMPOSITION;
  OutputMode(command);
  return true;
}

void Session::ReloadConfig() {
  const config::Config &config = config::ConfigHandler::GetConfig();
  InitTransformTable(config, &transform_table_);
  composer_->ReloadConfig();
  converter_->ReloadConfig();
}

bool Session::GetStatus(commands::Command *command) {
  OutputMode(command);
  return true;
}

bool Session::SelectCandidate(commands::Command *command) {
  // If the current state is not conversion or composition, the
  // candidate window should not be shown.  (On composition, the
  // window is able to be shown as a suggestion window).
  if (!(state_ & (SessionState::CONVERSION | SessionState::COMPOSITION))) {
    return DoNothing(command);
  }
  if (!command->input().has_command() ||
      !command->input().command().has_id()) {
    LOG(WARNING) << "input.command or input.command.id did not exist.";
    return DoNothing(command);
  }

  command->mutable_output()->set_consumed(true);
  converter_->CandidateMoveToId(command->input().command().id());
  state_ = SessionState::CONVERSION;

  Output(command);
  return true;
}

bool Session::HighlightCandidate(commands::Command *command) {
  if (!SelectCandidate(command)) {
    return false;
  }
  converter_->SetCandidateListVisible(true);
  Output(command);
  return true;
}

bool Session::MaybeSelectCandidate(commands::Command *command) {
  if (state_ != SessionState::CONVERSION) {
    return false;
  }

  // Check if the input character is in the shortcut.
  // TODO(komatsu): Support non ASCII characters such as Unicode and
  // special keys.
  const char shortcut = static_cast<char>(command->input().key().key_code());
  return converter_->CandidateMoveToShortcut(shortcut);
}


bool Session::InsertCharacter(commands::Command *command) {
  // If the input_style is DIRECT_INPUT, KeyEvent is not consumed and
  // done echo back.  It works only when key_string is equal to
  // key_code.  We should fix this limitation when the as_is flag is
  // used for rather than numpad characters.
  if (state_ == SessionState::PRECOMPOSITION &&
      command->input().key().input_style() ==
      commands::KeyEvent::DIRECT_INPUT) {
    command->mutable_output()->set_consumed(false);
    return true;
  }

  command->mutable_output()->set_consumed(true);
  // Handle shortcut keys selecting a candidate from a list.
  if (MaybeSelectCandidate(command)) {
    Output(command);
    return true;
  }

  if (state_ == SessionState::CONVERSION) {
    Commit(command);

    // KeyEvent is specified as DIRECT_INPUT, this KeyEvent should be
    // committed with the conversion result and change the state to
    // PRECOMPOSITION.
    if (command->input().key().input_style() ==
        commands::KeyEvent::DIRECT_INPUT) {
      // NOTE(komatsu): This modification of the result is a little
      // bit hacky.  It might be nice to make a function.
      commands::Result *result = command->mutable_output()->mutable_result();
      DCHECK(result != NULL);
      result->mutable_key()->append(command->input().key().key_string());
      // Append a character representing key_code.
      result->mutable_value()->append(1, command->input().key().key_code());
      state_ = SessionState::PRECOMPOSITION;
      return true;
    }
  }

  if (!command->input().has_key()) {
    LOG(ERROR) << "No key event: " << command->input().DebugString();
    return false;
  }

  composer_->InsertCharacterKeyEvent(command->input().key());

  state_ = SessionState::COMPOSITION;
  if (converter_->Suggest(composer_.get())) {
    DCHECK(converter_->IsActive());
    Output(command);
    return true;
  }

  OutputComposition(command);
  return true;
}

bool Session::IsFullWidthInsertSpace() const {
  // If the current input mode is full-width Ascii, half-width Ascii
  // or half-width Katakana, the width type of space should follow the
  // input mode.
  if (state_ == SessionState::PRECOMPOSITION ||
      state_ == SessionState::COMPOSITION) {
    const transliteration::TransliterationType input_mode =
      composer_->GetInputMode();
    if (transliteration::T13n::IsInFullAsciiTypes(input_mode)) {
      return true;
    } else if (transliteration::T13n::IsInHalfAsciiTypes(input_mode) ||
               transliteration::T13n::IsInHalfKatakanaTypes(input_mode)) {
      return false;
    }
    // Hiragana and Full-width Katakana come here.
  }

  bool is_full_width = false;
  switch (GET_CONFIG(space_character_form)) {
    case config::Config::FUNDAMENTAL_INPUT_MODE:
      is_full_width = (state_ != SessionState::DIRECT);
      break;
    case config::Config::FUNDAMENTAL_FULL_WIDTH:
      is_full_width = true;
      break;
    case config::Config::FUNDAMENTAL_HALF_WIDTH:
      is_full_width = false;
      break;
    default:
      LOG(WARNING) << "Unknown input mode";
      is_full_width = false;
      break;
  }

  return is_full_width;
}

bool Session::InsertSpace(commands::Command *command) {
  converter_->Reset();

  // halfwidth
  if (!IsFullWidthInsertSpace()) {
    return EchoBack(command);
  }

  // fullwidth
  // "\xE3\x80\x80" is full width space
  command->mutable_output()->set_consumed(true);
  session::SessionOutput::FillConversionResult(
      " ", "\xE3\x80\x80", command->mutable_output()->mutable_result());
  OutputMode(command);
  return true;
}

bool Session::InsertSpaceToggled(commands::Command *command) {
  converter_->Reset();

  // Toggle full/halfwidth halfwidth
  if (IsFullWidthInsertSpace()) {
    return EchoBack(command);
  }

  // fullwidth
  // "\xE3\x80\x80" is full width space
  command->mutable_output()->set_consumed(true);
  session::SessionOutput::FillConversionResult(
      " ", "\xE3\x80\x80", command->mutable_output()->mutable_result());
  OutputMode(command);
  return true;
}

bool Session::InsertSpaceHalfWidth(commands::Command *command) {
  if (!(state_ & (SessionState::PRECOMPOSITION |
                  SessionState::COMPOSITION |
                  SessionState::CONVERSION))) {
    return DoNothing(command);
  }

  if (state_ == SessionState::PRECOMPOSITION) {
    return EchoBack(command);
  }

  // state_ == SessionState::COMPOSITION or SessionState::CONVERSION
  command->mutable_input()->clear_key();
  commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
  key_event->set_key_code(' ');
  key_event->set_key_string(" ");
  key_event->set_input_style(commands::KeyEvent::AS_IS);
  return InsertCharacter(command);
}

bool Session::InsertSpaceFullWidth(commands::Command *command) {
  if (!(state_ & (SessionState::PRECOMPOSITION |
                  SessionState::COMPOSITION |
                  SessionState::CONVERSION))) {
    return DoNothing(command);
  }

  if (state_ == SessionState::PRECOMPOSITION) {
    // "\xE3\x80\x80" is full width space
    command->mutable_output()->set_consumed(true);
    session::SessionOutput::FillConversionResult(
        " ", "\xE3\x80\x80", command->mutable_output()->mutable_result());
    OutputMode(command);
    return true;
  }

  // state_ == SessionState::COMPOSITION or SessionState::CONVERSION
  command->mutable_input()->clear_key();
  commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
  key_event->set_key_code(' ');
  key_event->set_key_string("\xE3\x80\x80");
  key_event->set_input_style(commands::KeyEvent::AS_IS);
  return InsertCharacter(command);
}

bool Session::EditCancel(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  composer_->Reset();
  state_ = SessionState::PRECOMPOSITION;
  OutputMode(command);
  return true;
}

bool Session::Commit(commands::Command *command) {
  if (!(state_ & (SessionState::COMPOSITION | SessionState::CONVERSION))) {
    return DoNothing(command);
  }

  command->mutable_output()->set_consumed(true);
  if (state_ == SessionState::COMPOSITION) {
    converter_->CommitPreedit(*composer_);
  } else {  // SessionState::CONVERSION
    converter_->Commit();
  }
  composer_->Reset();
  state_ = SessionState::PRECOMPOSITION;
  Output(command);
  return true;
}

bool Session::CommitFirstSuggestion(commands::Command *command) {
  if (!(state_ == SessionState::COMPOSITION && converter_->IsActive())) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  const int kFirstIndex = 0;
  converter_->CommitSuggestion(kFirstIndex);

  composer_->Reset();
  state_ = SessionState::PRECOMPOSITION;
  Output(command);
  return true;
}

bool Session::CommitSegment(commands::Command *command) {
  if (!(state_ & (SessionState::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  converter_->CommitFirstSegment(composer_.get());
  if (!converter_->IsActive()) {
    // If the converter is not active (ie. the segment size was one.),
    // the state should be switched to precomposition.
    composer_->Reset();
    state_ = SessionState::PRECOMPOSITION;
  }
  Output(command);
  return true;
}

bool Session::ConvertToTransliteration(
    commands::Command *command,
    const transliteration::TransliterationType type) {
  if (!(state_ & (SessionState::CONVERSION | SessionState::COMPOSITION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  if (!converter_->ConvertToTransliteration(composer_.get(), type)) {
    return false;
  }
  state_ = SessionState::CONVERSION;
  Output(command);
  return true;
}

bool Session::ConvertToHiragana(commands::Command *command) {
  return ConvertToTransliteration(command, transliteration::HIRAGANA);
}

bool Session::ConvertToFullKatakana(commands::Command *command) {
  return ConvertToTransliteration(command, transliteration::FULL_KATAKANA);
}

bool Session::ConvertToHalfKatakana(commands::Command *command) {
  return ConvertToTransliteration(command, transliteration::HALF_KATAKANA);
}

bool Session::ConvertToFullASCII(commands::Command *command) {
  return ConvertToTransliteration(command, transliteration::FULL_ASCII);
}

bool Session::ConvertToHalfASCII(commands::Command *command) {
  return ConvertToTransliteration(command, transliteration::HALF_ASCII);
}

bool Session::SwitchKanaType(commands::Command *command) {
  if (!(state_ & (SessionState::CONVERSION | SessionState::COMPOSITION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  if (!converter_->SwitchKanaType(composer_.get())) {
    return false;
  }
  state_ = SessionState::CONVERSION;
  Output(command);
  return true;
}

bool Session::DisplayAsHiragana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (state_ == SessionState::CONVERSION) {
    return ConvertToHiragana(command);
  } else {  // state_ == SessionState::COMPOSITION
    composer_->SetOutputMode(transliteration::HIRAGANA);
    OutputComposition(command);
    return true;
  }
}

bool Session::DisplayAsFullKatakana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (state_ == SessionState::CONVERSION) {
    return ConvertToFullKatakana(command);
  } else {  // state_ == SessionState::COMPOSITION
    composer_->SetOutputMode(transliteration::FULL_KATAKANA);
    OutputComposition(command);
    return true;
  }
}

bool Session::DisplayAsHalfKatakana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (state_ == SessionState::CONVERSION) {
    return ConvertToHalfKatakana(command);
  } else {  // state_ == SessionState::COMPOSITION
    composer_->SetOutputMode(transliteration::HALF_KATAKANA);
    OutputComposition(command);
    return true;
  }
}

bool Session::TranslateFullASCII(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (state_ == SessionState::CONVERSION) {
    return ConvertToFullASCII(command);
  } else {  // state_ == SessionState::COMPOSITION
    composer_->SetOutputMode(transliteration::T13n::ToggleFullAsciiTypes(
                                 composer_->GetOutputMode()));
    OutputComposition(command);
    return true;
  }
}

bool Session::TranslateHalfASCII(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (state_ == SessionState::CONVERSION) {
    return ConvertToHalfASCII(command);
  } else {  // state_ == SessionState::COMPOSITION
    composer_->SetOutputMode(transliteration::T13n::ToggleHalfAsciiTypes(
                                 composer_->GetOutputMode()));
    OutputComposition(command);
    return true;
  }
}

bool Session::InputModeHiragana(commands::Command *command) {
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  if (composer_->GetInputMode() != transliteration::HIRAGANA) {
    composer_->SetInputMode(transliteration::HIRAGANA);
  }
  OutputMode(command);
  return true;
}

bool Session::InputModeFullKatakana(commands::Command *command) {
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  if (composer_->GetInputMode() != transliteration::FULL_KATAKANA) {
    composer_->SetInputMode(transliteration::FULL_KATAKANA);
  }
  OutputMode(command);
  return true;
}

bool Session::InputModeHalfKatakana(commands::Command *command) {
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  if (composer_->GetInputMode() != transliteration::HALF_KATAKANA) {
    composer_->SetInputMode(transliteration::HALF_KATAKANA);
  }
  OutputMode(command);
  return true;
}

bool Session::InputModeFullASCII(commands::Command *command) {
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  if (composer_->GetInputMode() != transliteration::FULL_ASCII) {
    composer_->SetInputMode(transliteration::FULL_ASCII);
  }
  OutputMode(command);
  return true;
}

bool Session::InputModeHalfASCII(commands::Command *command) {
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  if (composer_->GetInputMode() != transliteration::HALF_ASCII) {
    composer_->SetInputMode(transliteration::HALF_ASCII);
  }
  OutputMode(command);
  return true;
}

bool Session::ConvertToHalfWidth(commands::Command *command) {
  if (!(state_ & (SessionState::CONVERSION | SessionState::COMPOSITION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  if (!converter_->ConvertToHalfWidth(composer_.get())) {
    return false;
  }
  state_ = SessionState::CONVERSION;
  Output(command);
  return true;
}

bool Session::TranslateHalfWidth(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (state_ == SessionState::CONVERSION) {
    return ConvertToHalfWidth(command);
  } else {  // state_ == SessionState::COMPOSITION
    const transliteration::TransliterationType type =
      composer_->GetOutputMode();
    if (type == transliteration::HIRAGANA ||
        type == transliteration::FULL_KATAKANA ||
        type == transliteration::HALF_KATAKANA) {
      composer_->SetOutputMode(transliteration::HALF_KATAKANA);
    } else if (type == transliteration::FULL_ASCII) {
      composer_->SetOutputMode(transliteration::HALF_ASCII);
    } else if (type == transliteration::FULL_ASCII_UPPER) {
      composer_->SetOutputMode(transliteration::HALF_ASCII_UPPER);
    } else if (type == transliteration::FULL_ASCII_LOWER) {
      composer_->SetOutputMode(transliteration::HALF_ASCII_LOWER);
    } else if (type == transliteration::FULL_ASCII_CAPITALIZED) {
      composer_->SetOutputMode(transliteration::HALF_ASCII_CAPITALIZED);
    } else {
      // transliteration::HALF_ASCII_something
      return TranslateHalfASCII(command);
    }
    OutputComposition(command);
    return true;
  }
}

bool Session::ToggleAlphanumericMode(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  composer_->ToggleInputMode();

  // TODO(komatsu): These output should be integrated in one function.
  if (state_ == SessionState::PRECOMPOSITION) {
    OutputMode(command);
    return true;
  }

  if (state_ == SessionState::COMPOSITION) {
    OutputComposition(command);
    return true;
  }

  if (state_ == SessionState::CONVERSION) {
    Output(command);
    return true;
  }

  OutputMode(command);
  return true;
}

bool Session::Convert(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  string composition;
  composer_->GetQueryForConversion(&composition);

  // TODO(komatsu): Make a function like ConvertOrSpace.
  // Handle a space key on the ASCII composition mode.
  if (state_ == SessionState::COMPOSITION &&
      (composer_->GetInputMode() == transliteration::HALF_ASCII ||
       composer_->GetInputMode() == transliteration::FULL_ASCII) &&
      command->input().key().has_special_key() &&
      command->input().key().special_key() == commands::KeyEvent::SPACE) {
    if (composition.empty() || composition[composition.size() - 1] != ' ') {
      // If the last character is not space, space is inserted to the
      // composition.
      command->mutable_input()->mutable_key()->set_key_code(' ');
      return InsertCharacter(command);
    }
    if (!composition.empty()) {
      DCHECK(composition[composition.size() - 1] == ' ');
      // Delete the last space.
      composer_->Backspace();
    }
  }

  if (!converter_->Convert(composer_.get())) {
    LOG(ERROR) << "Conversion failed for some reasons.";
    OutputComposition(command);
    return true;
  }

  state_ = SessionState::CONVERSION;
  Output(command);
  return true;
}

bool Session::ConvertWithoutHistory(commands::Command *command) {
  command->mutable_output()->set_consumed(true);

  session::ConversionPreferences preferences =
    converter_->conversion_preferences();
  preferences.use_history = false;
  if (!converter_->ConvertWithPreferences(composer_.get(), preferences)) {
    LOG(ERROR) << "Conversion failed for some reasons.";
    OutputComposition(command);
    return true;
  }

  state_ = SessionState::CONVERSION;
  Output(command);
  return true;
}

bool Session::MoveCursorRight(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  composer_->MoveCursorRight();
  if (converter_->Suggest(composer_.get())) {
    DCHECK(converter_->IsActive());
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::MoveCursorLeft(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  composer_->MoveCursorLeft();
  if (converter_->Suggest(composer_.get())) {
    DCHECK(converter_->IsActive());
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::MoveCursorToEnd(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  composer_->MoveCursorToEnd();
  if (converter_->Suggest(composer_.get())) {
    DCHECK(converter_->IsActive());
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::MoveCursorToBeginning(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  composer_->MoveCursorToBeginning();
  if (converter_->Suggest(composer_.get())) {
    DCHECK(converter_->IsActive());
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::Delete(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  composer_->Delete();
  if (composer_->Empty()) {
    // Input/output modes remain.
    composer_->EditErase();
    state_ = SessionState::PRECOMPOSITION;
    OutputMode(command);
  } else if (converter_->Suggest(composer_.get())) {
    DCHECK(converter_->IsActive());
    Output(command);
    return true;
  } else {
    OutputComposition(command);
  }
  return true;
}

bool Session::Backspace(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  composer_->Backspace();
  if (composer_->Empty()) {
    // Input/output modes remain.
    composer_->EditErase();
    state_ = SessionState::PRECOMPOSITION;
    OutputMode(command);
  } else if (converter_->Suggest(composer_.get())) {
    DCHECK(converter_->IsActive());
    Output(command);
    return true;
  } else {
    OutputComposition(command);
  }
  return true;
}

bool Session::SegmentFocusRight(commands::Command *command) {
  if (!(state_ & (SessionState::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  converter_->SegmentFocusRight();
  Output(command);
  return true;
}

bool Session::SegmentFocusLast(commands::Command *command) {
  if (!(state_ & (SessionState::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  converter_->SegmentFocusLast();
  Output(command);
  return true;
}

bool Session::SegmentFocusLeft(commands::Command *command) {
  if (!(state_ & (SessionState::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  converter_->SegmentFocusLeft();
  Output(command);
  return true;
}

bool Session::SegmentFocusLeftEdge(commands::Command *command) {
  if (!(state_ & (SessionState::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  converter_->SegmentFocusLeftEdge();
  Output(command);
  return true;
}

bool Session::SegmentWidthExpand(commands::Command *command) {
  if (!(state_ & (SessionState::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  converter_->SegmentWidthExpand();
  Output(command);
  return true;
}

bool Session::SegmentWidthShrink(commands::Command *command) {
  if (!(state_ & (SessionState::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  converter_->SegmentWidthShrink();
  Output(command);
  return true;
}

bool Session::ReportBug(commands::Command *command) {
  return DoNothing(command);
}

bool Session::ConvertNext(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  converter_->CandidateNext();
  Output(command);
  return true;
}

bool Session::ConvertNextPage(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  converter_->CandidateNextPage();
  Output(command);
  return true;
}

bool Session::ConvertPrev(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  converter_->CandidatePrev();
  Output(command);
  return true;
}

bool Session::ConvertPrevPage(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  converter_->CandidatePrevPage();
  Output(command);
  return true;
}

bool Session::ConvertCancel(commands::Command *command) {
  command->mutable_output()->set_consumed(true);

  state_ = SessionState::COMPOSITION;
  converter_->Cancel();
  if (converter_->Suggest(composer_.get())) {
    DCHECK(converter_->IsActive());
    Output(command);
  } else {
    OutputComposition(command);
  }
  return true;
}


bool Session::PredictAndConvert(commands::Command *command) {
  if (state_ == SessionState::CONVERSION) {
    return ConvertNext(command);
  }

  command->mutable_output()->set_consumed(true);
  if (converter_->Predict(composer_.get())) {
    state_ = SessionState::CONVERSION;
    Output(command);
  } else {
    OutputComposition(command);
  }
  return true;
}

void Session::Output(commands::Command *command) {
  OutputMode(command);
  converter_->PopOutput(command->mutable_output());
}

void Session::OutputMode(commands::Command *command) const {
  if (state_ == SessionState::DIRECT) {
    command->mutable_output()->set_mode(commands::DIRECT);
  } else if (composer_->GetInputMode() == transliteration::HIRAGANA) {
    command->mutable_output()->set_mode(commands::HIRAGANA);
  } else if (composer_->GetInputMode() == transliteration::FULL_KATAKANA) {
    command->mutable_output()->set_mode(commands::FULL_KATAKANA);
  } else if (composer_->GetInputMode() == transliteration::HALF_KATAKANA) {
    command->mutable_output()->set_mode(commands::HALF_KATAKANA);
  } else if (composer_->GetInputMode() == transliteration::FULL_ASCII) {
    command->mutable_output()->set_mode(commands::FULL_ASCII);
  } else if (composer_->GetInputMode() == transliteration::HALF_ASCII) {
    command->mutable_output()->set_mode(commands::HALF_ASCII);
  }
}

void Session::OutputComposition(commands::Command *command) const {
  OutputMode(command);
  commands::Preedit *preedit = command->mutable_output()->mutable_preedit();
  session::SessionOutput::FillPreedit(*composer_, preedit);
}

void Session::OutputKey(commands::Command *command) const {
  OutputMode(command);
  commands::KeyEvent *key = command->mutable_output()->mutable_key();
  key->CopyFrom(command->input().key());
}

void Session::UpdateTime() {
  last_command_time_ = Util::GetTime();
}
}  // namespace mozc

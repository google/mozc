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

// Session class of Mozc server.

#include "session/session.h"

#include "base/crash_report_util.h"
#include "base/port.h"
#include "base/process.h"
#include "base/singleton.h"
#include "base/url.h"
#include "base/util.h"
#include "base/version.h"
#include "composer/composer.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
// TODO(komatsu): Delete the next line by refactoring of the initializer.
#include "converter/converter_interface.h"
#include "rewriter/calculator/calculator_interface.h"
#include "session/internal/keymap.h"
#include "session/internal/keymap-inl.h"
#include "session/internal/keymap_factory.h"

#include "session/internal/session_output.h"
#include "session/session_converter.h"

namespace mozc {
namespace session {

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

// Logic of nested calculation
// Returns the number of characters to expand preedit to left.
size_t GetCompositionExpansionForCalculator(const string &preceding_text,
                                            const string &composition,
                                            string *expanded_characters) {
  DCHECK(expanded_characters != NULL);

  // Return 0, if last character is neither "=" nor "＝".
  if (composition.empty() ||
      !(Util::EndsWith(composition, "=") ||
        Util::EndsWith(composition, "\xEF\xBC\x9D"))) {
    return 0;
  }

  const CalculatorInterface *calculator = CalculatorFactory::GetCalculator();

  string result;
  const size_t preceding_length = Util::CharsLen(preceding_text);
  size_t expansion = preceding_length;
  while (expansion > 0) {
    const string part_of_preceding =
        Util::SubString(preceding_text,
                        preceding_length - expansion,
                        expansion);
    const string key = part_of_preceding + composition;
    // Skip if the first character is space.
    if (!Util::StartsWith(key, " ") &&
        calculator->CalculateString(key, &result)) {
      *expanded_characters = part_of_preceding;
      break;
    }
    --expansion;
  }
  return expansion;
}

// Set input mode if the current input mode is not the given mode.
void SwitchInputMode(const transliteration::TransliterationType mode,
                     composer::Composer *composer) {
  if (composer->GetInputMode() != mode) {
    composer->SetInputMode(mode);
  }
  composer->SetNewInput();
}

// Return true if the specified key event consists of any modifier key only.
bool IsPureModifierKeyEvent(const commands::KeyEvent &key) {
  if (key.has_key_code()) {
    return false;
  }
  if (key.has_special_key()) {
    return false;
  }
  if (key.modifier_keys_size() == 0) {
    return false;
  }
  return true;
}
}  // namespace

// TODO(komatsu): Remove these argument by using/making singletons.
Session::Session()
    : context_(new ImeContext),
      prev_context_(NULL) {
  InitContext(context_.get());
}

Session::~Session() {}

void Session::InitContext(ImeContext *context) const {
  context->set_create_time(Util::GetTime());
  context->set_last_command_time(0);
  context->set_composer(new composer::Composer);
  context->set_converter(
      new SessionConverter(ConverterFactory::GetConverter()));
#ifdef OS_WINDOWS
  // On Windows session is started with direct mode.
  // FIXME(toshiyuki): Ditto for Mac after verifying on Mac.
  context->set_state(ImeContext::DIRECT);
#else
  context->set_state(ImeContext::PRECOMPOSITION);
#endif

  UpdateConfig(config::ConfigHandler::GetConfig(), context);
}

void Session::SetSessionState(const ImeContext::State state) {
  const ImeContext::State prev_state = context_->state();
  context_->set_state(state);
  if (state == ImeContext::DIRECT ||
      state == ImeContext::PRECOMPOSITION) {
    context_->mutable_composer()->Reset();
  } else if (state == ImeContext::CONVERSION) {
    context_->mutable_composer()->ResetInputMode();
  } else if (state == ImeContext::COMPOSITION) {
    if (prev_state == ImeContext::PRECOMPOSITION) {
      // NOTE: In case of state change including commitment, state change does
      // not happen directly at once from CONVERSION to COMPOSITION. Actual path
      // is CONVERSION to PRECOMPOSITION at first, then PRECOMPOSITION to
      // COMPOSITION. However in this case we can only get one
      // SendCaretRectangle because above state change is executed atomically.
      composition_rectangle_.CopyFrom(caret_rectangle_);
    }
  }
}

void Session::PushUndoContext() {
  // TODO(komatsu): Support multiple undo.
  prev_context_.reset(new ImeContext);
  InitContext(prev_context_.get());
  ImeContext::CopyContext(*context_, prev_context_.get());
}

void Session::PopUndoContext() {
  // TODO(komatsu): Support multiple undo.
  if (!prev_context_.get()) {
    return;
  }
  ImeContext::CopyContext(*prev_context_, context_.get());
  prev_context_.reset(NULL);
}

void Session::ClearUndoContext() {
  prev_context_.reset(NULL);
}

void Session::EnsureIMEIsOn() {
  if (context_->state() == ImeContext::DIRECT) {
    SetSessionState(ImeContext::PRECOMPOSITION);
  }
}

bool Session::SendCommand(commands::Command *command) {
  UpdateTime();
  UpdatePreferences(command);
  if (!command->input().has_command()) {
    return false;
  }
  TransformInput(command->mutable_input());
  const commands::SessionCommand &session_command = command->input().command();

  // TODO(peria): Set usage stats tracker for each command like SendKey()

  bool result = false;
  if (session_command.type() == commands::SessionCommand::SWITCH_INPUT_MODE) {
    if (!session_command.has_composition_mode()) {
      return false;
    }
    switch (session_command.composition_mode()) {
      case commands::DIRECT:
        // TODO(komatsu): Implement here.
        break;
      case commands::HIRAGANA:
        result = InputModeHiragana(command);
        break;
      case commands::FULL_KATAKANA:
        result = InputModeFullKatakana(command);
        break;
      case commands::HALF_ASCII:
        result = InputModeHalfASCII(command);
        break;
      case commands::FULL_ASCII:
        result = InputModeFullASCII(command);
        break;
      case commands::HALF_KATAKANA:
        result = InputModeHalfKatakana(command);
        break;
      default:
        LOG(ERROR) << "Unknown mode: " << session_command.composition_mode();
        break;
    }
    return result;
  }

  DCHECK_EQ(false, result);
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
    case commands::SessionCommand::CONVERT_REVERSE:
      result = ConvertReverse(command);
      break;
    case commands::SessionCommand::UNDO:
      result = Undo(command);
      break;
    case commands::SessionCommand::RESET_CONTEXT:
      result = ResetContext(command);
      break;
    case commands::SessionCommand::MOVE_CURSOR:
      result = MoveCursorTo(command);
      break;
    case commands::SessionCommand::EXPAND_SUGGESTION:
      result = ExpandSuggestion(command);
      break;
    case commands::SessionCommand::SWITCH_INPUT_FIELD_TYPE:
      result = SwitchInputFieldType(command);
      break;
    case commands::SessionCommand::USAGE_STATS_EVENT:
      // Set consumed to false, because the client don't need to do anything
      // when it receive the output from the server.
      command->mutable_output()->set_consumed(false);
      result = true;
      break;
    case commands::SessionCommand::SEND_CARET_LOCATION:
      result = SetCaretLocation(command);
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
  TransformInput(command->mutable_input());

  if (context_->state() == ImeContext::NONE) {
    // This must be an error.
    LOG(ERROR) << "Invalid state: NONE";
    return false;
  }

  const keymap::KeyMapManager *keymap =
      keymap::KeyMapFactory::GetKeyMapManager(context_->keymap());

  // Direct input
  if (context_->state() == ImeContext::DIRECT) {
    keymap::DirectInputState::Commands key_command;
    if (!keymap->GetCommandDirect(command->input().key(), &key_command) ||
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
  if (context_->state() == ImeContext::PRECOMPOSITION) {
    keymap::PrecompositionState::Commands key_command;
    const bool result =
        context_->converter().CheckState(
            SessionConverterInterface::SUGGESTION) ?
        keymap->GetCommandZeroQuerySuggestion(
            command->input().key(), &key_command) :
        keymap->GetCommandPrecomposition(
            command->input().key(), &key_command);
    if (!result || key_command == keymap::PrecompositionState::NONE) {
      // Clear undo context just in case. b/5529702.
      // Note that the undo context will not be cleard in
      // EchoBackAndClearUndoContext if the key event consists of modifier keys
      // only.
      return EchoBackAndClearUndoContext(command);
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

    // If undo context is empty, echoes back the key event so that it can be
    // handled by the application. b/5553298
    if (key_command == keymap::PrecompositionState::UNDO &&
        !prev_context_.get()) {
      return EchoBack(command);
    }

    return DoNothing(command);
  }

  // Do nothing.
  return DoNothing(command);
}

bool Session::SendKey(commands::Command *command) {
  UpdateTime();
  UpdatePreferences(command);
  TransformInput(command->mutable_input());

  bool result = false;
  switch (context_->state()) {
    case ImeContext::DIRECT:
      result = SendKeyDirectInputState(command);
      break;

    case ImeContext::PRECOMPOSITION:
      result = SendKeyPrecompositionState(command);
      break;

    case ImeContext::COMPOSITION:
      result = SendKeyCompositionState(command);
      break;

    case ImeContext::CONVERSION:
      result = SendKeyConversionState(command);
      break;

    case ImeContext::NONE:
      result = false;
      break;
  }

  return result;
}

bool Session::SendKeyDirectInputState(commands::Command *command) {
  keymap::DirectInputState::Commands key_command;
  const keymap::KeyMapManager *keymap =
      keymap::KeyMapFactory::GetKeyMapManager(context_->keymap());
  if (!keymap->GetCommandDirect(command->input().key(), &key_command)) {
    return EchoBackAndClearUndoContext(command);
  }
  string command_name;
  if (keymap->GetNameFromCommandDirect(key_command, &command_name)) {
    const string name = "Direct_" + command_name;
    command->mutable_output()->set_performed_command(name);
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
      return EchoBackAndClearUndoContext(command);
    case keymap::DirectInputState::RECONVERT:
      return RequestConvertReverse(command);
  }
  return false;
}

bool Session::SendKeyPrecompositionState(commands::Command *command) {
  keymap::PrecompositionState::Commands key_command;
  const keymap::KeyMapManager *keymap =
      keymap::KeyMapFactory::GetKeyMapManager(context_->keymap());
  const bool result =
      context_->converter().CheckState(SessionConverterInterface::SUGGESTION) ?
      keymap->GetCommandZeroQuerySuggestion(command->input().key(),
                                            &key_command) :
      keymap->GetCommandPrecomposition(command->input().key(), &key_command);

  if (!result) {
    return EchoBackAndClearUndoContext(command);
  }
  string command_name;
  if (keymap->GetNameFromCommandPrecomposition(key_command, &command_name)) {
    const string name = "Precomposition_" + command_name;
    command->mutable_output()->set_performed_command(name);
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
      return RequestUndo(command);
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
    case keymap::PrecompositionState::INPUT_MODE_SWITCH_KANA_TYPE:
      return InputModeSwitchKanaType(command);

    case keymap::PrecompositionState::LAUNCH_CONFIG_DIALOG:
      return LaunchConfigDialog(command);
    case keymap::PrecompositionState::LAUNCH_DICTIONARY_TOOL:
      return LaunchDictionaryTool(command);
    case keymap::PrecompositionState::LAUNCH_WORD_REGISTER_DIALOG:
      return LaunchWordRegisterDialog(command);

    // For zero query suggestion
    case keymap::PrecompositionState::CANCEL:
      // It is a little kind of abuse of the EditCancel command.  It
      // would be nice to make a new command when EditCancel is
      // extended or the requirement of this command is added.
      return EditCancel(command);
    // For zero query suggestion
    case keymap::PrecompositionState::COMMIT_FIRST_SUGGESTION:
      return CommitFirstSuggestion(command);
    // For zero query suggestion
    case keymap::PrecompositionState::PREDICT_AND_CONVERT:
      return PredictAndConvert(command);

    case keymap::PrecompositionState::ABORT:
      return Abort(command);
    case keymap::PrecompositionState::NONE:
      return EchoBackAndClearUndoContext(command);
    case keymap::PrecompositionState::RECONVERT:
      return RequestConvertReverse(command);
  }
  return false;
}

bool Session::SendKeyCompositionState(commands::Command *command) {
  keymap::CompositionState::Commands key_command;
  const keymap::KeyMapManager *keymap =
      keymap::KeyMapFactory::GetKeyMapManager(context_->keymap());
  const bool result =
      context_->converter().CheckState(SessionConverterInterface::SUGGESTION) ?
      keymap->GetCommandSuggestion(command->input().key(), &key_command) :
      keymap->GetCommandComposition(command->input().key(), &key_command);

  if (!result) {
    return DoNothing(command);
  }
  string command_name;
  if (keymap->GetNameFromCommandComposition(key_command, &command_name)) {
    const string name = "Composition_" + command_name;
    command->mutable_output()->set_performed_command(name);
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

    case keymap::CompositionState::UNDO:
      return RequestUndo(command);

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
  const keymap::KeyMapManager *keymap =
      keymap::KeyMapFactory::GetKeyMapManager(context_->keymap());
  const bool result =
      context_->converter().CheckState(SessionConverterInterface::PREDICTION) ?
      keymap->GetCommandPrediction(command->input().key(), &key_command) :
      keymap->GetCommandConversion(command->input().key(), &key_command);

  if (!result) {
    return DoNothing(command);
  }
  string command_name;
  if (keymap->GetNameFromCommandConversion(key_command,
                                           &command_name)) {
    const string name = "Conversion_" + command_name;
    command->mutable_output()->set_performed_command(name);
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

    case keymap::ConversionState::UNDO:
      return RequestUndo(command);

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

void Session::UpdatePreferences(commands::Command *command) {
  DCHECK(command);

  const config::Config &config = command->input().config();
  if (config.has_session_keymap()) {
    context_->set_keymap(config.session_keymap());
  } else {
    context_->set_keymap(GET_CONFIG(session_keymap));
  }

  if (command->input().has_capability()) {
    context_->mutable_client_capability()->CopyFrom(
        command->input().capability());
  }

  UpdateOperationPreferences(config, context_.get());
}

bool Session::IMEOn(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  ClearUndoContext();

  SetSessionState(ImeContext::PRECOMPOSITION);
  const commands::KeyEvent &key = command->input().key();
  if (key.has_mode()) {
    // Ime on with specified mode.
    switch (key.mode()) {
      case commands::HIRAGANA:
        SwitchInputMode(transliteration::HIRAGANA,
                        context_->mutable_composer());
        break;
      case commands::FULL_KATAKANA:
        SwitchInputMode(transliteration::FULL_KATAKANA,
                        context_->mutable_composer());
        break;
      case commands::HALF_KATAKANA:
        SwitchInputMode(transliteration::HALF_KATAKANA,
                        context_->mutable_composer());
        break;
      case commands::FULL_ASCII:
        SwitchInputMode(transliteration::FULL_ASCII,
                        context_->mutable_composer());
        break;
      case commands::HALF_ASCII:
        SwitchInputMode(transliteration::HALF_ASCII,
                        context_->mutable_composer());
        break;
      default:
        LOG(ERROR) << "ime on with invalid mode";
        DLOG(FATAL);
    }
  }
  OutputMode(command);
  return true;
}

bool Session::IMEOff(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  ClearUndoContext();

  // If you want to cancel composition on IMEOff,
  // call EditCancel() instead of Commit() here.
  // TODO(toshiyuki): Modify here if we have the config.
  //  EditCancel(command);
  Commit(command);

  // Reset the context.
  context_->mutable_converter()->Reset();

  SetSessionState(ImeContext::DIRECT);
  OutputMode(command);
  return true;
}

bool Session::EchoBack(commands::Command *command) {
  command->mutable_output()->set_consumed(false);
  context_->mutable_converter()->Reset();
  OutputKey(command);
  return true;
}

bool Session::EchoBackAndClearUndoContext(commands::Command *command) {
  command->mutable_output()->set_consumed(false);

  // Don't clear undo context when KeyEvent has a modifier key only.
  // TODO(hsumita): A modifier key may be assigned to another functions.
  //                ex) InsertSpace
  //                We need to check it outside of this function.
  const commands::KeyEvent &key_event = command->input().key();
  if (!IsPureModifierKeyEvent(key_event)) {
    ClearUndoContext();
  }

  return EchoBack(command);
}

bool Session::DoNothing(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::PRECOMPOSITION) {
    if (context_->converter().IsActive()) {
      context_->mutable_converter()->Reset();
      Output(command);
    }
  } else if (context_->state() == ImeContext::COMPOSITION) {
    OutputComposition(command);
  } else if (context_->state() == ImeContext::CONVERSION) {
    Output(command);
  }
  return true;
}

bool Session::Abort(commands::Command *command) {
#ifdef _DEBUG
  // Abort the server without any finalization.  This is only for
  // debugging.
  command->mutable_output()->set_consumed(true);
  ClearUndoContext();

  CrashReportUtil::Abort();
  return true;
#else
  return DoNothing(command);
#endif
}

bool Session::Revert(commands::Command *command) {
  if (context_->state() == ImeContext::PRECOMPOSITION) {
    context_->mutable_converter()->Revert();
    return EchoBack(command);
  }

  if (!(context_->state() & (ImeContext::COMPOSITION |
                             ImeContext::CONVERSION))) {
    return DoNothing(command);
  }

  command->mutable_output()->set_consumed(true);
  ClearUndoContext();

  if (context_->state() == ImeContext::CONVERSION) {
    context_->mutable_converter()->Cancel();
  }

  SetSessionState(ImeContext::PRECOMPOSITION);
  OutputMode(command);
  return true;
}

bool Session::ResetContext(commands::Command *command) {
  if (context_->state() == ImeContext::PRECOMPOSITION) {
    context_->mutable_converter()->Reset();
    return EchoBackAndClearUndoContext(command);
  }

  command->mutable_output()->set_consumed(true);
  ClearUndoContext();

  context_->mutable_converter()->Reset();

  SetSessionState(ImeContext::PRECOMPOSITION);
  OutputMode(command);
  return true;
}

void Session::ReloadConfig() {
  UpdateConfig(config::ConfigHandler::GetConfig(), context_.get());
}

// static
void Session::UpdateConfig(const config::Config &config, ImeContext *context) {
  context->set_keymap(config.session_keymap());

  InitTransformTable(config, context->mutable_transform_table());
  context->mutable_composer()->ReloadConfig();
  UpdateOperationPreferences(config, context);
}

// static
void Session::UpdateOperationPreferences(const config::Config &config,
                                         ImeContext *context) {
  OperationPreferences operation_preferences;
  // Keyboard shortcut for candidates.
  const char kShortcut123456789[] = "123456789";
  const char kShortcutASDFGHJKL[] = "asdfghjkl";
  config::Config::SelectionShortcut shortcut;
  if (config.has_selection_shortcut()) {
    shortcut = config.selection_shortcut();
  } else {
    shortcut = GET_CONFIG(selection_shortcut);
  }
  switch (shortcut) {
    case config::Config::SHORTCUT_123456789:
      operation_preferences.candidate_shortcuts = kShortcut123456789;
      break;
    case config::Config::SHORTCUT_ASDFGHJKL:
      operation_preferences.candidate_shortcuts = kShortcutASDFGHJKL;
      break;
    case config::Config::NO_SHORTCUT:
      operation_preferences.candidate_shortcuts.clear();
      break;
    default:
      LOG(WARNING) << "Unkown shortcuts type: "
                   << GET_CONFIG(selection_shortcut);
      break;
  }

  // Cascading Window.
#ifndef OS_LINUX
  if (config.has_use_cascading_window()) {
    operation_preferences.use_cascading_window = config.use_cascading_window();
  }
#endif
  context->mutable_converter()->SetOperationPreferences(operation_preferences);
}

bool Session::GetStatus(commands::Command *command) {
  OutputMode(command);
  return true;
}

bool Session::RequestConvertReverse(commands::Command *command) {
  if (context_->state() != ImeContext::PRECOMPOSITION &&
      context_->state() != ImeContext::DIRECT) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  Output(command);

  // Fill callback message.
  commands::SessionCommand *session_command =
      command->mutable_output()->mutable_callback()->mutable_session_command();
  session_command->set_type(commands::SessionCommand::CONVERT_REVERSE);
  return true;
}

bool Session::ConvertReverse(commands::Command *command) {
  if (context_->state() != ImeContext::PRECOMPOSITION &&
      context_->state() != ImeContext::DIRECT) {
    return DoNothing(command);
  }
  const string &composition = command->input().command().text();
  string reading;
  if (!context_->mutable_converter()->GetReadingText(composition, &reading)) {
    LOG(ERROR) << "Failed to get reading text";
    return DoNothing(command);
  }

  composer::Composer *composer = context_->mutable_composer();
  composer->Reset();
  vector<string> reading_characters;
  // TODO(hsumita): Currently, InsertCharacterPreedit can't deal multiple
  // characters at the same time.
  // So we should split query to UTF-8 chars to avoid DCHECK failure.
  // http://b/3437358
  //
  // See also http://b/5094684, http://b/5094642
  Util::SplitStringToUtf8Chars(reading, &reading_characters);
  for (size_t i = 0; i < reading_characters.size(); ++i) {
    composer->InsertCharacterPreedit(reading_characters[i]);
  }
  composer->set_source_text(composition);
  // start conversion here.
  if (!context_->mutable_converter()->Convert(*composer)) {
    LOG(ERROR) << "Failed to start conversion for reverse conversion";
    return false;
  }

  command->mutable_output()->set_consumed(true);

  SetSessionState(ImeContext::CONVERSION);
  context_->mutable_converter()->SetCandidateListVisible(true);
  Output(command);
  return true;
}

bool Session::RequestUndo(commands::Command *command) {
  if (!(context_->state() & (ImeContext::PRECOMPOSITION |
                             ImeContext::CONVERSION |
                             ImeContext::COMPOSITION))) {
    return DoNothing(command);
  }

  // If undo context is empty, echoes back the key event so that it can be
  // handled by the application. b/5553298
  if (context_->state() == ImeContext::PRECOMPOSITION &&
      !prev_context_.get()) {
    return EchoBack(command);
  }

  command->mutable_output()->set_consumed(true);
  Output(command);

  // Fill callback message.
  commands::SessionCommand *session_command =
      command->mutable_output()->mutable_callback()->mutable_session_command();
  session_command->set_type(commands::SessionCommand::UNDO);
  return true;
}

bool Session::Undo(commands::Command *command) {
  if (!(context_->state() & (ImeContext::PRECOMPOSITION |
                             ImeContext::CONVERSION |
                             ImeContext::COMPOSITION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  // Check the undo context
  if (!prev_context_.get()) {
    return DoNothing(command);
  }

  // Rollback the last user history.
  context_->mutable_converter()->Revert();

  size_t result_size = 0;
  if (context_->output().has_result()) {
    // Check the client's capability
    if (!(context_->client_capability().text_deletion() &
          commands::Capability::DELETE_PRECEDING_TEXT)) {
      return DoNothing(command);
    }
    result_size = Util::CharsLen(context_->output().result().value());
  }

  PopUndoContext();

  if (result_size > 0) {
    commands::DeletionRange *range =
      command->mutable_output()->mutable_deletion_range();
    range->set_offset(-static_cast<int>(result_size));
    range->set_length(result_size);
  }

  Output(command);
  return true;
}

bool Session::SelectCandidateInternal(commands::Command *command) {
  // If the current state is not conversion, composition or
  // precomposition, the candidate window should not be shown.  (On
  // composition or precomposition, the window is able to be shown as
  // a suggestion window).
  if (!(context_->state() & (ImeContext::CONVERSION |
                             ImeContext::COMPOSITION |
                             ImeContext::PRECOMPOSITION))) {
    return false;
  }
  if (!command->input().has_command() ||
      !command->input().command().has_id()) {
    LOG(WARNING) << "input.command or input.command.id did not exist.";
    return false;
  }
  if (!context_->converter().IsActive()) {
    LOG(WARNING) << "converter is not active. (no candidates)";
    return false;
  }

  command->mutable_output()->set_consumed(true);

  context_->mutable_converter()->CandidateMoveToId(
      command->input().command().id(), context_->composer());
  SetSessionState(ImeContext::CONVERSION);

  return true;
}

bool Session::SelectCandidate(commands::Command *command) {
  if (!SelectCandidateInternal(command)) {
    return DoNothing(command);
  }
  Output(command);
  return true;
}


bool Session::HighlightCandidate(commands::Command *command) {
  if (!SelectCandidateInternal(command)) {
    return false;
  }
  context_->mutable_converter()->SetCandidateListVisible(true);
  Output(command);
  return true;
}

bool Session::MaybeSelectCandidate(commands::Command *command) {
  if (context_->state() != ImeContext::CONVERSION) {
    return false;
  }

  // Note that SHORTCUT_ASDFGHJKL should be handled even when the CapsLock is
  // enabled. This is why we need to normalize the key event here.
  // See b/5655743.
  commands::KeyEvent normalized_keyevent;
  keymap::NormalizeKeyEvent(command->input().key(), &normalized_keyevent);

  // Check if the input character is in the shortcut.
  // TODO(komatsu): Support non ASCII characters such as Unicode and
  // special keys.
  const char shortcut = static_cast<char>(normalized_keyevent.key_code());
  return context_->mutable_converter()->CandidateMoveToShortcut(shortcut);
}

void Session::set_client_capability(const commands::Capability &capability) {
  context_->mutable_client_capability()->CopyFrom(capability);
}

void Session::set_application_info(const commands::ApplicationInfo
                                   &application_info) {
  context_->mutable_application_info()->CopyFrom(application_info);
}

const commands::ApplicationInfo &Session::application_info() const {
  return context_->application_info();
}

uint64 Session::create_session_time() const {
  return context_->create_time();
}

uint64 Session::last_command_time() const {
  return context_->last_command_time();
}

bool Session::InsertCharacter(commands::Command *command) {
  if (!command->input().has_key()) {
    LOG(ERROR) << "No key event: " << command->input().DebugString();
    return false;
  }

  const commands::KeyEvent &key = command->input().key();
  if (key.input_style() == commands::KeyEvent::DIRECT_INPUT &&
      context_->state() == ImeContext::PRECOMPOSITION) {
    // If the key event represents a half width ascii character (ie.
    // key_code is equal to key_string), that key event is not
    // consumed and done echo back.
    if (key.key_string().size() == 1 &&
        key.key_code() == key.key_string()[0]) {
      return EchoBackAndClearUndoContext(command);
    }

    context_->mutable_composer()->InsertCharacterKeyEvent(key);
    SetSessionState(ImeContext::COMPOSITION);
    return Commit(command);
  }

  command->mutable_output()->set_consumed(true);

  // Handle shortcut keys selecting a candidate from a list.
  if (MaybeSelectCandidate(command)) {
    Output(command);
    return true;
  }


  string composition;
  context_->composer().GetQueryForConversion(&composition);
  bool should_commit = (context_->state() == ImeContext::CONVERSION);


  if (should_commit) {
    Commit(command);
    if (key.input_style() == commands::KeyEvent::DIRECT_INPUT) {
      // Do ClearUndoContext() because it is a direct input.
      ClearUndoContext();

      context_->mutable_composer()->InsertCharacterKeyEvent(key);
      context_->composer().GetQueryForConversion(&composition);
      string conversion;
      context_->composer().GetStringForSubmission(&conversion);

      commands::Result *result = command->mutable_output()->mutable_result();
      DCHECK(result != NULL);
      result->mutable_key()->append(composition);
      result->mutable_value()->append(conversion);

      SetSessionState(ImeContext::PRECOMPOSITION);
      Output(command);
      return true;
    }
  }

  context_->mutable_composer()->InsertCharacterKeyEvent(key);
  if (context_->mutable_composer()->ShouldCommit()) {
    return Commit(command);
  }
  size_t length_to_commit = 0;
  if (context_->mutable_composer()->ShouldCommitHead(&length_to_commit)) {
    return CommitHead(length_to_commit, command);
  }

  ExpandCompositionForCalculator(command);

  SetSessionState(ImeContext::COMPOSITION);
  if (CanStartAutoConversion(key)) {
    return Convert(command);
  }

  if (context_->mutable_converter()->Suggest(context_->composer())) {
    DCHECK(context_->converter().IsActive());
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::IsFullWidthInsertSpace() const {
  // If IME is off, any space has to be half-width.
  if (context_->state() == ImeContext::DIRECT) {
    return false;
  }

  // PRECOMPOSITION and current mode is HALF_ASCII: situation is same
  // as DIRECT.
  if (context_->state() == ImeContext::PRECOMPOSITION &&
      transliteration::T13n::IsInHalfAsciiTypes(
          context_->composer().GetInputMode())) {
    return false;
  }

  // Otherwise, check the current config and the current input status.
  bool is_full_width = false;
  switch (GET_CONFIG(space_character_form)) {
    case config::Config::FUNDAMENTAL_INPUT_MODE: {
      const transliteration::TransliterationType input_mode =
          context_->composer().GetInputMode();
      if (transliteration::T13n::IsInHalfAsciiTypes(input_mode) ||
          transliteration::T13n::IsInHalfKatakanaTypes(input_mode)) {
        is_full_width = false;
      } else {
        is_full_width = true;
      }
      break;
    }
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
  if (IsFullWidthInsertSpace()) {
    return InsertSpaceFullWidth(command);
  } else {
    return InsertSpaceHalfWidth(command);
  }
}

bool Session::InsertSpaceToggled(commands::Command *command) {
  if (IsFullWidthInsertSpace()) {
    return InsertSpaceHalfWidth(command);
  } else {
    return InsertSpaceFullWidth(command);
  }
}

bool Session::InsertSpaceHalfWidth(commands::Command *command) {
  if (!(context_->state() & (ImeContext::PRECOMPOSITION |
                             ImeContext::COMPOSITION |
                             ImeContext::CONVERSION))) {
    return DoNothing(command);
  }

  if (context_->state() == ImeContext::PRECOMPOSITION) {
    return EchoBackAndClearUndoContext(command);
  }

  const commands::CompositionMode mode = command->input().key().mode();
  command->mutable_input()->clear_key();
  commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
  key_event->set_key_code(' ');
  key_event->set_key_string(" ");
  key_event->set_input_style(commands::KeyEvent::DIRECT_INPUT);
  key_event->set_mode(mode);
  return InsertCharacter(command);
}

bool Session::InsertSpaceFullWidth(commands::Command *command) {
  if (!(context_->state() & (ImeContext::PRECOMPOSITION |
                             ImeContext::COMPOSITION |
                             ImeContext::CONVERSION))) {
    return DoNothing(command);
  }

  if (context_->state() == ImeContext::PRECOMPOSITION) {
    // TODO(komatsu): make sure if
    // |context_->mutable_converter()->Reset()| is necessary here.
    context_->mutable_converter()->Reset();
  }

  const commands::CompositionMode mode = command->input().key().mode();
  command->mutable_input()->clear_key();
  commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
  key_event->set_key_code(' ');
  // "　" (full-width space)
  key_event->set_key_string("\xE3\x80\x80");
  key_event->set_input_style(commands::KeyEvent::DIRECT_INPUT);
  key_event->set_mode(mode);
  return InsertCharacter(command);
}

bool Session::EditCancel(commands::Command *command) {
  if (CommitIfPassword(command)) {
    command->mutable_output()->set_consumed(false);
    return true;
  }

  command->mutable_output()->set_consumed(true);

  // If source_text is set, it usually means this session started by a
  // reverse conversion.  In this case EditCancel should restore the
  // string used for the reverse conversion.
  if (!context_->composer().source_text().empty()) {
    // The value of source_text is reset at the below Reset
    // command, so this variable cannot be a reference.
    const string source_text = context_->composer().source_text();
    context_->mutable_composer()->Reset();
    context_->mutable_composer()->InsertCharacterPreedit(source_text);
    context_->mutable_converter()->CommitPreedit(context_->composer());

    SetSessionState(ImeContext::PRECOMPOSITION);
    Output(command);
    return true;
  }

  SetSessionState(ImeContext::PRECOMPOSITION);
  // It is nice to use Output() instead of OutputMode().  However, if
  // Output() is used, unnecessary candidate words are shown because
  // the previous candidate state is not cleared here.  To fix it, we
  // should carefully modify SessionConverter.
  //
  // TODO(komatsu): Use Output() instead of OutputMode.
  OutputMode(command);
  return true;
}

bool Session::Commit(commands::Command *command) {
  if (!(context_->state() & (ImeContext::COMPOSITION |
                             ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  PushUndoContext();

  if (context_->state() == ImeContext::COMPOSITION) {
    context_->mutable_converter()->CommitPreedit(context_->composer());
  } else {  // ImeContext::CONVERSION
    context_->mutable_converter()->Commit();
  }

  SetSessionState(ImeContext::PRECOMPOSITION);


  Output(command);
  // Copy the previous output for Undo.
  context_->mutable_output()->CopyFrom(command->output());
  return true;
}

bool Session::CommitHead(size_t count, commands::Command *command) {
  if (!(context_->state() &
      (ImeContext::COMPOSITION | ImeContext::PRECOMPOSITION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  // TODO(yamaguchi): Support undo feature.
  ClearUndoContext();

  size_t committed_size;
  context_->mutable_converter()->
      CommitHead(count, context_->composer(), &committed_size);
  context_->mutable_composer()->DeleteRange(0, committed_size);
  Output(command);
  return true;
}

bool Session::CommitFirstSuggestion(commands::Command *command) {
  if (!(context_->state() == ImeContext::COMPOSITION ||
        context_->state() == ImeContext::PRECOMPOSITION)) {
    return DoNothing(command);
  }
  if (!context_->converter().IsActive()) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  PushUndoContext();

  const int kFirstIndex = 0;
  size_t committed_key_size = 0;
  context_->mutable_converter()->CommitSuggestionByIndex(
      kFirstIndex, context_->composer(), &committed_key_size);

  SetSessionState(ImeContext::PRECOMPOSITION);


  Output(command);
  // Copy the previous output for Undo.
  context_->mutable_output()->CopyFrom(command->output());
  return true;
}

bool Session::CommitSegment(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  PushUndoContext();

  CommitFirstSegmentInternal();

  if (!context_->converter().IsActive()) {
    // If the converter is not active (ie. the segment size was one.),
    // the state should be switched to precomposition.
    SetSessionState(ImeContext::PRECOMPOSITION);

  }
  Output(command);
  // Copy the previous output for Undo.
  context_->mutable_output()->CopyFrom(command->output());
  return true;
}

void Session::CommitFirstSegmentInternal() {
  size_t size;
  context_->mutable_converter()->CommitFirstSegment(&size);
  if (size > 0) {
    // Delete the key characters of the first segment from the preedit.
    context_->mutable_composer()->DeleteRange(0, size);
    // The number of segments should be more than one.
    DCHECK_GT(context_->composer().GetLength(), 0);
  }
}

bool Session::ConvertToTransliteration(
    commands::Command *command,
    const transliteration::TransliterationType type) {
  if (!(context_->state() & (ImeContext::CONVERSION |
                             ImeContext::COMPOSITION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  if (!context_->mutable_converter()->ConvertToTransliteration(
          context_->composer(), type)) {
    return false;
  }
  SetSessionState(ImeContext::CONVERSION);
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
  if (!(context_->state() & (ImeContext::CONVERSION |
                             ImeContext::COMPOSITION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  if (!context_->mutable_converter()->SwitchKanaType(context_->composer())) {
    return false;
  }
  SetSessionState(ImeContext::CONVERSION);
  Output(command);
  return true;
}

bool Session::DisplayAsHiragana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertToHiragana(command);
  } else {  // context_->state() == ImeContext::COMPOSITION
    context_->mutable_composer()->SetOutputMode(transliteration::HIRAGANA);
    OutputComposition(command);
    return true;
  }
}

bool Session::DisplayAsFullKatakana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertToFullKatakana(command);
  } else {  // context_->state() == ImeContext::COMPOSITION
    context_->mutable_composer()->SetOutputMode(transliteration::FULL_KATAKANA);
    OutputComposition(command);
    return true;
  }
}

bool Session::DisplayAsHalfKatakana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertToHalfKatakana(command);
  } else {  // context_->state() == ImeContext::COMPOSITION
    context_->mutable_composer()->SetOutputMode(transliteration::HALF_KATAKANA);
    OutputComposition(command);
    return true;
  }
}

bool Session::TranslateFullASCII(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertToFullASCII(command);
  } else {  // context_->state() == ImeContext::COMPOSITION
    context_->mutable_composer()->SetOutputMode(
        transliteration::T13n::ToggleFullAsciiTypes(
            context_->composer().GetOutputMode()));
    OutputComposition(command);
    return true;
  }
}

bool Session::TranslateHalfASCII(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertToHalfASCII(command);
  } else {  // context_->state() == ImeContext::COMPOSITION
    context_->mutable_composer()->SetOutputMode(
        transliteration::T13n::ToggleHalfAsciiTypes(
            context_->composer().GetOutputMode()));
    OutputComposition(command);
    return true;
  }
}

bool Session::InputModeHiragana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  SwitchInputMode(transliteration::HIRAGANA, context_->mutable_composer());
  OutputFromState(command);
  return true;
}

bool Session::InputModeFullKatakana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  SwitchInputMode(transliteration::FULL_KATAKANA, context_->mutable_composer());
  OutputFromState(command);
  return true;
}

bool Session::InputModeHalfKatakana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  SwitchInputMode(transliteration::HALF_KATAKANA, context_->mutable_composer());
  OutputFromState(command);
  return true;
}

bool Session::InputModeFullASCII(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  SwitchInputMode(transliteration::FULL_ASCII, context_->mutable_composer());
  OutputFromState(command);
  return true;
}

bool Session::InputModeHalfASCII(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  SwitchInputMode(transliteration::HALF_ASCII, context_->mutable_composer());
  OutputFromState(command);
  return true;
}

bool Session::InputModeSwitchKanaType(commands::Command *command) {
  if (context_->state() != ImeContext::PRECOMPOSITION) {
    return DoNothing(command);
  }

  command->mutable_output()->set_consumed(true);

  transliteration::TransliterationType current_type =
      context_->composer().GetInputMode();
  transliteration::TransliterationType next_type;

  switch (current_type) {
  case transliteration::HIRAGANA:
    next_type = transliteration::FULL_KATAKANA;
    break;

  case transliteration::FULL_KATAKANA:
    next_type = transliteration::HALF_KATAKANA;
    break;

  case transliteration::HALF_KATAKANA:
    next_type = transliteration::HIRAGANA;
    break;

  case transliteration::HALF_ASCII:
  case transliteration::FULL_ASCII:
    next_type = current_type;
    break;

  default:
    LOG(ERROR) << "Unknown input mode: " << current_type;
    // don't change input mode
    next_type = current_type;
    break;
  }

  // The temporary mode should not be overridden.
  SwitchInputMode(next_type, context_->mutable_composer());
  OutputFromState(command);
  return true;
}

bool Session::ConvertToHalfWidth(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION |
                             ImeContext::COMPOSITION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  if (!context_->mutable_converter()->ConvertToHalfWidth(
          context_->composer())) {
    return false;
  }
  SetSessionState(ImeContext::CONVERSION);
  Output(command);
  return true;
}

bool Session::TranslateHalfWidth(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertToHalfWidth(command);
  } else {  // context_->state() == ImeContext::COMPOSITION
    const transliteration::TransliterationType type =
      context_->composer().GetOutputMode();
    if (type == transliteration::HIRAGANA ||
        type == transliteration::FULL_KATAKANA ||
        type == transliteration::HALF_KATAKANA) {
      context_->mutable_composer()->SetOutputMode(
          transliteration::HALF_KATAKANA);
    } else if (type == transliteration::FULL_ASCII) {
      context_->mutable_composer()->SetOutputMode(transliteration::HALF_ASCII);
    } else if (type == transliteration::FULL_ASCII_UPPER) {
      context_->mutable_composer()->SetOutputMode(
          transliteration::HALF_ASCII_UPPER);
    } else if (type == transliteration::FULL_ASCII_LOWER) {
      context_->mutable_composer()->SetOutputMode(
          transliteration::HALF_ASCII_LOWER);
    } else if (type == transliteration::FULL_ASCII_CAPITALIZED) {
      context_->mutable_composer()->SetOutputMode(
          transliteration::HALF_ASCII_CAPITALIZED);
    } else {
      // transliteration::HALF_ASCII_something
      return TranslateHalfASCII(command);
    }
    OutputComposition(command);
    return true;
  }
}

bool Session::LaunchConfigDialog(commands::Command *command) {
  command->mutable_output()->set_launch_tool_mode(
      commands::Output::CONFIG_DIALOG);
  return DoNothing(command);
}

bool Session::LaunchDictionaryTool(commands::Command *command) {
  command->mutable_output()->set_launch_tool_mode(
      commands::Output::DICTIONARY_TOOL);
  return DoNothing(command);
}

bool Session::LaunchWordRegisterDialog(commands::Command *command) {
  command->mutable_output()->set_launch_tool_mode(
      commands::Output::WORD_REGISTER_DIALOG);
  return DoNothing(command);
}

bool Session::SendComposerCommand(
    const composer::Composer::InternalCommand composer_command,
    commands::Command *command) {
  if (!(context_->state() & ImeContext::COMPOSITION)) {
    DLOG(WARNING) << "State : " << context_->state();
    return false;
  }

  context_->mutable_composer()->InsertCommandCharacter(composer_command);
  // InsertCommandCharacter method updates the preedit text
  // so we need to update suggest candidates.
  if (context_->mutable_converter()->Suggest(context_->composer())) {
    DCHECK(context_->converter().IsActive());
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::ToggleAlphanumericMode(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_composer()->ToggleInputMode();

  OutputFromState(command);
  return true;
}

bool Session::Convert(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  string composition;
  context_->composer().GetQueryForConversion(&composition);

  // TODO(komatsu): Make a function like ConvertOrSpace.
  // Handle a space key on the ASCII composition mode.
  if (context_->state() == ImeContext::COMPOSITION &&
      (context_->composer().GetInputMode() == transliteration::HALF_ASCII ||
       context_->composer().GetInputMode() == transliteration::FULL_ASCII) &&
      command->input().key().has_special_key() &&
      command->input().key().special_key() == commands::KeyEvent::SPACE) {

    // TODO(komatsu): Consider FullWidth Space too.
    if (!Util::EndsWith(composition, " ")) {
      bool should_commit = false;
      if (should_commit) {
        // Space is committed with the composition
        context_->mutable_composer()->InsertCharacterPreedit(" ");
        return Commit(command);
      } else {
        // SPACE_OR_CONVERT_KEEPING_COMPOSITION or
        // SPACE_OR_CONVERT_COMMITING_COMPOSITION.

        // If the last character is not space, space is inserted to the
        // composition.
        command->mutable_input()->mutable_key()->set_key_code(' ');
        return InsertCharacter(command);
      }
    }

    if (!composition.empty()) {
      DCHECK_EQ(' ', composition[composition.size() - 1]);
      // Delete the last space.
      context_->mutable_composer()->Backspace();
    }
  }

  if (!context_->mutable_converter()->Convert(context_->composer())) {
    LOG(ERROR) << "Conversion failed for some reasons.";
    OutputComposition(command);
    return true;
  }

  SetSessionState(ImeContext::CONVERSION);
  Output(command);
  return true;
}

bool Session::ConvertWithoutHistory(commands::Command *command) {
  command->mutable_output()->set_consumed(true);

  ConversionPreferences preferences =
    context_->converter().conversion_preferences();
  preferences.use_history = false;
  if (!context_->mutable_converter()->ConvertWithPreferences(
          context_->composer(), preferences)) {
    LOG(ERROR) << "Conversion failed for some reasons.";
    OutputComposition(command);
    return true;
  }

  SetSessionState(ImeContext::CONVERSION);
  Output(command);
  return true;
}

bool Session::CommitIfPassword(commands::Command *command) {
  if (context_->composer().GetInputFieldType() ==
      commands::SessionCommand::PASSWORD) {
    Commit(command);
    return true;
  }
  return false;
}

bool Session::MoveCursorRight(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (CommitIfPassword(command)) {
    return true;
  }
  context_->mutable_composer()->MoveCursorRight();
  if (context_->mutable_converter()->Suggest(context_->composer())) {
    DCHECK(context_->converter().IsActive());
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::MoveCursorLeft(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (CommitIfPassword(command)) {
    return true;
  }
  context_->mutable_composer()->MoveCursorLeft();
  if (context_->mutable_converter()->Suggest(context_->composer())) {
    DCHECK(context_->converter().IsActive());
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::MoveCursorToEnd(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (CommitIfPassword(command)) {
    return true;
  }
  context_->mutable_composer()->MoveCursorToEnd();
  if (context_->mutable_converter()->Suggest(context_->composer())) {
    DCHECK(context_->converter().IsActive());
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::MoveCursorTo(commands::Command *command) {
  if (context_->state() != ImeContext::COMPOSITION) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  if (CommitIfPassword(command)) {
    return true;
  }
  context_->mutable_composer()->
      MoveCursorTo(command->input().command().cursor_position());
  if (context_->mutable_converter()->Suggest(context_->composer())) {
    DCHECK(context_->converter().IsActive());
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::MoveCursorToBeginning(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (CommitIfPassword(command)) {
    return true;
  }
  context_->mutable_composer()->MoveCursorToBeginning();
  if (context_->mutable_converter()->Suggest(context_->composer())) {
    DCHECK(context_->converter().IsActive());
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::Delete(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_composer()->Delete();
  if (context_->mutable_composer()->Empty()) {
    SetSessionState(ImeContext::PRECOMPOSITION);
    OutputMode(command);
  } else if (context_->mutable_converter()->Suggest(context_->composer())) {
    DCHECK(context_->converter().IsActive());
    Output(command);
    return true;
  } else {
    OutputComposition(command);
  }
  return true;
}

bool Session::Backspace(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_composer()->Backspace();
  if (context_->mutable_composer()->Empty()) {
    SetSessionState(ImeContext::PRECOMPOSITION);
    OutputMode(command);
  } else if (context_->mutable_converter()->Suggest(context_->composer())) {
    DCHECK(context_->converter().IsActive());
    Output(command);
    return true;
  } else {
    OutputComposition(command);
  }
  return true;
}

bool Session::SegmentFocusRight(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->SegmentFocusRight();
  Output(command);
  return true;
}

bool Session::SegmentFocusLast(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->SegmentFocusLast();
  Output(command);
  return true;
}

bool Session::SegmentFocusLeft(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->SegmentFocusLeft();
  Output(command);
  return true;
}

bool Session::SegmentFocusLeftEdge(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->SegmentFocusLeftEdge();
  Output(command);
  return true;
}

bool Session::SegmentWidthExpand(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->SegmentWidthExpand();
  Output(command);
  return true;
}

bool Session::SegmentWidthShrink(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->SegmentWidthShrink();
  Output(command);
  return true;
}

bool Session::ReportBug(commands::Command *command) {
  return DoNothing(command);
}

bool Session::ConvertNext(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->CandidateNext(context_->composer());
  Output(command);
  return true;
}

bool Session::ConvertNextPage(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->CandidateNextPage();
  Output(command);
  return true;
}

bool Session::ConvertPrev(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->CandidatePrev();
  Output(command);
  return true;
}

bool Session::ConvertPrevPage(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->CandidatePrevPage();
  Output(command);
  return true;
}

bool Session::ConvertCancel(commands::Command *command) {
  command->mutable_output()->set_consumed(true);

  SetSessionState(ImeContext::COMPOSITION);
  context_->mutable_converter()->Cancel();
  if (context_->mutable_converter()->Suggest(context_->composer())) {
    DCHECK(context_->converter().IsActive());
    Output(command);
  } else {
    OutputComposition(command);
  }
  return true;
}

bool Session::PredictAndConvert(commands::Command *command) {
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertNext(command);
  }

  command->mutable_output()->set_consumed(true);
  if (context_->mutable_converter()->Predict(context_->composer())) {
    SetSessionState(ImeContext::CONVERSION);
    Output(command);
  } else {
    OutputComposition(command);
  }
  return true;
}

bool Session::ExpandSuggestion(commands::Command *command) {
  if (context_->state() == ImeContext::CONVERSION ||
      context_->state() == ImeContext::DIRECT) {
    return DoNothing(command);
  }

  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->ExpandSuggestion(context_->composer());
  Output(command);
  return true;
}

void Session::OutputFromState(commands::Command *command) {
  if (context_->state() == ImeContext::PRECOMPOSITION) {
    OutputMode(command);
    return;
  }
  if (context_->state() == ImeContext::COMPOSITION) {
    OutputComposition(command);
    return;
  }
  if (context_->state() == ImeContext::CONVERSION) {
    Output(command);
    return;
  }
  OutputMode(command);
}

void Session::Output(commands::Command *command) {
  OutputMode(command);
  context_->mutable_converter()->PopOutput(
      context_->composer(), command->mutable_output());
  OutputWindowLocation(command);
}

void Session::OutputWindowLocation(commands::Command *command) const {
  if (!(command->output().has_candidates() &&
        caret_rectangle_.IsInitialized() &&
        composition_rectangle_.IsInitialized())) {
    return;
  }

  DCHECK(command->output().candidates().has_category());

  commands::Candidates *candidates =
      command->mutable_output()->mutable_candidates();

  candidates->mutable_caret_rectangle()->CopyFrom(caret_rectangle_);

  candidates->mutable_composition_rectangle()->CopyFrom(
      composition_rectangle_);

  if (command->output().candidates().category() == commands::SUGGESTION) {
    candidates->set_window_location(commands::Candidates::COMPOSITION);
  } else {
    candidates->set_window_location(commands::Candidates::CARET);
  }
}

void Session::OutputMode(commands::Command *command) const {
  commands::CompositionMode mode = commands::HIRAGANA;
  switch (context_->composer().GetInputMode()) {
  case transliteration::HIRAGANA:
    mode = commands::HIRAGANA;
    break;
  case transliteration::FULL_KATAKANA:
    mode = commands::FULL_KATAKANA;
    break;
  case transliteration::HALF_KATAKANA:
    mode = commands::HALF_KATAKANA;
    break;
  case transliteration::FULL_ASCII:
    mode = commands::FULL_ASCII;
    break;
  case transliteration::HALF_ASCII:
    mode = commands::HALF_ASCII;
    break;
  default:
    LOG(ERROR) << "Unknown input mode: " << context_->composer().GetInputMode();
    // use HIRAGANA as a default.
  }

  if (context_->state() == ImeContext::DIRECT) {
    command->mutable_output()->set_mode(commands::DIRECT);
    command->mutable_output()->mutable_status()->set_activated(false);
  } else {
    command->mutable_output()->set_mode(mode);
    command->mutable_output()->mutable_status()->set_activated(true);
  }
  command->mutable_output()->mutable_status()->set_mode(mode);
}

void Session::OutputComposition(commands::Command *command) const {
  OutputMode(command);
  commands::Preedit *preedit = command->mutable_output()->mutable_preedit();
  SessionOutput::FillPreedit(context_->composer(), preedit);
}

void Session::OutputKey(commands::Command *command) const {
  OutputMode(command);
  commands::KeyEvent *key = command->mutable_output()->mutable_key();
  key->CopyFrom(command->input().key());
}

namespace {
// return
// ((key_code == static_cast<uint32>('.') ||
//       key_string == "." || key_string == "．" ||
//   key_string == "。" || key_string == "｡") &&
//  (config.auto_conversion_key() &
//   config::Config::AUTO_CONVERSION_KUTEN)) ||
// ((key_code == static_cast<uint32>(',') ||
//       key_string == "," || key_string == "，" ||
//   key_string == "、" || key_string == "､") &&
//  (config.auto_conversion_key() &
//   config::Config::AUTO_CONVERSION_TOUTEN)) ||
// ((key_code == static_cast<uint32>('?') ||
//   key_string == "?" || key_string == "？") &&
//  (config.auto_conversion_key() &
//   config::Config::AUTO_CONVERSION_QUESTION_MARK)) ||
// ((key_code == static_cast<uint32>('!') ||
//   key_string == "!" || key_string == "！") &&
//  (config.auto_conversion_key() &
//   config::Config::AUTO_CONVERSION_EXCLAMATION_MARK));
bool IsValidKey(const config::Config &config,
                const uint32 key_code, const string &key_string) {
  return
      (((key_code == static_cast<uint32>('.') && key_string.empty()) ||
        key_string == "." || key_string == "\xEF\xBC\x8E" ||
        key_string == "\xE3\x80\x82" || key_string == "\xEF\xBD\xA1") &&
       (config.auto_conversion_key() &
        config::Config::AUTO_CONVERSION_KUTEN)) ||
      (((key_code == static_cast<uint32>(',') && key_string.empty()) ||
        key_string == "," || key_string == "\xEF\xBC\x8C" ||
        key_string == "\xE3\x80\x81" || key_string == "\xEF\xBD\xA4") &&
       (config.auto_conversion_key() &
        config::Config::AUTO_CONVERSION_TOUTEN)) ||
      (((key_code == static_cast<uint32>('?') && key_string.empty()) ||
        key_string == "?" || key_string == "\xEF\xBC\x9F") &&
       (config.auto_conversion_key() &
        config::Config::AUTO_CONVERSION_QUESTION_MARK)) ||
      (((key_code == static_cast<uint32>('!') && key_string.empty()) ||
        key_string == "!" || key_string == "\xEF\xBC\x81") &&
       (config.auto_conversion_key() &
        config::Config::AUTO_CONVERSION_EXCLAMATION_MARK));
}
}  // namespace

bool Session::CanStartAutoConversion(
    const commands::KeyEvent &key_event) const {
  if (!GET_CONFIG(use_auto_conversion)) {
    return false;
  }

  // Disable if the input comes from non-standard user keyboards, like numpad.
  // http://b/issue?id=2932067
  if (key_event.input_style() != commands::KeyEvent::FOLLOW_MODE) {
    return false;
  }

  // This is a tentative workaround for the bug http://b/issue?id=2932028
  // When user types <Shift Down>O<Shift Up>racle<Shift Down>!<Shift Up>,
  // The final "!" must be half-width, however, due to the limitation
  // of converter interface, we don't have a good way to change it halfwidth, as
  // the default preference of "!" is fullwidth. Basically, the converter is
  // not composition-mode-aware.
  // We simply disable the auto conversion feature if the mode is ASCII.
  // We conclude that disabling this feature is better in this situation.
  // TODO(taku): fix the behavior. Converter module needs to be fixed.
  if (key_event.mode() == commands::HALF_ASCII ||
      key_event.mode() == commands::FULL_ASCII) {
    return false;
  }

  // We should NOT check key_string. http://b/issue?id=3217992

  // now evaluate preedit string and preedit length.
  const size_t length = context_->composer().GetLength();
  if (length <= 1) {
    return false;
  }

  const config::Config &config = config::ConfigHandler::GetConfig();
  const uint32 key_code = key_event.key_code();

  string preedit;
  context_->composer().GetStringForPreedit(&preedit);
  const string last_char = Util::SubString(preedit, length - 1, 1);
  if (last_char.empty()) {
    return false;
  }

  // Check last character as user may change romaji table,
  // For instance, if user assigns "." as "foo", we don't
  // want to invoke auto_conversion.
  if (!IsValidKey(config, key_code, last_char)) {
    return false;
  }

  // check the previous character of last_character.
  // when |last_prev_char| is number, we don't invoke auto_conversion
  // if the same invoke key is repeated, do not conversion.
  // http://b/issue?id=2932118
  const string last_prev_char = Util::SubString(preedit, length - 2, 1);
  if (last_prev_char.empty() || last_prev_char == last_char ||
      Util::NUMBER == Util::GetScriptType(last_prev_char)) {
    return false;
  }

  return true;
}

void Session::UpdateTime() {
  context_->set_last_command_time(Util::GetTime());
}

void Session::TransformInput(commands::Input *input) {
  if (input->has_key()) {
    TransformKeyEvent(context_->transform_table(), input->mutable_key());
  }
  context_->converter().FillContext(input->mutable_context());
}

void Session::ExpandCompositionForCalculator(commands::Command *command) {
  if (!(context_->client_capability().text_deletion() &
        commands::Capability::DELETE_PRECEDING_TEXT)) {
    return;
  }
  if (!command->input().has_context()) {
    return;
  }

  // Expand composition if expanded composition makes an expression of
  // Calculator.
  // E.g. if preceding text is "あいう１" and composition is "+1=", then
  // composition is expanded to "１+1=".
  string preedit, expanded_characters;
  context_->composer().GetStringForPreedit(&preedit);
  const size_t expansion_length =
      GetCompositionExpansionForCalculator(
          command->input().context().preceding_text(),
          preedit,
          &expanded_characters);

  if (expansion_length == 0) {
    return;
  }

  context_->mutable_composer()->InsertCharacterPreeditAt(0,
                                                         expanded_characters);

  commands::DeletionRange *range =
      command->mutable_output()->mutable_deletion_range();
  range->set_offset(-static_cast<int>(expansion_length));
  range->set_length(expansion_length);

  // Delete part of history segments, because corresponding surrounding
  // text will be removed by client.
  context_->mutable_converter()->RemoveTailOfHistorySegments(expansion_length);
}

bool Session::SwitchInputFieldType(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_composer()->SetInputFieldType(
       command->input().command().input_field_type());
  Output(command);
  return true;
}

bool Session::SetCaretLocation(commands::Command *command) {
  if (!command->input().has_command()) {
    return false;
  }

  const commands::SessionCommand &session_command = command->input().command();
  if (session_command.has_caret_rectangle()) {
    caret_rectangle_.CopyFrom(session_command.caret_rectangle());
  } else {
    caret_rectangle_.Clear();
  }
  return true;
}

// TODO(komatsu): delete this funciton.
composer::Composer *Session::get_internal_composer_only_for_unittest() {
  return context_->mutable_composer();
}

const ImeContext &Session::context() const {
  return *context_;
}

}  // namespace session
}  // namespace mozc

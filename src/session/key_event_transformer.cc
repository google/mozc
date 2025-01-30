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

#include "session/key_event_transformer.h"

#include <cstdint>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "base/japanese_util.h"
#include "composer/key_event_util.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace session {
namespace {

using ::mozc::commands::KeyEvent;
using ::mozc::config::Config;

}  // namespace

KeyEventTransformer::KeyEventTransformer() {
  ReloadConfig(config::ConfigHandler::DefaultConfig());
}

void KeyEventTransformer::ReloadConfig(const Config &config) {
  numpad_character_form_ = config.numpad_character_form();

  table_.clear();
  const Config::PunctuationMethod punctuation = config.punctuation_method();
  if (punctuation == Config::COMMA_PERIOD ||
      punctuation == Config::COMMA_TOUTEN) {
    KeyEvent key_event;
    key_event.set_key_code(static_cast<uint32_t>(','));
    key_event.set_key_string("，");
    table_.emplace("、", key_event);
  }
  if (punctuation == Config::COMMA_PERIOD ||
      punctuation == Config::KUTEN_PERIOD) {
    KeyEvent key_event;
    key_event.set_key_code(static_cast<uint32_t>('.'));
    key_event.set_key_string("．");
    table_.emplace("。", key_event);
  }

  const Config::SymbolMethod symbol = config.symbol_method();
  if (symbol == Config::SQUARE_BRACKET_SLASH ||
      symbol == Config::SQUARE_BRACKET_MIDDLE_DOT) {
    {
      KeyEvent key_event;
      key_event.set_key_code(static_cast<uint32_t>('['));
      key_event.set_key_string("［");
      table_.emplace("「", key_event);
    }
    {
      KeyEvent key_event;
      key_event.set_key_code(static_cast<uint32_t>(']'));
      key_event.set_key_string("］");
      table_.emplace("」", key_event);
    }
  }
  if (symbol == Config::SQUARE_BRACKET_SLASH ||
      symbol == Config::CORNER_BRACKET_SLASH) {
    KeyEvent key_event;
    key_event.set_key_code(static_cast<uint32_t>('/'));
    key_event.set_key_string("／");
    table_.emplace("・", key_event);
  }
}

bool KeyEventTransformer::TransformKeyEvent(KeyEvent *key_event) const {
  if (key_event == nullptr) {
    LOG(ERROR) << "key_event is NULL";
    return false;
  }
  if (TransformKeyEventForNumpad(key_event)) {
    return true;
  }
  if (TransformKeyEventForKana(key_event)) {
    return true;
  }
  return false;
}

bool KeyEventTransformer::TransformKeyEventForNumpad(
    KeyEvent *key_event) const {
  DCHECK(key_event);

  if (!KeyEventUtil::IsNumpadKey(*key_event)) {
    return false;
  }

  {
    KeyEvent key_event_origin;
    key_event_origin = *key_event;
    KeyEventUtil::NormalizeNumpadKey(key_event_origin, key_event);
  }

  // KeyEvent::SEPARATOR is transformed to Enter.
  if (key_event->has_special_key()) {
    DCHECK_EQ(KeyEvent::ENTER, key_event->special_key());
    return true;
  }

  bool is_full_width = true;
  switch (numpad_character_form_) {
    case Config::NUMPAD_INPUT_MODE:
      is_full_width = true;
      key_event->set_input_style(KeyEvent::FOLLOW_MODE);
      break;
    case Config::NUMPAD_FULL_WIDTH:
      is_full_width = true;
      key_event->set_input_style(KeyEvent::AS_IS);
      break;
    case Config::NUMPAD_HALF_WIDTH:
      is_full_width = false;
      key_event->set_input_style(KeyEvent::AS_IS);
      break;
    case Config::NUMPAD_DIRECT_INPUT:
      is_full_width = false;
      key_event->set_input_style(KeyEvent::DIRECT_INPUT);
      break;
    default:
      LOG(ERROR) << "Unknown numpad character form value.";
      // Use the same behavior with NUMPAD_HALF_WIDTH as a fallback.
      is_full_width = false;
      key_event->set_input_style(KeyEvent::AS_IS);
      break;
  }

  // All key event except for KeyEvent::SEPARATOR should have key code
  // and its value should represent a ASCII character since it is generated
  // from numpad key.
  DCHECK(key_event->has_key_code());
  const uint32_t key_code = key_event->key_code();
  DCHECK_GT(128, key_code);
  const std::string half_width_key_string(1, static_cast<char>(key_code));

  if (is_full_width) {
    std::string full_width_key_string =
        japanese_util::HalfWidthAsciiToFullWidthAscii(half_width_key_string);
    key_event->set_key_string(full_width_key_string);
  } else {
    key_event->set_key_string(half_width_key_string);
  }

  return true;
}

bool KeyEventTransformer::TransformKeyEventForKana(KeyEvent *key_event) const {
  DCHECK(key_event);

  if (!key_event->has_key_string()) {
    return false;
  }
  if (key_event->modifier_keys_size() > 0) {
    return false;
  }
  if (key_event->has_modifiers() && key_event->modifiers() != 0) {
    return false;
  }

  const Table::const_iterator it = table_.find(key_event->key_string());
  if (it == table_.end()) {
    return false;
  }

  *key_event = it->second;
  return true;
}

}  // namespace session
}  // namespace mozc

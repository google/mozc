// Copyright 2010-2013, Google Inc.
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

#include "session/internal/key_event_transformer.h"

#include <string>

#include "base/base.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "session/key_event_util.h"

namespace mozc {
namespace session {

KeyEventTransformer::KeyEventTransformer() {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  ReloadConfig(config);
}

KeyEventTransformer::~KeyEventTransformer() {
}

void KeyEventTransformer::ReloadConfig(const config::Config &config) {
  numpad_character_form_ = config.numpad_character_form();

  table_.clear();
  const config::Config::PunctuationMethod punctuation =
      config.punctuation_method();
  if (punctuation == config::Config::COMMA_PERIOD ||
      punctuation == config::Config::COMMA_TOUTEN) {
    commands::KeyEvent key_event;
    key_event.set_key_code(static_cast<uint32>(','));
    // "，"
    key_event.set_key_string("\xef\xbc\x8c");
    // "、"
    table_.insert(make_pair("\xe3\x80\x81", key_event));
  }
  if (punctuation == config::Config::COMMA_PERIOD ||
      punctuation == config::Config::KUTEN_PERIOD) {
    commands::KeyEvent key_event;
    key_event.set_key_code(static_cast<uint32>('.'));
    // "．"
    key_event.set_key_string("\xef\xbc\x8e");
    // "。"
    table_.insert(make_pair("\xe3\x80\x82", key_event));
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
      table_.insert(make_pair("\xe3\x80\x8c", key_event));
    }
    {
      commands::KeyEvent key_event;
      key_event.set_key_code(static_cast<uint32>(']'));
      // "］"
      key_event.set_key_string("\xef\xbc\xbd");
      // "」"
      table_.insert(make_pair("\xe3\x80\x8d", key_event));
    }
  }
  if (symbol == config::Config::SQUARE_BRACKET_SLASH ||
      symbol == config::Config::CORNER_BRACKET_SLASH) {
    commands::KeyEvent key_event;
    key_event.set_key_code(static_cast<uint32>('/'));
    // "／"
    key_event.set_key_string("\xef\xbc\x8f");
    // "・"
    table_.insert(make_pair("\xE3\x83\xBB", key_event));
  }
}

bool KeyEventTransformer::TransformKeyEvent(commands::KeyEvent *key_event) {
  if (key_event == NULL) {
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
    commands::KeyEvent *key_event) {
  DCHECK(key_event);

  if (!KeyEventUtil::IsNumpadKey(*key_event)) {
    return false;
  }

  {
    commands::KeyEvent key_event_origin;
    key_event_origin.CopyFrom(*key_event);
    KeyEventUtil::NormalizeNumpadKey(key_event_origin, key_event);
  }

  // commands::KeyEvent::SEPARATOR is transformed to Enter.
  if (key_event->has_special_key()) {
    DCHECK_EQ(commands::KeyEvent::ENTER, key_event->special_key());
    return true;
  }

  bool is_full_width = true;
  switch (numpad_character_form_) {
    case config::Config::NUMPAD_INPUT_MODE:
      is_full_width = true;
      key_event->set_input_style(commands::KeyEvent::FOLLOW_MODE);
      break;
    case config::Config::NUMPAD_FULL_WIDTH:
      is_full_width = true;
      key_event->set_input_style(commands::KeyEvent::AS_IS);
      break;
    case config::Config::NUMPAD_HALF_WIDTH:
      is_full_width = false;
      key_event->set_input_style(commands::KeyEvent::AS_IS);
      break;
    case config::Config::NUMPAD_DIRECT_INPUT:
      is_full_width = false;
      key_event->set_input_style(commands::KeyEvent::DIRECT_INPUT);
      break;
    default:
      LOG(ERROR) << "Unknown numpad character form value.";
      // Use the same behavior with NUMPAD_HALF_WIDTH as a fallback.
      is_full_width = false;
      key_event->set_input_style(commands::KeyEvent::AS_IS);
      break;
  }

  // All key event except for commands::KeyEvent::SEPARATOR should have key code
  // and it's value should represent a ASCII character since it is generated
  // from numpad key.
  DCHECK(key_event->has_key_code());
  const uint32 key_code = key_event->key_code();
  DCHECK_GT(128, key_code);
  const string half_width_key_string(1, static_cast<char>(key_code));

  if (is_full_width) {
    string full_width_key_string;
    Util::HalfWidthAsciiToFullWidthAscii(half_width_key_string,
                                         &full_width_key_string);
    key_event->set_key_string(full_width_key_string);
  } else {
    key_event->set_key_string(half_width_key_string);
  }

  return true;
}

bool KeyEventTransformer::TransformKeyEventForKana(
    commands::KeyEvent *key_event) {
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

  key_event->CopyFrom(it->second);
  return true;
}

}  // namespace session
}  // namespace mozc

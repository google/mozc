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

#include "session/internal/key_event_transformer.h"

#include "base/singleton.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"


namespace mozc {
namespace session {

namespace {
// Numpad keys are transformed to normal characters.
static const struct NumpadKeyEventTransformer {
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
}  // namespace


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
      switch (numpad_character_form_) {
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

bool KeyEventTransformer::TransformKeyEventForKana(
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

  const Table::const_iterator it = table_.find(key_event->key_string());
  if (it == table_.end()) {
    return false;
  }

  key_event->CopyFrom(it->second);
  return true;
}

}  // namespace session
}  // namespace mozc

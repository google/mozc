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

#include "unix/ibus/key_translator.h"

#include <ibus.h>

#include "base/logging.h"

namespace {

const struct SpecialKeyMap {
  guint from;
  mozc::commands::KeyEvent::SpecialKey to;
} special_key_map[] = {
  {IBUS_space, mozc::commands::KeyEvent::SPACE},
  {IBUS_Return, mozc::commands::KeyEvent::ENTER},
  {IBUS_Left, mozc::commands::KeyEvent::LEFT},
  {IBUS_Right, mozc::commands::KeyEvent::RIGHT},
  {IBUS_Up, mozc::commands::KeyEvent::UP},
  {IBUS_Down, mozc::commands::KeyEvent::DOWN},
  {IBUS_Escape, mozc::commands::KeyEvent::ESCAPE},
  {IBUS_Delete, mozc::commands::KeyEvent::DEL},
  {IBUS_BackSpace, mozc::commands::KeyEvent::BACKSPACE},
  {IBUS_Insert, mozc::commands::KeyEvent::INSERT},
  {IBUS_Henkan, mozc::commands::KeyEvent::HENKAN},
  {IBUS_Muhenkan, mozc::commands::KeyEvent::MUHENKAN},
  {IBUS_Hiragana, mozc::commands::KeyEvent::KANA},
  {IBUS_Katakana, mozc::commands::KeyEvent::KANA},
  {IBUS_Eisu_toggle, mozc::commands::KeyEvent::EISU},
  {IBUS_Home, mozc::commands::KeyEvent::HOME},
  {IBUS_End, mozc::commands::KeyEvent::END},
  {IBUS_Tab, mozc::commands::KeyEvent::TAB},
  {IBUS_F1, mozc::commands::KeyEvent::F1},
  {IBUS_F2, mozc::commands::KeyEvent::F2},
  {IBUS_F3, mozc::commands::KeyEvent::F3},
  {IBUS_F4, mozc::commands::KeyEvent::F4},
  {IBUS_F5, mozc::commands::KeyEvent::F5},
  {IBUS_F6, mozc::commands::KeyEvent::F6},
  {IBUS_F7, mozc::commands::KeyEvent::F7},
  {IBUS_F8, mozc::commands::KeyEvent::F8},
  {IBUS_F9, mozc::commands::KeyEvent::F9},
  {IBUS_F10, mozc::commands::KeyEvent::F10},
  {IBUS_F11, mozc::commands::KeyEvent::F11},
  {IBUS_F12, mozc::commands::KeyEvent::F12},
  {IBUS_F13, mozc::commands::KeyEvent::F13},
  {IBUS_F14, mozc::commands::KeyEvent::F14},
  {IBUS_F15, mozc::commands::KeyEvent::F15},
  {IBUS_F16, mozc::commands::KeyEvent::F16},
  {IBUS_F17, mozc::commands::KeyEvent::F17},
  {IBUS_F18, mozc::commands::KeyEvent::F18},
  {IBUS_F19, mozc::commands::KeyEvent::F19},
  {IBUS_F20, mozc::commands::KeyEvent::F20},
  {IBUS_F21, mozc::commands::KeyEvent::F21},
  {IBUS_F22, mozc::commands::KeyEvent::F22},
  {IBUS_F23, mozc::commands::KeyEvent::F23},
  {IBUS_F24, mozc::commands::KeyEvent::F24},
  {IBUS_Page_Up, mozc::commands::KeyEvent::PAGE_UP},
  {IBUS_Page_Down, mozc::commands::KeyEvent::PAGE_DOWN},

  // Keypad (10-key).
  {IBUS_KP_0, mozc::commands::KeyEvent::NUMPAD0},
  {IBUS_KP_1, mozc::commands::KeyEvent::NUMPAD1},
  {IBUS_KP_2, mozc::commands::KeyEvent::NUMPAD2},
  {IBUS_KP_3, mozc::commands::KeyEvent::NUMPAD3},
  {IBUS_KP_4, mozc::commands::KeyEvent::NUMPAD4},
  {IBUS_KP_5, mozc::commands::KeyEvent::NUMPAD5},
  {IBUS_KP_6, mozc::commands::KeyEvent::NUMPAD6},
  {IBUS_KP_7, mozc::commands::KeyEvent::NUMPAD7},
  {IBUS_KP_8, mozc::commands::KeyEvent::NUMPAD8},
  {IBUS_KP_9, mozc::commands::KeyEvent::NUMPAD9},
  {IBUS_KP_Equal, mozc::commands::KeyEvent::EQUALS},  // [=]
  {IBUS_KP_Multiply, mozc::commands::KeyEvent::MULTIPLY},  // [*]
  {IBUS_KP_Add, mozc::commands::KeyEvent::ADD},  // [+]
  {IBUS_KP_Separator, mozc::commands::KeyEvent::SEPARATOR},  // enter
  {IBUS_KP_Subtract, mozc::commands::KeyEvent::SUBTRACT},  // [-]
  {IBUS_KP_Decimal, mozc::commands::KeyEvent::DECIMAL},  // [.]
  {IBUS_KP_Divide, mozc::commands::KeyEvent::DIVIDE},  // [/]
  {IBUS_KP_Space, mozc::commands::KeyEvent::SPACE},
  {IBUS_KP_Tab, mozc::commands::KeyEvent::TAB},
  {IBUS_KP_Enter, mozc::commands::KeyEvent::ENTER},
  {IBUS_KP_Home, mozc::commands::KeyEvent::HOME},
  {IBUS_KP_Left, mozc::commands::KeyEvent::LEFT},
  {IBUS_KP_Up, mozc::commands::KeyEvent::UP},
  {IBUS_KP_Right, mozc::commands::KeyEvent::RIGHT},
  {IBUS_KP_Down, mozc::commands::KeyEvent::DOWN},
  {IBUS_KP_Page_Up, mozc::commands::KeyEvent::PAGE_UP},
  {IBUS_KP_Page_Down, mozc::commands::KeyEvent::PAGE_DOWN},
  {IBUS_KP_End, mozc::commands::KeyEvent::END},
  {IBUS_KP_Delete, mozc::commands::KeyEvent::DEL},
  {IBUS_KP_Insert, mozc::commands::KeyEvent::INSERT},

  // Shift+TAB.
  {IBUS_ISO_Left_Tab, mozc::commands::KeyEvent::TAB},

  // TODO(mazda): Handle following keys?
  //   - IBUS_Kana_Lock? IBUS_KEY_Kana_Shift?
};

const struct ModifierKeyMap {
  guint from;
  mozc::commands::KeyEvent::ModifierKey to;
} modifier_key_map[] = {
  {IBUS_Shift_L, mozc::commands::KeyEvent::SHIFT},
  {IBUS_Shift_R, mozc::commands::KeyEvent::SHIFT},
  {IBUS_Control_L, mozc::commands::KeyEvent::CTRL},
  {IBUS_Control_R, mozc::commands::KeyEvent::CTRL},
  {IBUS_Alt_L, mozc::commands::KeyEvent::ALT},
  {IBUS_Alt_R, mozc::commands::KeyEvent::ALT},
};

const struct ModifierMaskMap {
  guint from;
  mozc::commands::KeyEvent::ModifierKey to;
} modifier_mask_map[] = {
  {IBUS_SHIFT_MASK, mozc::commands::KeyEvent::SHIFT},
  {IBUS_CONTROL_MASK, mozc::commands::KeyEvent::CTRL},
  {IBUS_MOD1_MASK, mozc::commands::KeyEvent::ALT},
};

}  // namespace

namespace mozc {
namespace ibus {

KeyTranslator::KeyTranslator() {
  Init();
}

KeyTranslator::~KeyTranslator() {
}

// TODO(mazda): Support Kana input
bool KeyTranslator::Translate(guint keyval,
                              guint keycode,
                              guint modifiers,
                              mozc::commands::KeyEvent *out_event) const {
  DCHECK(out_event) << "out_event is NULL";
  out_event->Clear();

  if (IsAscii(keyval, keycode, modifiers)) {
    out_event->set_key_code(keyval);
  } else if (IsModifierKey(keyval, keycode, modifiers)) {
    ModifierKeyMap::const_iterator i = modifier_key_map_.find(keyval);
    DCHECK(i != modifier_key_map_.end());
    out_event->add_modifier_keys((*i).second);
  } else if (IsSpecialKey(keyval, keycode, modifiers)) {
    SpecialKeyMap::const_iterator i = special_key_map_.find(keyval);
    DCHECK(i != special_key_map_.end());
    out_event->set_special_key((*i).second);
  } else {
    VLOG(1) << "Unknown keyval: " << keyval;
    return false;
  }

  for (ModifierKeyMap::const_iterator i = modifier_mask_map_.begin();
       i != modifier_mask_map_.end();
       ++i) {
    // Do not set a SHIFT modifier when |keyval| is a printable key by following
    // the Mozc's rule.
    if (((*i).second == commands::KeyEvent::SHIFT) &&
        IsAscii(keyval, keycode, modifiers)) {
      continue;
    }

    if ((*i).first & modifiers) {
      out_event->add_modifier_keys((*i).second);
    }
  }

  return true;
}

void KeyTranslator::Init() {
  for (int i = 0; i < arraysize(special_key_map); ++i) {
    CHECK(special_key_map_.insert(make_pair(special_key_map[i].from,
                                            special_key_map[i].to)).second);
  }
  for (int i = 0; i < arraysize(modifier_key_map); ++i) {
    CHECK(modifier_key_map_.insert(make_pair(modifier_key_map[i].from,
                                             modifier_key_map[i].to)).second);
  }
  for (int i = 0; i < arraysize(modifier_mask_map); ++i) {
    CHECK(modifier_mask_map_.insert(make_pair(modifier_mask_map[i].from,
                                              modifier_mask_map[i].to)).second);
  }
}

bool KeyTranslator::IsModifierKey(guint keyval,
                                  guint keycode,
                                  guint modifiers) const {
  return modifier_key_map_.find(keyval) != modifier_key_map_.end();
}

bool KeyTranslator::IsSpecialKey(guint keyval,
                                 guint keycode,
                                 guint modifiers) const {
  return special_key_map_.find(keyval) != special_key_map_.end();
}

bool KeyTranslator::IsAscii(guint keyval,
                            guint keycode,
                            guint modifiers) {
  return (keyval > IBUS_space &&
          // Note: Space key (0x20) is a special key in Mozc.
          keyval <= IBUS_asciitilde);  // 0x7e.
}

}  // namespace ibus
}  // namespace mozc

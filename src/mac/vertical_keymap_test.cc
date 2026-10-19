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

#include "mac/vertical_keymap.h"

#include "protocol/commands.pb.h"
#include "testing/gunit.h"

namespace mozc::mac {
namespace {

using commands::KeyEvent;

TEST(VerticalKeymapTest, CandidatesAndSegmentsFollowVerticalLayout) {
  constexpr KeyEvent::SpecialKey input[] = {KeyEvent::LEFT, KeyEvent::RIGHT,
                                            KeyEvent::UP, KeyEvent::DOWN};
  constexpr KeyEvent::SpecialKey expected[] = {KeyEvent::DOWN, KeyEvent::UP,
                                               KeyEvent::LEFT, KeyEvent::RIGHT};
  for (int i = 0; i < 4; ++i) {
    KeyEvent key;
    key.set_special_key(input[i]);
    RotateConversionArrows(true, true, &key);
    EXPECT_EQ(key.special_key(), expected[i]);
  }
}

TEST(VerticalKeymapTest, HorizontalAndOrdinaryCompositionAreUnchanged) {
  for (bool vertical : {false, true}) {
    for (bool converting : {false, true}) {
      if (vertical && converting) continue;
      for (auto arrow :
           {KeyEvent::LEFT, KeyEvent::RIGHT, KeyEvent::UP, KeyEvent::DOWN}) {
        KeyEvent key;
        key.set_special_key(arrow);
        RotateConversionArrows(vertical, converting, &key);
        EXPECT_EQ(key.special_key(), arrow);
      }
    }
  }
}

TEST(VerticalKeymapTest, ModifiedArrowsAndOtherKeysAreUnchanged) {
  for (auto modifier : {KeyEvent::SHIFT, KeyEvent::CTRL, KeyEvent::ALT}) {
    for (auto arrow :
         {KeyEvent::LEFT, KeyEvent::RIGHT, KeyEvent::UP, KeyEvent::DOWN}) {
      KeyEvent key;
      key.set_special_key(arrow);
      key.add_modifier_keys(modifier);
      const auto original = key.SerializeAsString();
      RotateConversionArrows(true, true, &key);
      EXPECT_EQ(key.SerializeAsString(), original);
    }
  }
  for (auto special : {KeyEvent::PAGE_UP, KeyEvent::PAGE_DOWN, KeyEvent::SPACE,
                       KeyEvent::ENTER}) {
    KeyEvent key;
    key.set_special_key(special);
    RotateConversionArrows(true, true, &key);
    EXPECT_EQ(key.special_key(), special);
  }
}

TEST(VerticalKeymapTest, CapsLockDoesNotChangeArrowDirections) {
  KeyEvent key;
  key.set_special_key(KeyEvent::LEFT);
  key.add_modifier_keys(KeyEvent::CAPS);
  RotateConversionArrows(true, true, &key);
  EXPECT_EQ(key.special_key(), KeyEvent::DOWN);
  ASSERT_EQ(key.modifier_keys_size(), 1);
  EXPECT_EQ(key.modifier_keys(0), KeyEvent::CAPS);
}

}  // namespace
}  // namespace mozc::mac

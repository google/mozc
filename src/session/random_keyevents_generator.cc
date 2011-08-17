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

#include "session/random_keyevents_generator.h"

#include "base/base.h"
#include "base/mutex.h"
#include "base/util.h"

namespace mozc {
namespace session {
namespace {

#include "session/session_stress_test_data.h"

const commands::KeyEvent::SpecialKey kSpecialKeys[] = {
  commands::KeyEvent::SPACE,
  commands::KeyEvent::BACKSPACE,
  commands::KeyEvent::DEL,
  commands::KeyEvent::DOWN,
  commands::KeyEvent::END,
  commands::KeyEvent::ENTER,
  commands::KeyEvent::ESCAPE,
  commands::KeyEvent::HOME,
  commands::KeyEvent::INSERT,
  commands::KeyEvent::HENKAN,
  commands::KeyEvent::MUHENKAN,
  commands::KeyEvent::LEFT,
  commands::KeyEvent::RIGHT,
  commands::KeyEvent::UP,
  commands::KeyEvent::DOWN,
  commands::KeyEvent::PAGE_UP,
  commands::KeyEvent::PAGE_DOWN,
  commands::KeyEvent::TAB,
  commands::KeyEvent::F1,
  commands::KeyEvent::F2,
  commands::KeyEvent::F3,
  commands::KeyEvent::F4,
  commands::KeyEvent::F5,
  commands::KeyEvent::F6,
  commands::KeyEvent::F7,
  commands::KeyEvent::F8,
  commands::KeyEvent::F9,
  commands::KeyEvent::F10,
  commands::KeyEvent::F11,
  commands::KeyEvent::F12,
  commands::KeyEvent::NUMPAD0,
  commands::KeyEvent::NUMPAD1,
  commands::KeyEvent::NUMPAD2,
  commands::KeyEvent::NUMPAD3,
  commands::KeyEvent::NUMPAD4,
  commands::KeyEvent::NUMPAD5,
  commands::KeyEvent::NUMPAD6,
  commands::KeyEvent::NUMPAD7,
  commands::KeyEvent::NUMPAD8,
  commands::KeyEvent::NUMPAD9,
  commands::KeyEvent::MULTIPLY,
  commands::KeyEvent::ADD,
  commands::KeyEvent::SEPARATOR,
  commands::KeyEvent::SUBTRACT,
  commands::KeyEvent::DECIMAL,
  commands::KeyEvent::DIVIDE,
  commands::KeyEvent::EQUALS
};

int GetRandom(int size) {
  return static_cast<int> (1.0 * size * rand() / (RAND_MAX + 1.0));
}

uint32 GetRandomAsciiKey() {
  return static_cast<uint32>(' ') + GetRandom(static_cast<uint32>('~' - ' '));
}

void InitSeed() {
  uint32 seed = 0;
  mozc::Util::GetSecureRandomSequence(reinterpret_cast<char *>(&seed),
                                      sizeof(seed));
  srand(seed);
}

once_t seed_init_once = MOZC_ONCE_INIT;
}  // namespace

void RandomKeyEventsGenerator::PrepareForMemoryLeakTest() {
  // Read all kTestSentences and load these to memory.
  const int size = arraysize(kTestSentences);
  for (int i = 0; i < size; ++i) {
    const char *sentence = kTestSentences[i];
    CHECK_GT(strlen(sentence), 0);
  }
}

void RandomKeyEventsGenerator::GenerateSequence(
    vector<commands::KeyEvent> *keys) {
  CHECK(keys);
  keys->clear();

  CallOnce(&seed_init_once, &InitSeed);

  const string sentence = kTestSentences[GetRandom(arraysize(kTestSentences))];
  CHECK(!sentence.empty());

  string tmp, input;
  Util::HiraganaToRomanji(sentence, &tmp);
  Util::FullWidthToHalfWidth(tmp, &input);

  VLOG(1) << input;

  // Must send ON event first.
  {
    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ON);
    keys->push_back(key);
  }

  vector<commands::KeyEvent> basic_keys;

  // generate basic input
  {
    // first add a normal keys
    const char *begin = input.data();
    const char *end = input.data() + input.size();
    while (begin < end) {
      size_t mblen = 0;
      const uint16 ucs2 = Util::UTF8ToUCS2(begin, end, &mblen);
      CHECK_GT(mblen, 0);
      begin += mblen;
      if (ucs2 >= 0x20 && ucs2 <= 0x7F) {
        commands::KeyEvent key;
        key.set_key_code(ucs2);
        basic_keys.push_back(key);
      }
    }
  }

  // basic keys + conversion
  {
    for (size_t i = 0; i < basic_keys.size(); ++i) {
      keys->push_back(basic_keys[i]);
    }

    for (int i = 0; i < 5; ++i) {
      const size_t num = GetRandom(30) + 8;
      for (size_t j = 0; j < num; ++j) {
        commands::KeyEvent key;
        key.set_special_key(commands::KeyEvent::SPACE);
        if (GetRandom(4) == 0) {
          key.add_modifier_keys(commands::KeyEvent::SHIFT);
          keys->push_back(key);
        }
      }
      commands::KeyEvent key;
      key.set_special_key(commands::KeyEvent::RIGHT);
      keys->push_back(key);
    }

    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ENTER);
    keys->push_back(key);
  }

  // segment resize
  {
    for (size_t i = 0; i < basic_keys.size(); ++i) {
      keys->push_back(basic_keys[i]);
    }

    const size_t num = GetRandom(30) + 10;
    for (size_t i = 0; i < num; ++i) {
      commands::KeyEvent key;
      switch (GetRandom(4)) {
        case 0:
          key.set_special_key(commands::KeyEvent::LEFT);
          if (GetRandom(2) == 0) {
            key.add_modifier_keys(commands::KeyEvent::SHIFT);
          }
          break;
        case 1:
          key.set_special_key(commands::KeyEvent::RIGHT);
          if (GetRandom(2) == 0) {
            key.add_modifier_keys(commands::KeyEvent::SHIFT);
          }
          break;
        default:
          {
            const size_t space_num = GetRandom(20) + 3;
            for (size_t i = 0; i < space_num; ++i) {
              key.set_special_key(commands::KeyEvent::SPACE);
              keys->push_back(key);
            }
          }
          break;
      }

      if (GetRandom(4) == 0) {
        key.add_modifier_keys(commands::KeyEvent::CTRL);
      }

      if (GetRandom(10) == 0) {
        key.add_modifier_keys(commands::KeyEvent::ALT);
      }

      keys->push_back(key);
    }

    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ENTER);
    keys->push_back(key);
  }

  // insert + delete
  {
    for (size_t i = 0; i < basic_keys.size(); ++i) {
      keys->push_back(basic_keys[i]);
    }

    const size_t num = GetRandom(20) + 10;
    for (size_t i = 0; i < num; ++i) {
      commands::KeyEvent key;
      switch (GetRandom(5)) {
        case 0:
          key.set_special_key(commands::KeyEvent::LEFT);
          break;
        case 1:
          key.set_special_key(commands::KeyEvent::RIGHT);
          break;
        case 2:
          key.set_special_key(commands::KeyEvent::DEL);
          break;
        case 3:
          key.set_special_key(commands::KeyEvent::BACKSPACE);
          break;
        default:
          {
            // add any ascii
            const size_t insert_num = GetRandom(5) + 1;
            for (size_t i = 0; i < insert_num; ++i) {
              key.set_key_code(GetRandomAsciiKey());
            }
          }
      }
      keys->push_back(key);
    }

    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ENTER);
    keys->push_back(key);
  }

  // basic keys + modifiers
  {
    for (size_t i = 0; i < basic_keys.size(); ++i) {
      commands::KeyEvent key;
      switch (GetRandom(8)) {
        case 0:
          key.set_key_code(kSpecialKeys[GetRandom(arraysize(kSpecialKeys))]);
          break;
        case 1:
          key.set_key_code(GetRandomAsciiKey());
          break;
        default:
          key.CopyFrom(basic_keys[i]);
          break;
      }

      if (GetRandom(10) == 0) {  // 10%
        key.add_modifier_keys(commands::KeyEvent::CTRL);
      }

      if (GetRandom(10) == 0) {  // 10%
        key.add_modifier_keys(commands::KeyEvent::SHIFT);
      }

      if (GetRandom(50) == 0) {  // 2%
        key.add_modifier_keys(commands::KeyEvent::KEY_DOWN);
      }

      if (GetRandom(50) == 0) {  // 2%
        key.add_modifier_keys(commands::KeyEvent::KEY_UP);
      }

      keys->push_back(key);
    }

    // submit
    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ENTER);
    keys->push_back(key);
  }

  CHECK_GT(keys->size(), 0);
  VLOG(1) << "key sequence is generated: " << keys->size();
}

// static
const char **RandomKeyEventsGenerator::GetTestSentences(size_t *size) {
  *size = arraysize(kTestSentences);
  return kTestSentences;
}
}  // namespace session
}  // namespace mozc

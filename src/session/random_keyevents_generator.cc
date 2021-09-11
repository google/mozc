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

#include "session/random_keyevents_generator.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/mutex.h"
#include "base/port.h"
#include "base/util.h"
#include "protocol/commands.pb.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace session {
namespace {

#include "session/session_stress_test_data.h"

// Constants for ProbableKeyEvent.
constexpr double kMostPossibleKeyProbability = 0.98;
constexpr size_t kProbableKeyEventSize = 8;

const commands::KeyEvent::SpecialKey kSpecialKeys[] = {
    commands::KeyEvent::SPACE,     commands::KeyEvent::BACKSPACE,
    commands::KeyEvent::DEL,       commands::KeyEvent::DOWN,
    commands::KeyEvent::END,       commands::KeyEvent::ENTER,
    commands::KeyEvent::ESCAPE,    commands::KeyEvent::HOME,
    commands::KeyEvent::INSERT,    commands::KeyEvent::HENKAN,
    commands::KeyEvent::MUHENKAN,  commands::KeyEvent::LEFT,
    commands::KeyEvent::RIGHT,     commands::KeyEvent::UP,
    commands::KeyEvent::DOWN,      commands::KeyEvent::PAGE_UP,
    commands::KeyEvent::PAGE_DOWN, commands::KeyEvent::TAB,
    commands::KeyEvent::F1,        commands::KeyEvent::F2,
    commands::KeyEvent::F3,        commands::KeyEvent::F4,
    commands::KeyEvent::F5,        commands::KeyEvent::F6,
    commands::KeyEvent::F7,        commands::KeyEvent::F8,
    commands::KeyEvent::F9,        commands::KeyEvent::F10,
    commands::KeyEvent::F11,       commands::KeyEvent::F12,
    commands::KeyEvent::NUMPAD0,   commands::KeyEvent::NUMPAD1,
    commands::KeyEvent::NUMPAD2,   commands::KeyEvent::NUMPAD3,
    commands::KeyEvent::NUMPAD4,   commands::KeyEvent::NUMPAD5,
    commands::KeyEvent::NUMPAD6,   commands::KeyEvent::NUMPAD7,
    commands::KeyEvent::NUMPAD8,   commands::KeyEvent::NUMPAD9,
    commands::KeyEvent::MULTIPLY,  commands::KeyEvent::ADD,
    commands::KeyEvent::SEPARATOR, commands::KeyEvent::SUBTRACT,
    commands::KeyEvent::DECIMAL,   commands::KeyEvent::DIVIDE,
    commands::KeyEvent::EQUALS,    commands::KeyEvent::COMMA,
};

uint32_t GetRandomAsciiKey() {
  return static_cast<uint32_t>(' ') +
         Util::Random(static_cast<uint32_t>('~' - ' '));
}

void InitSeedWithRandomValue() {
  uint32_t seed = 0;
  mozc::Util::GetRandomSequence(reinterpret_cast<char *>(&seed), sizeof(seed));
  Util::SetRandomSeed(seed);
}

void DoNothing() {
  // Do nothing.
  // Used only for marking the seed initialized.
}

once_t seed_init_once = MOZC_ONCE_INIT;
}  // namespace

void RandomKeyEventsGenerator::PrepareForMemoryLeakTest() {
  // Read all kTestSentences and load these to memory.
  const int size = std::size(kTestSentences);
  for (int i = 0; i < size; ++i) {
    const char *sentence = kTestSentences[i];
    CHECK_GT(strlen(sentence), 0);
  }
}

// Generates KeyEvent instances based on |romaji| and stores into |keys|.
void TypeRawKeys(absl::string_view romaji, bool create_probable_key_events,
                 std::vector<commands::KeyEvent> *keys) {
  for (ConstChar32Iterator iter(romaji); !iter.Done(); iter.Next()) {
    const uint32_t ucs4 = iter.Get();
    if (ucs4 < 0x20 || ucs4 > 0x7F) {
      continue;
    }
    commands::KeyEvent key;
    key.set_key_code(ucs4);
    if (create_probable_key_events) {
      commands::KeyEvent::ProbableKeyEvent *probable_key_event =
          key.add_probable_key_event();
      probable_key_event->set_key_code(ucs4);
      probable_key_event->set_probability(kMostPossibleKeyProbability);
      for (size_t i = 0; i < kProbableKeyEventSize; ++i) {
        commands::KeyEvent::ProbableKeyEvent *probable_key_event =
            key.add_probable_key_event();
        probable_key_event->set_key_code(0x20 + Util::Random(0x7F - 0x20));
        probable_key_event->set_probability(
            (1.0 - kMostPossibleKeyProbability) / kProbableKeyEventSize);
      }
    }
    keys->push_back(key);
  }
}

// Converts from Hiragana to Romaji.
std::string ToRomaji(absl::string_view hiragana) {
  std::string tmp, result;
  Util::HiraganaToRomanji(hiragana, &tmp);
  Util::FullWidthToHalfWidth(tmp, &result);
  return result;
}

void RandomKeyEventsGenerator::InitSeed(uint32_t seed) {
  Util::SetRandomSeed(seed);
  CallOnce(&seed_init_once, &DoNothing);
}

// Generates KeyEvent instances based on |sentence| and stores into |keys|.
// And Enter key event is appended at the tail.
// The instances have ProbableKeyEvent if |create_probable_key_events| is set.
void GenerateMobileSequenceInternal(absl::string_view sentence,
                                    bool create_probable_key_events,
                                    std::vector<commands::KeyEvent> *keys) {
  const std::string input = ToRomaji(sentence);
  VLOG(1) << input;

  // Type the sentence
  TypeRawKeys(input, create_probable_key_events, keys);

  commands::KeyEvent key;
  key.set_special_key(commands::KeyEvent::ENTER);
  keys->push_back(key);
}

void RandomKeyEventsGenerator::GenerateMobileSequence(
    bool create_probable_key_events, std::vector<commands::KeyEvent> *keys) {
  CHECK(keys);
  keys->clear();

  // If seed was not initialized, set seed randomly.
  CallOnce(&seed_init_once, &InitSeedWithRandomValue);

  const absl::string_view sentence(
      kTestSentences[Util::Random(std::size(kTestSentences))]);
  CHECK(!sentence.empty());
  for (size_t i = 0; i < sentence.size();) {
    // To simulate mobile key events, split given sentence into smaller parts.
    // Average 5, Min 1, Max 15
    const size_t len = Util::Random(5) + Util::Random(5) + Util::Random(5);
    GenerateMobileSequenceInternal(absl::ClippedSubstr(sentence, i, len),
                                   create_probable_key_events, keys);
    i += len;
  }
}

void RandomKeyEventsGenerator::GenerateSequence(
    std::vector<commands::KeyEvent> *keys) {
  CHECK(keys);
  keys->clear();

  // If seed was not initialized, set seed randomly.
  CallOnce(&seed_init_once, &InitSeedWithRandomValue);

  const std::string sentence =
      kTestSentences[Util::Random(std::size(kTestSentences))];
  CHECK(!sentence.empty());

  const std::string input = ToRomaji(sentence);

  VLOG(1) << input;

  // Must send ON event first.
  {
    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ON);
    keys->push_back(key);
  }

  std::vector<commands::KeyEvent> basic_keys;

  // generate basic input
  TypeRawKeys(input, false, &basic_keys);

  // basic keys + conversion
  {
    for (size_t i = 0; i < basic_keys.size(); ++i) {
      keys->push_back(basic_keys[i]);
    }

    for (int i = 0; i < 5; ++i) {
      const size_t num = Util::Random(30) + 8;
      for (size_t j = 0; j < num; ++j) {
        commands::KeyEvent key;
        key.set_special_key(commands::KeyEvent::SPACE);
        if (Util::Random(4) == 0) {
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

    const size_t num = Util::Random(30) + 10;
    for (size_t i = 0; i < num; ++i) {
      commands::KeyEvent key;
      switch (Util::Random(4)) {
        case 0:
          key.set_special_key(commands::KeyEvent::LEFT);
          if (Util::Random(2) == 0) {
            key.add_modifier_keys(commands::KeyEvent::SHIFT);
          }
          break;
        case 1:
          key.set_special_key(commands::KeyEvent::RIGHT);
          if (Util::Random(2) == 0) {
            key.add_modifier_keys(commands::KeyEvent::SHIFT);
          }
          break;
        default: {
          const size_t space_num = Util::Random(20) + 3;
          for (size_t i = 0; i < space_num; ++i) {
            key.set_special_key(commands::KeyEvent::SPACE);
            keys->push_back(key);
          }
        } break;
      }

      if (Util::Random(4) == 0) {
        key.add_modifier_keys(commands::KeyEvent::CTRL);
      }

      if (Util::Random(10) == 0) {
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

    const size_t num = Util::Random(20) + 10;
    for (size_t i = 0; i < num; ++i) {
      commands::KeyEvent key;
      switch (Util::Random(5)) {
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
        default: {
          // add any ascii
          const size_t insert_num = Util::Random(5) + 1;
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
      switch (Util::Random(8)) {
        case 0:
          key.set_key_code(kSpecialKeys[Util::Random(std::size(kSpecialKeys))]);
          break;
        case 1:
          key.set_key_code(GetRandomAsciiKey());
          break;
        default:
          key.CopyFrom(basic_keys[i]);
          break;
      }

      if (Util::Random(10) == 0) {  // 10%
        key.add_modifier_keys(commands::KeyEvent::CTRL);
      }

      if (Util::Random(10) == 0) {  // 10%
        key.add_modifier_keys(commands::KeyEvent::SHIFT);
      }

      if (Util::Random(50) == 0) {  // 2%
        key.add_modifier_keys(commands::KeyEvent::KEY_DOWN);
      }

      if (Util::Random(50) == 0) {  // 2%
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
  *size = std::size(kTestSentences);
  return kTestSentences;
}
}  // namespace session
}  // namespace mozc

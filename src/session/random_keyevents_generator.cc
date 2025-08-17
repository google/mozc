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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/japanese_util.h"
#include "base/util.h"
#include "base/vlog.h"
#include "protocol/commands.pb.h"
#include "session/session_stress_test_data.h"

namespace mozc {
namespace session {
namespace {

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

}  // namespace

void RandomKeyEventsGenerator::PrepareForMemoryLeakTest() {
  // Read all kTestSentences and load these to memory.
  const int size = std::size(kTestSentences);
  for (int i = 0; i < size; ++i) {
    const absl::string_view sentence = kTestSentences[i];
    CHECK(!sentence.empty());
  }
}

// Generates KeyEvent instances based on |romaji| and stores into |keys|.
void RandomKeyEventsGenerator::TypeRawKeys(
    absl::string_view romaji, bool create_probable_key_events,
    std::vector<commands::KeyEvent>* keys) {
  for (ConstChar32Iterator iter(romaji); !iter.Done(); iter.Next()) {
    const uint32_t codepoint = iter.Get();
    if (codepoint < 0x20 || codepoint > 0x7F) {
      continue;
    }
    commands::KeyEvent key;
    key.set_key_code(codepoint);
    if (create_probable_key_events) {
      commands::KeyEvent::ProbableKeyEvent* probable_key_event =
          key.add_probable_key_event();
      probable_key_event->set_key_code(codepoint);
      probable_key_event->set_probability(kMostPossibleKeyProbability);
      for (size_t i = 0; i < kProbableKeyEventSize; ++i) {
        commands::KeyEvent::ProbableKeyEvent* probable_key_event =
            key.add_probable_key_event();
        probable_key_event->set_key_code(absl::Uniform(bitgen_, 0x20, 0x7F));
        probable_key_event->set_probability(
            (1.0 - kMostPossibleKeyProbability) / kProbableKeyEventSize);
      }
    }
    keys->push_back(std::move(key));
  }
}

// Converts from Hiragana to Romaji.
std::string ToRomaji(absl::string_view hiragana) {
  std::string tmp = japanese_util::HiraganaToRomanji(hiragana);
  return japanese_util::FullWidthToHalfWidth(tmp);
}

// Generates KeyEvent instances based on |sentence| and stores into |keys|.
// And Enter key event is appended at the tail.
// The instances have ProbableKeyEvent if |create_probable_key_events| is set.
void RandomKeyEventsGenerator::GenerateMobileSequenceInternal(
    absl::string_view sentence, bool create_probable_key_events,
    std::vector<commands::KeyEvent>* keys) {
  const std::string input = ToRomaji(sentence);
  MOZC_VLOG(1) << input;

  // Type the sentence
  TypeRawKeys(input, create_probable_key_events, keys);

  commands::KeyEvent key;
  key.set_special_key(commands::KeyEvent::ENTER);
  keys->push_back(std::move(key));
}

void RandomKeyEventsGenerator::GenerateMobileSequence(
    bool create_probable_key_events, std::vector<commands::KeyEvent>* keys) {
  CHECK(keys);
  keys->clear();

  const absl::string_view sentence = kTestSentences[absl::Uniform<size_t>(
      bitgen_, 0, std::size(kTestSentences))];
  CHECK(!sentence.empty());
  for (size_t i = 0; i < sentence.size();) {
    // To simulate mobile key events, split given sentence into smaller parts.
    // Average 5, Min 1, Max 15.
    // absl::Poisson returns [0, max]. Add one and cap at 15.
    const size_t len =
        std::max<size_t>(absl::Poisson<size_t>(bitgen_, 4) + 1, 15);
    GenerateMobileSequenceInternal(absl::ClippedSubstr(sentence, i, len),
                                   create_probable_key_events, keys);
    i += len;
  }
}

void RandomKeyEventsGenerator::GenerateSequence(
    std::vector<commands::KeyEvent>* keys) {
  CHECK(keys);
  keys->clear();

  const absl::string_view sentence = kTestSentences[absl::Uniform<size_t>(
      bitgen_, 0, std::size(kTestSentences))];
  CHECK(!sentence.empty());

  const std::string input = ToRomaji(sentence);

  MOZC_VLOG(1) << input;

  // Must send ON event first.
  {
    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ON);
    keys->push_back(std::move(key));
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
      const size_t num = absl::Uniform(bitgen_, 8, 38);
      for (size_t j = 0; j < num; ++j) {
        if (absl::Bernoulli(bitgen_, 0.25)) {
          commands::KeyEvent key;
          key.set_special_key(commands::KeyEvent::SPACE);
          key.add_modifier_keys(commands::KeyEvent::SHIFT);
          keys->push_back(std::move(key));
        }
      }
      commands::KeyEvent key;
      key.set_special_key(commands::KeyEvent::RIGHT);
      keys->push_back(std::move(key));
    }

    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ENTER);
    keys->push_back(std::move(key));
  }

  // segment resize
  {
    for (size_t i = 0; i < basic_keys.size(); ++i) {
      keys->push_back(basic_keys[i]);
    }

    const size_t num = absl::Uniform<size_t>(bitgen_, 10, 40);
    for (size_t i = 0; i < num; ++i) {
      commands::KeyEvent key;
      switch (absl::Uniform(bitgen_, 0, 4)) {
        case 0:
          key.set_special_key(commands::KeyEvent::LEFT);
          if (absl::Bernoulli(bitgen_, 0.5)) {
            key.add_modifier_keys(commands::KeyEvent::SHIFT);
          }
          break;
        case 1:
          key.set_special_key(commands::KeyEvent::RIGHT);
          if (absl::Bernoulli(bitgen_, 0.5)) {
            key.add_modifier_keys(commands::KeyEvent::SHIFT);
          }
          break;
        default: {
          const size_t space_num = absl::Uniform(bitgen_, 3, 23);
          for (size_t i = 0; i < space_num; ++i) {
            key.set_special_key(commands::KeyEvent::SPACE);
            keys->push_back(key);
          }
        } break;
      }

      if (absl::Bernoulli(bitgen_, 0.25)) {
        key.add_modifier_keys(commands::KeyEvent::CTRL);
      }

      if (absl::Bernoulli(bitgen_, 0.1)) {
        key.add_modifier_keys(commands::KeyEvent::ALT);
      }

      keys->push_back(std::move(key));
    }

    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ENTER);
    keys->push_back(std::move(key));
  }

  // insert + delete
  {
    for (size_t i = 0; i < basic_keys.size(); ++i) {
      keys->push_back(basic_keys[i]);
    }

    const size_t num = absl::Uniform(bitgen_, 10, 30);
    for (size_t i = 0; i < num; ++i) {
      commands::KeyEvent key;
      switch (absl::Uniform(bitgen_, 0, 5)) {
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
          const size_t insert_num = absl::Uniform<size_t>(bitgen_, 1, 6);
          for (size_t i = 0; i < insert_num; ++i) {
            key.set_key_code(GetRandomAsciiKey());
          }
        }
      }
      keys->push_back(std::move(key));
    }

    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ENTER);
    keys->push_back(std::move(key));
  }

  // basic keys + modifiers
  {
    for (size_t i = 0; i < basic_keys.size(); ++i) {
      commands::KeyEvent key;
      switch (absl::Uniform(bitgen_, 0, 8)) {
        case 0:
          key.set_key_code(kSpecialKeys[absl::Uniform<uint32_t>(
              bitgen_, 0, std::size(kSpecialKeys))]);
          break;
        case 1:
          key.set_key_code(GetRandomAsciiKey());
          break;
        default:
          key = basic_keys[i];
          break;
      }

      if (absl::Bernoulli(bitgen_, 0.1)) {  // 10%
        key.add_modifier_keys(commands::KeyEvent::CTRL);
      }

      if (absl::Bernoulli(bitgen_, 0.1)) {  // 10%
        key.add_modifier_keys(commands::KeyEvent::SHIFT);
      }

      if (absl::Bernoulli(bitgen_, 0.02)) {  // 2%
        key.add_modifier_keys(commands::KeyEvent::KEY_DOWN);
      }

      if (absl::Bernoulli(bitgen_, 0.02)) {  // 2%
        key.add_modifier_keys(commands::KeyEvent::KEY_UP);
      }

      keys->push_back(std::move(key));
    }

    // submit
    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ENTER);
    keys->push_back(std::move(key));
  }

  CHECK_GT(keys->size(), 0);
  MOZC_VLOG(1) << "key sequence is generated: " << keys->size();
}

uint32_t RandomKeyEventsGenerator::GetRandomAsciiKey() {
  return absl::Uniform<uint32_t>(bitgen_, ' ', '~');
}

// static
absl::Span<const char*> RandomKeyEventsGenerator::GetTestSentences() {
  return absl::Span<const char*>(kTestSentences);
}
}  // namespace session
}  // namespace mozc

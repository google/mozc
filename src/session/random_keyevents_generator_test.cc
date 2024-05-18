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

#include <map>
#include <vector>

#include "protocol/commands.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace session {
namespace {

using mozc::commands::KeyEvent;

// Some of the tests are not deterministic, but we repeat enough times so
// they highly likely succeed.
constexpr int kIterations = 1000;

// This is not a deterministic test.
TEST(RandomKeyEventsGeneratorTest, GenerateSequence) {
  RandomKeyEventsGenerator generator;
  generator.PrepareForMemoryLeakTest();

  std::vector<KeyEvent> keys;
  // Deterministic checks
  generator.GenerateSequence(&keys);
  EXPECT_FALSE(keys.empty());
  EXPECT_EQ(keys.front().special_key(), KeyEvent::ON);
  EXPECT_EQ(keys.back().special_key(), KeyEvent::ENTER);

  // Non-deterministic checks
  std::map<KeyEvent::SpecialKey, int> special_keys;
  bool modifiers = false;
  for (int i = 0; i < kIterations; ++i) {
    keys.clear();
    generator.GenerateSequence(&keys);
    for (const auto& k : keys) {
      special_keys[k.special_key()]++;
      modifiers |= !k.modifier_keys().empty();
    }
  }
  // The following conditions are highly likely true given the number of
  // iterations.
  EXPECT_TRUE(modifiers);
  EXPECT_GT(special_keys[KeyEvent::BACKSPACE], 0);
  EXPECT_GT(special_keys[KeyEvent::LEFT], 0);
  EXPECT_GT(special_keys[KeyEvent::RIGHT], 0);
  EXPECT_GT(special_keys[KeyEvent::DEL], 0);
}

// This is not a deterministic test.
TEST(RandomKeyEventsGeneratorTest, GenerateMobileSequence) {
  RandomKeyEventsGenerator generator;
  generator.PrepareForMemoryLeakTest();

  // Deterministic checks
  std::vector<mozc::commands::KeyEvent> keys;
  generator.GenerateMobileSequence(false, &keys);
  EXPECT_FALSE(keys.empty());
  EXPECT_EQ(keys.back().special_key(), KeyEvent::ENTER);
}

TEST(RandomKeyEventsGeneratorTest, GenerateMobileSequenceProbable) {
  RandomKeyEventsGenerator generator;
  generator.PrepareForMemoryLeakTest();

  std::vector<mozc::commands::KeyEvent> keys;
  // Deterministic checks
  generator.GenerateMobileSequence(true, &keys);
  EXPECT_FALSE(keys.empty());
  EXPECT_EQ(keys.back().special_key(), KeyEvent::ENTER);
  for (auto it = keys.begin(); it != keys.end() - 1; ++it) {
    if (!it->probable_key_event().empty()) {
      for (const auto& p : it->probable_key_event()) {
        EXPECT_GT(p.probability(), 0);
        EXPECT_LT(p.probability(), 1);
      }
    }
  }
}

}  // namespace
}  // namespace session
}  // namespace mozc

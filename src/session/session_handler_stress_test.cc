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

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/file_util.h"
#include "base/port.h"
#include "engine/engine_factory.h"
#include "protocol/commands.pb.h"
#include "session/random_keyevents_generator.h"
#include "session/session_handler.h"
#include "session/session_handler_tool.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/flags/flag.h"

namespace {
uint32_t GenerateRandomSeed() {
  uint32_t seed = 0;
  mozc::Util::GetRandomSequence(reinterpret_cast<char *>(&seed), sizeof(seed));
  return seed;
}
}  // namespace

// There is no DEFINE_uint32.
ABSL_FLAG(uint64_t, random_seed, GenerateRandomSeed(),
          "Random seed value. "
          "This value will be interpreted as uint32.");

namespace mozc {
namespace {

using session::SessionHandlerTool;

TEST(SessionHandlerStressTest, BasicStressTest) {
  std::vector<commands::KeyEvent> keys;
  commands::Output output;
  std::unique_ptr<Engine> engine = EngineFactory::Create().value();
  SessionHandlerTool client(std::move(engine));
  size_t keyevents_size = 0;
  constexpr size_t kMaxEventSize = 2500;
  ASSERT_TRUE(client.CreateSession());

  const uint32_t random_seed =
      static_cast<uint32_t>(absl::GetFlag(FLAGS_random_seed));
  LOG(INFO) << "Random seed: " << random_seed;
  session::RandomKeyEventsGenerator::InitSeed(random_seed);
  while (keyevents_size < kMaxEventSize) {
    keys.clear();
    session::RandomKeyEventsGenerator::GenerateSequence(&keys);
    for (size_t i = 0; i < keys.size(); ++i) {
      ++keyevents_size;
      EXPECT_TRUE(client.TestSendKey(keys[i], &output));
      EXPECT_TRUE(client.SendKey(keys[i], &output));
    }
  }
  EXPECT_TRUE(client.DeleteSession());
}

}  // namespace
}  // namespace mozc

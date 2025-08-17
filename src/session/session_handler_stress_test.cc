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

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "engine/engine_factory.h"
#include "protocol/commands.pb.h"
#include "request/request_test_util.h"
#include "session/random_keyevents_generator.h"
#include "session/session_handler_tool.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

ABSL_FLAG(std::optional<uint32_t>, random_seed, std::nullopt,
          "Random seed value. This value will be interpreted as uint32_t.");
ABSL_FLAG(bool, set_mobile_request, false,
          "If true, set commands::Request to the mobine one.");

namespace mozc::session {
namespace {

constexpr size_t kTotalEventSize = 2500;

constexpr size_t kShardCount = 32;
constexpr size_t kEventsPerShard = kTotalEventSize / kShardCount;

class SessionHandlerStressTest
    : public ::mozc::testing::TestWithTempUserProfile,
      public ::testing::WithParamInterface<int> {
 protected:
  SessionHandlerStressTest() : client_(EngineFactory::Create().value()) {
    if (absl::GetFlag(FLAGS_random_seed).has_value()) {
      const uint32_t random_seed =
          absl::GetFlag(FLAGS_random_seed).value() + GetParam();
      LOG(INFO) << "Random seed: " << random_seed;
      generator_ = RandomKeyEventsGenerator(std::seed_seq{random_seed});
    }
  }

  void SetMobileRequest() {
    if (absl::GetFlag(FLAGS_set_mobile_request)) {
      commands::Output output;
      commands::Request request;
      request_test_util::FillMobileRequest(&request);
      client_.SetRequest(request, &output);
    }
  }

  SessionHandlerTool client_;
  RandomKeyEventsGenerator generator_;
};

TEST_P(SessionHandlerStressTest, BasicStressTest) {
  std::vector<commands::KeyEvent> keys;
  commands::Output output;
  size_t keyevents_size = 0;

  ASSERT_TRUE(client_.CreateSession());
  SetMobileRequest();

  while (keyevents_size < kEventsPerShard) {
    keys.clear();
    generator_.GenerateSequence(&keys);
    for (const commands::KeyEvent& key : keys) {
      ++keyevents_size;
      EXPECT_TRUE(client_.TestSendKey(key, &output));
      EXPECT_TRUE(client_.SendKey(key, &output));
    }
  }
  EXPECT_TRUE(client_.DeleteSession());
}

template <typename T, T... I>
std::array<T, sizeof...(I)> ToArray(std::integer_sequence<T, I...> sequence) {
  return {{I...}};
}

INSTANTIATE_TEST_SUITE_P(Shards, SessionHandlerStressTest,
                         ::testing::ValuesIn(ToArray(
                             std::make_integer_sequence<int, kShardCount>{})));

}  // namespace
}  // namespace mozc::session

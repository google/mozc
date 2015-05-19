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

#include <algorithm>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "session/japanese_session_factory.h"
#include "session/random_keyevents_generator.h"
#include "session/session_handler.h"
#include "session/session_handler_test_util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

using mozc::session::testing::TestSessionClient;

using mozc::session::testing::JapaneseSessionHandlerTestBase;
class SessionHandlerStressTest : public JapaneseSessionHandlerTestBase {
};

TEST_F(SessionHandlerStressTest, BasicStressTest) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  // TOOD(all): Add a test for the case where
  // use_realtime_conversion is true.
  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);

  vector<mozc::commands::KeyEvent> keys;
  mozc::commands::Output output;
  TestSessionClient client;
  size_t keyevents_size = 0;
  const size_t kMaxEventSize = 50000;
  ASSERT_TRUE(client.CreateSession());
  while (keyevents_size < kMaxEventSize) {
    keys.clear();
    mozc::session::RandomKeyEventsGenerator::GenerateSequence(&keys);
    for (size_t i = 0; i < keys.size(); ++i) {
      ++keyevents_size;
      EXPECT_TRUE(client.TestSendKey(keys[i], &output));
      EXPECT_TRUE(client.SendKey(keys[i], &output));
    }
  }
  EXPECT_TRUE(client.DeleteSession());
}

}  // namespae mozc

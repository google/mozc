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

#include <string>
#include <vector>
#include "base/base.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "session/random_keyevents_generator.h"
#include "session/session_handler.h"

DECLARE_string(FLAGS_test_tmpdir);

namespace mozc {
namespace {

class TestSessionClient {
 public:
  TestSessionClient()
      : id_(0), handler_(new SessionHandler) {}
  virtual ~TestSessionClient() {}

  bool CreateSession() {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
    if (!handler_->EvalCommand(&command)) {
      return false;
    }
    id_ = command.output().id();
    return true;
  }

  bool DeleteSession() {
    commands::Command command;
    command.mutable_input()->set_id(id_);
    command.mutable_input()->set_type(commands::Input::DELETE_SESSION);
    return handler_->EvalCommand(&command);
  }
  bool SendKey(const commands::KeyEvent &key,
               commands::Output *output) {
    commands::Input input;
    input.set_type(commands::Input::SEND_KEY);
    input.mutable_key()->CopyFrom(key);
    return Call(&input, output);
  }

  bool TestSendKey(const commands::KeyEvent &key,
                   commands::Output *output) {
    commands::Input input;
    input.set_type(commands::Input::TEST_SEND_KEY);
    input.mutable_key()->CopyFrom(key);
    return Call(&input, output);
  }

 private:
  bool Call(commands::Input *input,
            commands::Output *output) {
    input->set_id(id_);
    output->set_id(0);
    commands::Command command;
    command.mutable_input()->CopyFrom(*input);
    if (!handler_->EvalCommand(&command)) {
      return false;
    }
    output->CopyFrom(command.output());
    return true;
  }

  uint64 id_;
  scoped_ptr<SessionHandler> handler_;
};

}   // namespace
}  // namespae mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  mozc::config::Config config;
  mozc::config::ConfigHandler::GetDefaultConfig(&config);
  // TOOD(all): Add a test for the case where
  // use_realtime_conversion is true.
  config.set_use_realtime_conversion(false);
  mozc::config::ConfigHandler::SetConfig(config);

  mozc::session::RandomKeyEventsGenerator::PrepareForMemoryLeakTest();

  vector<mozc::commands::KeyEvent> keys;
  mozc::commands::Output output;
  mozc::TestSessionClient client;
  size_t keyevents_size = 0;
  const size_t kMaxEventSize = 10000000;
  client.CreateSession();
  while (keyevents_size < kMaxEventSize) {
    keys.clear();
    mozc::session::RandomKeyEventsGenerator::GenerateSequence(&keys);
    for (size_t i = 0; i < keys.size(); ++i) {
      ++keyevents_size;
      client.TestSendKey(keys[i], &output);
      client.SendKey(keys[i], &output);
    }
  }
  client.DeleteSession();

  return 0;
}

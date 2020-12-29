// Copyright 2010-2020, Google Inc.
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

#include "session/session_observer_handler.h"
#include "protocol/commands.pb.h"
#include "session/session_observer_interface.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace session {

class SessionObserverMock : public SessionObserverInterface {
 public:
  SessionObserverMock() : eval_count_(0) {}
  ~SessionObserverMock() override {}

  void EvalCommandHandler(const commands::Command &command) override {
    command_.CopyFrom(command);
    ++eval_count_;
  }

  int eval_count() const { return eval_count_; }

  const commands::Command &command() const { return command_; }

 private:
  int eval_count_;
  commands::Command command_;
  DISALLOW_COPY_AND_ASSIGN(SessionObserverMock);
};

TEST(SessionObserverHandlerTest, ObserverTest) {
  SessionObserverHandler handler;
  SessionObserverMock observer1;
  SessionObserverMock observer2;
  handler.AddObserver(&observer1);
  handler.AddObserver(&observer2);

  // Round1: EvalCommandHandler
  commands::Command command;
  command.mutable_input()->mutable_key()->set_key_code('a');
  command.mutable_output()->set_consumed(true);
  handler.EvalCommandHandler(command);

  EXPECT_EQ(1, observer1.eval_count());
  EXPECT_EQ('a', observer1.command().input().key().key_code());
  EXPECT_TRUE(observer1.command().output().consumed());

  EXPECT_EQ(1, observer2.eval_count());
  EXPECT_EQ('a', observer2.command().input().key().key_code());
  EXPECT_TRUE(observer2.command().output().consumed());

  // Round2: EvalCommandHandler
  command.Clear();
  command.mutable_input()->mutable_key()->set_key_code('z');
  command.mutable_output()->set_consumed(false);
  handler.EvalCommandHandler(command);

  EXPECT_EQ(2, observer1.eval_count());
  EXPECT_EQ('z', observer1.command().input().key().key_code());
  EXPECT_FALSE(observer1.command().output().consumed());

  EXPECT_EQ(2, observer2.eval_count());
  EXPECT_EQ('z', observer2.command().input().key().key_code());
  EXPECT_FALSE(observer2.command().output().consumed());
}

}  // namespace session
}  // namespace mozc

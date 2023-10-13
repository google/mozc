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

#include "session/session_observer_handler.h"

#include "protocol/commands.pb.h"
#include "session/session_observer_interface.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace session {
namespace {

using ::testing::AllOf;

class SessionObserverMock : public SessionObserverInterface {
 public:
  MOCK_METHOD(void, EvalCommandHandler, (const commands::Command &command),
              (override));
};

MATCHER_P(KeyCode, value, "") { return arg.input().key().key_code() == value; }
MATCHER_P(Consumed, value, "") { return arg.output().consumed() == value; }

struct ObserverTestParam {
  bool consumed;
  char key_code;
};

class SessionObserverHandlerTest
    : public ::testing::TestWithParam<ObserverTestParam> {};

TEST_P(SessionObserverHandlerTest, ObserverTest) {
  SessionObserverHandler handler;
  SessionObserverMock observer1;
  SessionObserverMock observer2;
  handler.AddObserver(&observer1);
  handler.AddObserver(&observer2);

  EXPECT_CALL(observer1,
              EvalCommandHandler(AllOf(KeyCode(GetParam().key_code),
                                       Consumed(GetParam().consumed))))
      .Times(1);
  EXPECT_CALL(observer2,
              EvalCommandHandler(AllOf(KeyCode(GetParam().key_code),
                                       Consumed(GetParam().consumed))))
      .Times(1);

  commands::Command command;
  command.mutable_input()->mutable_key()->set_key_code(GetParam().key_code);
  command.mutable_output()->set_consumed(GetParam().consumed);
  handler.EvalCommandHandler(command);
}

INSTANTIATE_TEST_SUITE_P(Observer, SessionObserverHandlerTest,
                         testing::Values(ObserverTestParam{true, 'a'},
                                         ObserverTestParam{false, 'z'}));

}  // namespace
}  // namespace session
}  // namespace mozc

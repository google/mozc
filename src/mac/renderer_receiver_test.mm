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

#import "mac/renderer_receiver.h"

#include "protocol/commands.pb.h"
#include "testing/gunit.h"

class RendererReceiverTest : public testing::Test {
 protected:
  void SetUp() {
    // Initialize RendererReceiver with init rather than initWithName.
    // This is because the test does not use renderer_connection_.
    _receiver = [[RendererReceiver alloc] init];
  }

  RendererReceiver *_receiver;
};

@interface MockController : NSObject <ControllerCallback> {
  int _numSendCommand;
  mozc::commands::SessionCommand _receivedSessionCommand;
  int _numOutputResult;
  mozc::commands::Output _receivedOutput;
}
@property(readonly) int numSendCommand;
@property(readonly) mozc::commands::SessionCommand receivedSessionCommand;
@property(readonly) int numOutputResult;
@property(readonly) mozc::commands::Output receivedOutput;
@end

@implementation MockController

- (void)sendCommand:(const mozc::commands::SessionCommand &)command {
  _receivedSessionCommand = command;
  ++_numSendCommand;
}

- (void)outputResult:(const mozc::commands::Output &)output {
  _receivedOutput = output;
  ++_numOutputResult;
}
@end

TEST_F(RendererReceiverTest, sendData) {
  MockController *controller = [[MockController alloc] init];
  [_receiver setCurrentController:controller];

  mozc::commands::SessionCommand command;
  command.Clear();
  command.set_type(mozc::commands::SessionCommand::SELECT_CANDIDATE);
  command.set_id(0);

  std::string commandData = command.SerializeAsString();
  [_receiver sendData:[NSData dataWithBytes:commandData.data() length:commandData.size()]];
  EXPECT_EQ(controller.numSendCommand, 1);
  EXPECT_EQ(controller.receivedSessionCommand.DebugString(), command.DebugString());
}

TEST_F(RendererReceiverTest, outputResult) {
  MockController *controller = [[MockController alloc] init];
  [_receiver setCurrentController:controller];

  mozc::commands::Output output;
  output.Clear();
  output.mutable_result()->set_type(mozc::commands::Result::STRING);
  output.mutable_result()->set_key("foobar");
  output.mutable_result()->set_value("baz");

  std::string outputData = output.SerializeAsString();
  [_receiver outputResult:[NSData dataWithBytes:outputData.data() length:outputData.size()]];
  EXPECT_EQ(controller.numOutputResult, 1);
  EXPECT_EQ(controller.receivedOutput.DebugString(), output.DebugString());
}

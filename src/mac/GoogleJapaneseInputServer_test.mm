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

#import "mac/GoogleJapaneseInputServer.h"

#include "protocol/commands.pb.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

class GoogleJapaneseInputServerTest : public testing::Test {
 protected:
  void SetUp() {
    // Although GoogleJapaneseInputServer is a subclass of IMKServer,
    // it does not use initWithName:... method to instantiate the
    // object because we don't test those IMKServer functionality
    // during this test.
    server_ = [[GoogleJapaneseInputServer alloc] init];
  }

 protected:
  GoogleJapaneseInputServer *server_;
};

@interface MockController : NSObject<ControllerCallback> {
  int numSendData_;
  mozc::commands::SessionCommand *expectedCommand_;
  int numOutputResult_;
  mozc::commands::Output *expectedData_;
}
@property(readonly) int numSendData;
@property(readwrite, assign) mozc::commands::SessionCommand *expectedCommand;
@property(readonly) int numOutputResult;
@property(readwrite, assign) mozc::commands::Output *expectedData;

- (void)sendCommand:(mozc::commands::SessionCommand &)command;
- (void)outputResult:(mozc::commands::Output *)data;
@end

@implementation MockController
@synthesize numSendData = numSendData_;
@synthesize expectedCommand = expectedCommand_;
@synthesize numOutputResult = numOutputResult_;
@synthesize expectedData = expectedData_;

- (void)sendCommand:(mozc::commands::SessionCommand &)command {
  ASSERT_NE((void*)0, expectedCommand_);
  EXPECT_EQ(expectedCommand_->DebugString(), command.DebugString());
  ++numSendData_;
}

- (void)outputResult:(mozc::commands::Output *)data {
  ASSERT_NE((void*)0, data);
  ASSERT_NE((void*)0, expectedData_);
  EXPECT_EQ(expectedData_->DebugString(), data->DebugString());
  ++numOutputResult_;
}
@end

TEST_F(GoogleJapaneseInputServerTest, sendData) {
  MockController *controller = [[MockController alloc] init];
  [server_ setCurrentController:controller];

  mozc::commands::SessionCommand command;
  command.Clear();
  command.set_type(mozc::commands::SessionCommand::SELECT_CANDIDATE);
  command.set_id(0);
  controller.expectedCommand = &command;

  string commandData = command.SerializeAsString();
  [server_ sendData:[NSData dataWithBytes:commandData.data()
                                   length:commandData.size()]];
  EXPECT_EQ(1, controller.numSendData);
}

TEST_F(GoogleJapaneseInputServerTest, outputResult) {
  MockController *controller = [[MockController alloc] init];
  [server_ setCurrentController:controller];

  mozc::commands::Output output;
  output.Clear();
  output.mutable_result()->set_type(mozc::commands::Result::STRING);
  output.mutable_result()->set_key("foobar");
  output.mutable_result()->set_value("baz");
  controller.expectedData = &output;

  string outputData = output.SerializeAsString();
  [server_ outputResult:[NSData dataWithBytes:outputData.data()
                                       length:outputData.size()]];
  EXPECT_EQ(1, controller.numOutputResult);
}

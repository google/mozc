// Copyright 2010, Google Inc.
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

#include "base/scoped_ptr.h"
#include "testing/base/public/gunit.h"
#include "unix/ibus/session.h"

namespace mozc {
namespace ibus {

class SessionTest : public testing::Test {
 protected:
  SessionTest() {}

  virtual void SetUp() {
    session_.reset(new Session);
  }

  scoped_ptr<Session> session_;
};

TEST_F(SessionTest, EnsureSession) {
  EXPECT_TRUE(session_->EnsureSession());
}

TEST_F(SessionTest, EnsureConnection) {
  EXPECT_TRUE(session_->EnsureConnection());
}

TEST_F(SessionTest, CheckVersionOrRestartServer) {
  EXPECT_TRUE(session_->CheckVersionOrRestartServer());
}

TEST_F(SessionTest, SendKey) {
  commands::KeyEvent key_event;
  key_event.set_key_code('a');

  commands::Output output;
  EXPECT_TRUE(session_->TestSendKey(key_event, &output));
  EXPECT_NE(0, output.id());
  EXPECT_TRUE(output.consumed());
}

TEST_F(SessionTest, TestSendKey) {
  commands::KeyEvent key_event;
  key_event.set_key_code('a');
  commands::Output output;
  EXPECT_TRUE(session_->SendKey(key_event, &output));
  EXPECT_NE(0, output.id());
  EXPECT_TRUE(output.consumed());

  key_event.Clear();
  key_event.set_special_key(commands::KeyEvent::ENTER);
  EXPECT_TRUE(session_->SendKey(key_event, &output));
  EXPECT_NE(0, output.id());
  EXPECT_TRUE(output.consumed());
}

TEST_F(SessionTest, SendCommand) {
  commands::KeyEvent key_event;
  key_event.set_key_code('a');
  commands::Output output;
  EXPECT_TRUE(session_->SendKey(key_event, &output));
  EXPECT_TRUE(output.has_preedit());

  commands::SessionCommand session_command;
  session_command.set_type(commands::SessionCommand::SUBMIT);
  EXPECT_TRUE(session_->SendCommand(session_command, &output));
  EXPECT_FALSE(output.has_preedit());
}

// Disabled bacause the corresponding function is not implemented yet.
TEST_F(SessionTest, DISABLED_GetConfig) {
}

// Disabled bacause the corresponding function is not implemented yet.
TEST_F(SessionTest, DISABLED_SetConfig) {
}

TEST_F(SessionTest, ClearUserHistory) {
  EXPECT_TRUE(session_->ClearUserHistory());
}

TEST_F(SessionTest, ClearUserPrediction) {
  EXPECT_TRUE(session_->ClearUserPrediction());
}

TEST_F(SessionTest, ClearUnusedUserPrediction) {
  EXPECT_TRUE(session_->ClearUnusedUserPrediction());
}

TEST_F(SessionTest, Shutdown) {
  EXPECT_TRUE(session_->Shutdown());
}

TEST_F(SessionTest, SyncData) {
  EXPECT_TRUE(session_->SyncData());
}

TEST_F(SessionTest, Reload) {
  EXPECT_TRUE(session_->Reload());
}

TEST_F(SessionTest, Cleanup) {
  EXPECT_TRUE(session_->Cleanup());
}

TEST_F(SessionTest, NoOperation) {
  EXPECT_TRUE(session_->NoOperation());
}

TEST_F(SessionTest, PingServer) {
  EXPECT_TRUE(session_->PingServer());
}

// Disabled bacause the corresponding function is not implemented yet.
TEST_F(SessionTest, DISABLED_Reset) {
}

// Disabled bacause the corresponding function is not implemented yet.
TEST_F(SessionTest, DISABLED_EnableCascadingWindow) {
}

TEST_F(SessionTest, set_timeout) {
  // Checks this function does not crash at least.
  session_->set_timeout(1);
}

}  // namespace ibus
}  // namespace mozc

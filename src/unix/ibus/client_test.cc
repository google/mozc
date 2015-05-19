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

#include "unix/ibus/client.h"

#include "base/base.h"
#include "base/scoped_ptr.h"
#include "client/client.h"
#ifdef OS_CHROMEOS
#include "session/japanese_session_factory.h"
#include "session/session_factory_manager.h"
#endif
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace ibus {

class ClientEnvironment : public testing::Environment {
 public:
  virtual void SetUp() {
    session_factory_.reset(new mozc::session::JapaneseSessionFactory);
    mozc::session::SessionFactoryManager::SetSessionFactory(
        session_factory_.get());
  }

 private:
  scoped_ptr<mozc::session::JapaneseSessionFactory> session_factory_;
};

class ClientTest : public testing::Test {
 protected:
  ClientTest() {}

  virtual void SetUp() {
#ifdef OS_CHROMEOS
    client_.reset(new ibus::Client);
#else
    client_.reset(client::ClientFactory::NewClient());
#endif
  }

  scoped_ptr<client::ClientInterface> client_;
};

TEST_F(ClientTest, EnsureSession) {
  EXPECT_TRUE(client_->EnsureSession());
}

TEST_F(ClientTest, EnsureConnection) {
  EXPECT_TRUE(client_->EnsureConnection());
}

TEST_F(ClientTest, CheckVersionOrRestartServer) {
  EXPECT_TRUE(client_->CheckVersionOrRestartServer());
}

TEST_F(ClientTest, SendKey) {
  commands::KeyEvent key_event;
  key_event.set_key_code('a');

  commands::Output output;
  EXPECT_TRUE(client_->TestSendKey(key_event, &output));
  EXPECT_NE(0, output.id());
  EXPECT_TRUE(output.consumed());
}

TEST_F(ClientTest, TestSendKey) {
  commands::KeyEvent key_event;
  key_event.set_key_code('a');
  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_NE(0, output.id());
  EXPECT_TRUE(output.consumed());

  key_event.Clear();
  key_event.set_special_key(commands::KeyEvent::ENTER);
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_NE(0, output.id());
  EXPECT_TRUE(output.consumed());
}

TEST_F(ClientTest, SendCommand) {
  commands::KeyEvent key_event;
  key_event.set_key_code('a');
  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_TRUE(output.has_preedit());

  commands::SessionCommand session_command;
  session_command.set_type(commands::SessionCommand::SUBMIT);
  EXPECT_TRUE(client_->SendCommand(session_command, &output));
  EXPECT_FALSE(output.has_preedit());
}

// Disabled bacause the corresponding function is not implemented yet.
TEST_F(ClientTest, DISABLED_GetConfig) {
}

// Disabled bacause the corresponding function is not implemented yet.
TEST_F(ClientTest, DISABLED_SetConfig) {
}

TEST_F(ClientTest, ClearUserHistory) {
  EXPECT_TRUE(client_->ClearUserHistory());
}

TEST_F(ClientTest, ClearUserPrediction) {
  EXPECT_TRUE(client_->ClearUserPrediction());
}

TEST_F(ClientTest, ClearUnusedUserPrediction) {
  EXPECT_TRUE(client_->ClearUnusedUserPrediction());
}

TEST_F(ClientTest, Shutdown) {
#ifndef OS_CHROMEOS
  EXPECT_TRUE(client_->Shutdown());
#endif
}

TEST_F(ClientTest, SyncData) {
  EXPECT_TRUE(client_->SyncData());
}

TEST_F(ClientTest, Reload) {
  EXPECT_TRUE(client_->Reload());
}

TEST_F(ClientTest, Cleanup) {
  EXPECT_TRUE(client_->Cleanup());
}

TEST_F(ClientTest, NoOperation) {
  EXPECT_TRUE(client_->NoOperation());
}

TEST_F(ClientTest, PingServer) {
  EXPECT_TRUE(client_->PingServer());
}

// Disabled bacause the corresponding function is not implemented yet.
TEST_F(ClientTest, DISABLED_Reset) {
}

// Disabled bacause the corresponding function is not implemented yet.
TEST_F(ClientTest, DISABLED_EnableCascadingWindow) {
}

TEST_F(ClientTest, set_timeout) {
  // Checks this function does not crash at least.
  client_->set_timeout(1);
}

}  // namespace ibus
}  // namespace mozc


// The main function for this test.  We do not reuse the predefined
// main function of gtest_main to inject ClientEnvironment.  The gtest
// guide suggests to write its own main() function to use Environment.
// See: http://code.google.com/p/googletest/wiki/AdvancedGuide
int main(int argc, char **argv) {
  // TODO(yukawa, team): Implement b/2805528 so that you can specify any option
  // given by gunit.
  InitGoogle(argv[0], &argc, &argv, false);
  mozc::InitTestFlags();
  testing::InitGoogleTest(&argc, argv);

  // Without this flag, ::RaiseException makes the job stuck.
  // See b/2805521 for details.
  testing::GTEST_FLAG(catch_exceptions) = true;

  testing::AddGlobalTestEnvironment(new mozc::ibus::ClientEnvironment);
  return RUN_ALL_TESTS();
}

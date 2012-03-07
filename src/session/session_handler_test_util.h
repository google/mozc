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

// Utilities for testing related to sessions.

#ifndef IME_MOZC_SESSION_SESSION_HANDLER_TEST_UTIL_H_
#define IME_MOZC_SESSION_SESSION_HANDLER_TEST_UTIL_H_

#include <string>
#include "base/scoped_ptr.h"
#include "config/config.pb.h"
#include "session/commands.pb.h"
#include "session/japanese_session_factory.h"
#include "testing/base/public/gunit.h"

namespace mozc {

class SessionFactoryInterface;
class SessionHandlerInterface;

namespace session {
namespace testing {

// Sends CREATE_SESSION command to the given handler and returns its result.
// If it is successfully completed and the given id is not NULL,
// also stores the session id to it.
bool CreateSession(SessionHandlerInterface *handler, uint64 *id);

// Sends DELETE_SESSION command with the given id to the given handler,
// and returns its result.
bool DeleteSession(SessionHandlerInterface *handler, uint64 id);

// Sends CLEANUP command to the given handler, and returns its result.
bool CleanUp(SessionHandlerInterface *handler);

// Returns the session represented by the given id is "good" or not, based
// on sending a SPACE key. See the implementation for the detail.
bool IsGoodSession(SessionHandlerInterface *handler, uint64 id);

// Base implementation of test cases based on JapaneseSessionFactory.
class JapaneseSessionHandlerTestBase : public ::testing::Test {
 protected:
  virtual void SetUp();
  virtual void TearDown();

  // This class should not be instantiated directly.
  JapaneseSessionHandlerTestBase();
  ~JapaneseSessionHandlerTestBase();

 private:
  // Keep the global configurations here, and restore them in tear down phase.
  string user_profile_directory_backup_;
  config::Config config_backup_;
  SessionFactoryInterface *session_factory_backup_;

  JapaneseSessionFactory session_factory_;
};

// Session utility for stress tests.
class TestSessionClient {
 public:
  TestSessionClient();
  ~TestSessionClient();

  bool CreateSession();
  bool DeleteSession();
  bool CleanUp();
  bool SendKey(const commands::KeyEvent &key, commands::Output *output);
  bool TestSendKey(const commands::KeyEvent &key, commands::Output *output);

  bool ResetContext();
  bool UndoOrRewind(commands::Output *output);
  bool SwitchInputMode(commands::CompositionMode composition_mode);
  bool SetRequest(const commands::Request &request, commands::Output *output);

 private:
  bool EvalCommand(commands::Input *input, commands::Output *output);

  uint64 id_;
  scoped_ptr<SessionHandlerInterface> handler_;
};

}  // namespace testing
}  // namespace session
}  // namespace mozc

#endif  // IME_MOZC_SESSION_SESSION_HANDLER_TEST_UTIL_H_

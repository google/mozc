// Copyright 2010-2014, Google Inc.
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

#ifndef MOZC_SESSION_SESSION_HANDLER_TEST_UTIL_H_
#define MOZC_SESSION_SESSION_HANDLER_TEST_UTIL_H_

#include <string>
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "config/config.pb.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats_testing_util.h"

namespace mozc {

class EngineInterface;
class SessionHandlerInterface;

namespace session {
class SessionObserverInterface;

namespace testing {

// Sends CREATE_SESSION command to the given handler and returns its result.
// If it is successfully completed and the given id is not NULL,
// also stores the session id to it.
bool CreateSession(SessionHandlerInterface *handler, uint64 *id);

// Sends DELETE_SESSION command with the given id to the given handler,
// and returns its result.
bool DeleteSession(SessionHandlerInterface *handler, uint64 id);

// Sends CLEANUP command to the given handler, and returns its result.
bool CleanUp(SessionHandlerInterface *handler, uint64 id);

// Sends CLEAR_USER_PREDICTION command to the given handler and returns its
// result.
bool CleanUserPrediction(SessionHandlerInterface *handler, uint64 id);

// Returns the session represented by the given id is "good" or not, based
// on sending a SPACE key. See the implementation for the detail.
bool IsGoodSession(SessionHandlerInterface *handler, uint64 id);

// Base implementation of test cases.
class SessionHandlerTestBase : public ::testing::Test {
 protected:
  virtual void SetUp();
  virtual void TearDown();

  // This class should not be instantiated directly.
  SessionHandlerTestBase();
  virtual ~SessionHandlerTestBase();

  void ClearState();

 private:
  // Keep the global configurations here, and restore them in tear down phase.
  string user_profile_directory_backup_;
  config::Config config_backup_;
  int32 flags_max_session_size_backup_;
  int32 flags_create_session_min_interval_backup_;
  int32 flags_watch_dog_interval_backup_;
  int32 flags_last_command_timeout_backup_;
  int32 flags_last_create_session_timeout_backup_;
  bool flags_restricted_backup_;

  const usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;

  DISALLOW_COPY_AND_ASSIGN(SessionHandlerTestBase);
};

// Session utility for stress tests.
class TestSessionClient {
 public:
  // This class doesn't take an ownership of *engine.
  explicit TestSessionClient(EngineInterface *engine);
  ~TestSessionClient();

  bool CreateSession();
  bool DeleteSession();
  bool CleanUp();
  bool ClearUserPrediction();
  bool SendKey(const commands::KeyEvent &key, commands::Output *output) {
    return SendKeyWithOption(
        key, commands::Input::default_instance(), output);
  }
  bool SendKeyWithOption(const commands::KeyEvent &key,
                         const commands::Input &option,
                         commands::Output *output);
  bool TestSendKey(const commands::KeyEvent &key, commands::Output *output) {
    return TestSendKeyWithOption(
        key, commands::Input::default_instance(), output);
  }
  bool TestSendKeyWithOption(const commands::KeyEvent &key,
                             const commands::Input &option,
                             commands::Output *output);
  bool SelectCandidate(uint32 id, commands::Output *output);
  bool SubmitCandidate(uint32 id, commands::Output *output);

  bool Reload();
  bool ResetContext();
  bool UndoOrRewind(commands::Output *output);
  bool SwitchInputMode(commands::CompositionMode composition_mode);
  bool SetRequest(const commands::Request &request, commands::Output *output);
  void SetCallbackText(const string &text);

 private:
  bool EvalCommand(commands::Input *input, commands::Output *output);
  bool EvalCommandInternal(commands::Input *input, commands::Output *output,
                           bool allow_callback);

  uint64 id_;
  scoped_ptr<SessionObserverInterface> usage_observer_;
  scoped_ptr<SessionHandlerInterface> handler_;
  string callback_text_;

  DISALLOW_COPY_AND_ASSIGN(TestSessionClient);
};

}  // namespace testing
}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_HANDLER_TEST_UTIL_H_

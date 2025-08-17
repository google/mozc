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

// Utilities for testing related to sessions.

#ifndef MOZC_SESSION_SESSION_HANDLER_TEST_UTIL_H_
#define MOZC_SESSION_SESSION_HANDLER_TEST_UTIL_H_

#include <cstdint>

#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/session_handler.h"
#include "testing/mozctest.h"

namespace mozc {
namespace session {
namespace testing {

// Sends CREATE_SESSION command to the given handler and returns its result.
// If it is successfully completed and the given id is not NULL,
// also stores the session id to it.
bool CreateSession(SessionHandler* handler, uint64_t* id);

// Sends DELETE_SESSION command with the given id to the given handler,
// and returns its result.
bool DeleteSession(SessionHandler* handler, uint64_t id);

// Sends CLEANUP command to the given handler, and returns its result.
bool CleanUp(SessionHandler* handler, uint64_t id);

// Returns the session represented by the given id is "good" or not, based
// on sending a SPACE key. See the implementation for the detail.
bool IsGoodSession(SessionHandler* handler, uint64_t id);

// Base implementation of test cases.
class SessionHandlerTestBase : public ::mozc::testing::TestWithTempUserProfile {
 protected:
  void SetUp() override;
  void TearDown() override;

  virtual void ClearState();

 private:
  config::Config config_backup_;
  int32_t flags_max_session_size_backup_;
  int32_t flags_create_session_min_interval_backup_;
  int32_t flags_watch_dog_interval_backup_;
  int32_t flags_last_command_timeout_backup_;
  int32_t flags_last_create_session_timeout_backup_;
  bool flags_restricted_backup_;
};

}  // namespace testing
}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_HANDLER_TEST_UTIL_H_

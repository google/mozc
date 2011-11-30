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

// The Session class to wrap libchewing.

#ifndef MOZC_LANGUAGES_CHEWING_SESSION_H_
#define MOZC_LANGUAGES_CHEWING_SESSION_H_

#include <chewing.h>

#include "base/base.h"
#include "session/commands.pb.h"
#include "session/session_interface.h"

namespace mozc {
namespace chewing {
class Session: public mozc::session::SessionInterface {
 public:
  Session();
  ~Session();

  enum State {
    PRECOMPOSITION = 0,
    IN_CONVERSION = 1,
  };

  virtual bool SendKey(commands::Command *command);

  // Check if the input key event will be consumed by the session.
  virtual bool TestSendKey(commands::Command *command);

  // Perform the SEND_COMMAND command defined commands.proto.
  virtual bool SendCommand(commands::Command *command);

  virtual void ReloadConfig();

  // Set client capability for this session.  Used by unittest.
  virtual void set_client_capability(const commands::Capability &capability);

  // Set application information for this session.
  virtual void set_application_info(
      const commands::ApplicationInfo &application_info);

  // Get application information
  virtual const commands::ApplicationInfo &application_info() const;

  // Return the time when this instance was created.
  virtual uint64 create_session_time() const;

  // return 0 (default value) if no command is executed in this session.
  virtual uint64 last_command_time() const;

#ifdef OS_CHROMEOS
  // Update the config by bypassing session layer for Chrome OS
  static void UpdateConfig(const config::ChewingConfig &config);
#endif

 private:
  // Fill the candidates with the current context.
  void FillCandidates(commands::Candidates *candidates);

  // Fill the output with the current context.  This does not update
  // 'consumed' field.  The caller has the responsibility to fill it
  // before calling this method.
  void FillOutput(commands::Command *command);

  // Set configurations.
  void ResetConfig();

  // Throw the existing chewing context and create another one again
  // to clear context completely.
  void RenewContext();

  ChewingContext *context_;
  State state_;
  commands::ApplicationInfo application_info_;
  uint64 create_session_time_;
  uint64 last_command_time_;
  uint64 last_config_updated_;
};

}  // namespace session
}  // namespace mozc
#endif  // MOZC_LANGUAGES_CHEWING_SESSION_H_

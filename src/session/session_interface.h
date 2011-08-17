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

// The abstraction of Session class.

#ifndef MOZC_SESSION_SESSION_INTERFACE_H_
#define MOZC_SESSION_SESSION_INTERFACE_H_

#include "base/base.h"
#include "session/commands.pb.h"

namespace mozc {
namespace commands {
class Command;
}
namespace session {
class SessionInterface {
 public:
  virtual ~SessionInterface() {}

  virtual bool SendKey(commands::Command *command) ABSTRACT;

  // Check if the input key event will be consumed by the session.
  virtual bool TestSendKey(commands::Command *command) ABSTRACT;

  // Perform the SEND_COMMAND command defined commands.proto.
  virtual bool SendCommand(commands::Command *command) ABSTRACT;

  virtual void ReloadConfig() ABSTRACT;

  // Set client capability for this session.  Used by unittest.
  virtual void set_client_capability(
      const commands::Capability &capability) ABSTRACT;

  // Set application information for this session.
  virtual void set_application_info(const commands::ApplicationInfo
                                    &application_info) ABSTRACT;

  // Get application information
  virtual const commands::ApplicationInfo &application_info() const ABSTRACT;

  // Return the time when this instance was created.
  virtual uint64 create_session_time() const ABSTRACT;

  // return 0 (default value) if no command is executed in this session.
  virtual uint64 last_command_time() const ABSTRACT;

};

}  // namespace session
}  // namespace mozc
#endif  // MOZC_SESSION_SESSION_INTERFACE_H_

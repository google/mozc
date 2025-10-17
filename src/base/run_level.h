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

#ifndef MOZC_BASE_RUN_LEVEL_H_
#define MOZC_BASE_RUN_LEVEL_H_

namespace mozc {

class RunLevel {
 public:
  enum RunLevelType {
    NORMAL,      // normal process
    RESTRICTED,  // with timeout
    DENY,        // do not launch Mozc
    RUNLEVEL_TYPE_SIZE
  };

  // request type
  enum RequestType {
    CLIENT,    // user is client
    SERVER,    // user is server
    RENDERER,  // user is renderer
    REQUEST_TYPE_SIZE
  };

  RunLevel() = delete;

  // return the runlevel of current process
  // NOTE:
  // DO NOT USE logging library inside this method,
  // since GetRunLevel() is called  before mozc::InitMozc().
  // Logging stream and flags may not be initialized.
  // Also, make sure that the code inside this function
  // never raises any exceptions. Exception handler is installed
  // inside mozc::InitMozc().
  static RunLevelType GetRunLevel(RunLevel::RequestType type);

  static bool IsValidClientRunLevel() {
    return (GetRunLevel(RunLevel::CLIENT) <= RunLevel::RESTRICTED);
  }

  // return true the current process is elevated by UAC.
  // return false if the function failed to determine it.
  // return false on Mac/Linux
  static bool IsElevatedByUAC();

  // Disable Mozc on UAC-elevated process.
  // To enable Mozc, pass disable = false
  // return true if successfully saving to the flag into registry.
  // return false always on Linux/Mac
  static bool SetElevatedProcessDisabled(bool disable);

  // return true if mozc is not enabled on elevated process.
  // return false always on Linux/Mac.
  static bool GetElevatedProcessDisabled();
};
}  // namespace mozc
#endif  // MOZC_BASE_RUN_LEVEL_H_

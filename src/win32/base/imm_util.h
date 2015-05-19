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

#ifndef MOZC_WIN32_BASE_IMM_UTIL_H_
#define MOZC_WIN32_BASE_IMM_UTIL_H_

#include <windows.h>
#include <string>

#include "base/port.h"

namespace mozc {
namespace win32 {

class ImeUtil {
 public:
  // Returns true if Google Japanese Input is selected as the default IME.
  static bool IsDefault();

  // Sets Google Japanese Input as the default IME and update the runtime
  // keyboard setting of the current session.
  // Returns true if the operation completed successfully.
  // NOTE: This function internally calls LoadKeyboardLayout on Windows XP.
  // Loading the specified keyboard layout might result in loading
  // corresponding IME module (*.ime) into the current process, although I
  // have not tested. If this is true, you cannot freely call SetDefault()
  // in some special situations like CustomAction.
  static bool SetDefault();

  // Enables or disables CUAS (Cicero Unaware Application Support).
  // Returns true if the operation completed successfully.
  static bool SetCuasEnabled(bool enable);

  // Returns true if cfmon.exe is running.
  // ctfmon.exe is running if TSF (Text Service Framework) is enabled.
  static bool IsCtfmonRunning();

  // Activates the IMM32 version of Google Japanese Input for the current
  // process.
  // Returns true if the operation completed successfully.
  static bool ActivateForCurrentProcess();


  // Activates the IMM32 version of Google Japanese Input for all existing
  // applications running in the current session.
  // Returns true if the operation completed successfully.
  static bool ActivateForCurrentSession();

 private:
  DISALLOW_COPY_AND_ASSIGN(ImeUtil);
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_BASE_IMM_UTIL_H_

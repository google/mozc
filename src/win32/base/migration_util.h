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

#ifndef MOZC_WIN32_BASE_MIGRATION_UTIL_H_
#define MOZC_WIN32_BASE_MIGRATION_UTIL_H_

#include "base/port.h"

namespace mozc {
namespace win32 {

// Provides utility functions to migrate user's IME from the old Google Japanese
// Input implemented with TSF to the new one implemented with IMM32.
class MigrationUtil {
 public:
  // Checks if the IMM32 version is available.
  // Returns true if the operation completed successfully.
  static bool IsFullIMEAvailable();

  // Checks if the TSF version is available.
  // Returns true if the operation completed successfully.
  static bool IsFullTIPAvailable();

  // Ensures preload key for the new IME exists.
  // Returns true if the operation completed successfully.
  static bool RestorePreload();

  // Launches an external process called mozc_broker to set the IMM32 version
  // to the default IME for the current user.  This function calls
  // |config.set_check_default(false)| when and only when |do_not_ask_me_again|
  // is true and the default settings is successfully updated.  In other words,
  // |config.check_default()| remains as it was if this function fails to
  // change the default IME.
  // Returns true if the operation completed successfully.
  static bool LaunchBrokerForSetDefault(bool do_not_ask_me_again);

  // Returns true if 1) IMM32 Mozc is not installed, 2) IMM32 Mozc is
  // already disabled for the current user, or 3) IMM32 Mozc is successfully
  // disabled by this method.
  static bool DisableLegacyMozcForCurrentUserOnWin8();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MigrationUtil);
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_BASE_MIGRATION_UTIL_H_

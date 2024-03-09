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

#ifndef MOZC_WIN32_BASE_UNINSTALL_HELPER_H_
#define MOZC_WIN32_BASE_UNINSTALL_HELPER_H_

namespace mozc {
namespace win32 {

// This class is used to determine the new enabled layout/profile for the
// current user after Google Japanese Input is uninstalled.
class UninstallHelper {
 public:
  UninstallHelper(const UninstallHelper &) = delete;
  UninstallHelper &operator=(const UninstallHelper &) = delete;

  // Returns true if IME environment for the current user is successfully
  // cleaned up and restored.  This function ensures following things.
  //  - 'Preload' key maintains consistency with containing at least one
  //    valid (and hopefully meaningful) IME.
  //  - Default IME is set to valid (and hopefully meaningful) IME or TIP.
  //  - Existing application is redirected to the new default IME or TIP
  //    so that a user never see any broken state caused by uninstallation.
  // Please beware that this method touches HKCU hive especially when you
  // call this method from a custom action.
  static bool RestoreUserIMEEnvironmentMain();
};

}  // namespace win32
}  // namespace mozc
#endif  // MOZC_WIN32_BASE_UNINSTALL_HELPER_H_

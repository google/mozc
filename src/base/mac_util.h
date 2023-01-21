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

#ifndef MOZC_BASE_MAC_UTIL_H_
#define MOZC_BASE_MAC_UTIL_H_

#ifdef __APPLE__
#include <string>

#include "base/port.h"

namespace mozc {
class MacUtil {
 public:
  // Returns the label commonly used in the project for specified suffix.
  static std::string GetLabelForSuffix(const std::string &suffix);

  // Returns (basically) "~/Library/Application Support".
  static std::string GetApplicationSupportDirectory();

  // Returns (basically) "~/Library/Caches".
  static std::string GetCachesDirectory();

  // Returns (basically) ~/Library/Logs
  static std::string GetLoggingDirectory();

  // Returns OS version string like, "Version 10.x (Build xXxxx)".
  static std::string GetOSVersionString();

  // Returns server directory using OS-specific API.
  static std::string GetServerDirectory();

  // Returns the "Resources/" directory in the current application.
  static std::string GetResourcesDirectory();

  // Returns the machine serial number.
  static std::string GetSerialNumber();

#ifndef OS_IOS
  // Starts the specified service by using launchd.  "service_name" is
  // a suffix for the service label (like "Converter" or "Renderer").
  // If "pid" is non-null, it will store the pid of the launched
  // process in it.  Returns true if it successfully launches the
  // process.
  static bool StartLaunchdService(const std::string &service_name,
                                  pid_t *pid);

  // Checks if the prelauncher is set in "Login Item".
  static bool CheckPrelauncherLoginItemStatus();

  // Removes the prelauncher from "Login Item".
  static void RemovePrelauncherLoginItem();

  // Adds the prelauncher to "Login Item"
  static void AddPrelauncherLoginItem();

  // Gets the name and the owner name of the frontmost window.
  // Returns false if an error occurred.
  static bool GetFrontmostWindowNameAndOwner(std::string *name,
                                             std::string *owner);

  // Returns true when Mozc's suggestion UI is expected to be suppressed on
  // the window specified by |name| and |owner|.
  static bool IsSuppressSuggestionWindow(const std::string &name,
                                         const std::string &owner);
#endif  // !OS_IOS

 private:
  MacUtil() {}
  ~MacUtil() {}
};
}  // namespace mozc

#endif  // __APPLE__
#endif  // MOZC_BASE_MAC_UTIL_H_

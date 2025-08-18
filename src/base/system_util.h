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

#ifndef MOZC_BASE_SYSTEM_UTIL_H_
#define MOZC_BASE_SYSTEM_UTIL_H_

#include <cstdint>
#include <string>

namespace mozc {

// SystemUtil class supports utility methods which are related to OSes or user
// profiles.  For example,
//   - accessors for paths used in Mozc,
//   - checkers for platform profiles,
//   - command line flags manipulation,
// are supported.
// TODO(peria): Enable to replace SystemUtil class to be used in tests.
class SystemUtil {
 public:
  SystemUtil() = delete;
  SystemUtil(const SystemUtil&) = delete;
  SystemUtil& operator=(const SystemUtil&) = delete;
  // return "~/.mozc" for Unix/Mac
  // return "%USERPROFILE%\\AppData\\LocalLow\\"
  //        "Google\\Google Japanese Input" for Windows Vista and later.
  static std::string GetUserProfileDirectory();

  // return ~/Library/Logs/Mozc for Mac
  // Otherwise same as GetUserProfileDirectory().
  static std::string GetLoggingDirectory();

  // set user dir

  // Currently we enabled this method to release-build too because
  // - to support multi user for Android, it is necessary to inject
  //   user profile directory from the client layer.
  // - some tests use this.
  // TODO(mukai,taku): find better way to hide this method in the release
  // build but available from those tests.
  static void SetUserProfileDirectory(const std::string& path);

  // return the directory name where the mozc server exist.
  static std::string GetServerDirectory();

  // return the path of the mozc server.
  static std::string GetServerPath();

  // return the path of the mozc renderer.
  static std::string GetRendererPath();

  // return the path of the mozc tool.
  static std::string GetToolPath();

  // Returns the directory name which holds some documents bundled to
  // the installed application package.  Typically it's
  // <server directory>/documents but it can change among platforms.
  static std::string GetDocumentDirectory();

  // Returns the directory path crash dumps are stored.
  static std::string GetCrashReportDirectory();

  // return the username.  This function's name was GetUserName.
  // Since Windows reserves GetUserName as a macro, we have changed
  // the name to GetUserNameAsString.
  static std::string GetUserNameAsString();

  // return Windows SID as string.
  // On Linux and Mac, GetUserSidAsString() is equivalent to
  // GetUserNameAsString()
  static std::string GetUserSidAsString();

  // return DesktopName as string.
  // On Windows. return <session_id>.<DesktopStationName>.<ThreadDesktopName>
  // On Linux, return getenv("DISPLAY")
  // Mac has no DesktopName() so, just return empty string
  static std::string GetDesktopNameAsString();

#ifdef _WIN32
  // From an early stage of the development of Mozc, we have somehow abused
  // CHECK macro assuming that any failure of fundamental APIs like
  // ::SHGetFolderPathW or ::SHGetKnownFolderPathis is worth being notified
  // as a crash.  But the circumstances have been changed.  As filed as
  // b/3216603, increasing number of instances of various applications begin
  // to use their own sandbox technology, where these kind of fundamental APIs
  // are far more likely to fail with an unexpected error code.
  // EnsureVitalImmutableDataIsAvailable is a simple fail-fast mechanism to
  // this situation.  This function simply returns false instead of making
  // the process crash if any of following functions cannot work as expected.
  // - SystemDirectoryCache
  // - ProgramFilesX86Cache
  // - LocalAppDataDirectoryCache
  // TODO(taku,yukawa): Implement more robust and reliable mechanism against
  //   sandboxed environment, where such kind of fundamental APIs are far more
  //   likely to fail.  See b/3216603.
  static bool EnsureVitalImmutableDataIsAvailable();

  // return system directory. If failed, return nullptr.
  // You need not to delete the returned pointer.
  // This function is thread safe.
  static const wchar_t* GetSystemDir();
#endif  // _WIN32

  // return string representing os version
  // TODO(toshiyuki): Add unittests.
  static std::string GetOSVersionString();

  // disable IME in the current process/thread
  static void DisableIME();

  // retrieve total physical memory. returns 0 if any error occurs.
  static uint64_t GetTotalPhysicalMemory();
};

}  // namespace mozc

#endif  // MOZC_BASE_SYSTEM_UTIL_H_

// Copyright 2010-2013, Google Inc.
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

#include <string>

#include "base/port.h"

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
  // return "~/.mozc" for Unix/Mac
  // return "%USERPROFILE%\\Local Settings\\Application\\"
  //        "Google\\Google Japanese Input" for Windows XP.
  // return "%USERPROFILE%\\AppData\\LocalLow\\"
  //        "Google\\Google Japanese Input" for Windows Vista and later.
  static string GetUserProfileDirectory();

  // return ~/Library/Logs/Mozc for Mac
  // Otherwise same as GetUserProfileDirectory().
  static string GetLoggingDirectory();

  // set user dir

  // Currently we enabled this method to release-build too because
  // - to support multi user for Android, it is necessary to inject
  //   user profile directory from the client layer.
  // - some tests use this.
  // TODO(mukai,taku): find better way to hide this method in the release
  // build but available from those tests.
  static void SetUserProfileDirectory(const string &path);

  // return the directory name where the mozc server exist.
  static string GetServerDirectory();

  // return the path of the mozc server.
  static string GetServerPath();

  // return the path of the mozc renderer.
  static string GetRendererPath();

  // return the path of the mozc tool.
  static string GetToolPath();

  // Returns the directory name which holds some documents bundled to
  // the installed application package.  Typically it's
  // <server directory>/documents but it can change among platforms.
  static string GetDocumentDirectory();

  // return the username.  This function's name was GetUserName.
  // Since Windows reserves GetUserName as a macro, we have changed
  // the name to GetUserNameAsString.
  static string GetUserNameAsString();

  // return Windows SID as string.
  // On Linux and Mac, GetUserSidAsString() is equivalent to
  // GetUserNameAsString()
  static string GetUserSidAsString();


  // return DesktopName as string.
  // On Windows. return <session_id>.<DesktopStationName>.<ThreadDesktopName>
  // On Linux, return getenv("DISPLAY")
  // Mac has no DesktopName() so, just return empty string
  static string GetDesktopNameAsString();

#ifdef OS_WIN
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
  // - IsVistaOrLaterCache
  // - SystemDirectoryCache
  // - ProgramFilesX86Cache
  // - LocalAppDataDirectoryCache
  // TODO(taku,yukawa): Implement more robust and reliable mechanism against
  //   sandboxed environment, where such kind of fundamental APIs are far more
  //   likely to fail.  See b/3216603.
  static bool EnsureVitalImmutableDataIsAvailable();
#endif  // OS_WIN

  // Command line arguments

  // Rotate the first argv value to the end of argv.
  static void CommandLineRotateArguments(int argc, char ***argv);

  // Get a pair of key and value from argv, and returns the number of
  // arguments used for the pair of key and value.  If the argv
  // contains invalid format, this function returns false and the
  // number of checked arguments.  Otherwise returns true.
  static bool CommandLineGetFlag(int argc,
                                 char **argv,
                                 string *key,
                                 string *value,
                                 int *used_args);


  // Return true if the OS is supported.
  // [OS_MACOSX] This function never returns false.
  // [OS_LINUX] This function never returns false.
  // TODO(yukawa): support Mac and Linux.
  static bool IsPlatformSupported();

  // returns true if the version of Windows is Vista or later.
  static bool IsVistaOrLater();

  // returns true if the version of Windows is 6.1 or later.
  static bool IsWindows7OrLater();

  // returns true if the version of Windows is 6.2 or later.
  static bool IsWindows8OrLater();

  // returns true if the version of Windows is x64 Edition.
  static bool IsWindowsX64();

  enum IsWindowsX64Mode {
    IS_WINDOWS_X64_DEFAULT_MODE,
    IS_WINDOWS_X64_EMULATE_32BIT_MACHINE,
    IS_WINDOWS_X64_EMULATE_64BIT_MACHINE,
  };

  // For unit tests, this function overrides the behavior of |IsWindowsX64|.
  static void SetIsWindowsX64ModeForTest(IsWindowsX64Mode mode);

#ifdef OS_WIN
  // return system directory. If failed, return NULL.
  // You need not to delete the returned pointer.
  // This function is thread safe.
  static const wchar_t *GetSystemDir();

  // Retrieves version of the specified file.
  // If the function fails, returns false.
  static bool GetFileVersion(const wstring &file_fullpath,
                             int *major,
                             int *minor,
                             int *build,
                             int *revision);

  // Retrieves version string of the specified file.
  // The version string consists of 4 digits separated by comma
  // like "X.YY.ZZZ.WWWW".
  // If the function fails, the return value is an empty string.
  static string GetFileVersionString(const wstring &file_fullpath);

  // Returns "MSCTF.AsmCacheReady.<desktop name><session #>" to work around
  // b/5765783.
  // Returns an empty string if fails.
  // Currently this method is defined in util.h because it depends on some
  // utility functions defined in util.cc.
  // TODO(yukawa): Move this method to win32/base/*
  static string GetMSCTFAsmCacheReadyEventName();
#endif  // OS_WIN

  // return string representing os version
  // TODO(toshiyuki): Add unittests.
  static string GetOSVersionString();

  // returns true if platform is MacOSX and the version is acceptable.
  static bool MacOSVersionIsGreaterOrEqual(int32 major, int32 minor, int32 fix);

  // disable IME in the current process/thread
  static void DisableIME();

  // retrieve total physical memory. returns 0 if any error occurs.
  static uint64 GetTotalPhysicalMemory();

  // check endian-ness at runtime.
  static bool IsLittleEndian();

  // Following mlock/munlock related functions work based on target environment.
  // In the case of Android, Native Client, Windows, we don't want to call
  // actual functions, so these functions do nothing and return -1. In other
  // cases, these functions call actual mlock/munlock functions and return it's
  // result.
  // On Android, page-out is probably acceptable because
  // - Smaller RAM on the device.
  // - The storage is (usually) solid state thus page-in/out is expected to
  //   be faster.
  // On Linux, in the kernel version >= 2.6.9, user process can mlock. In older
  // kernel, it fails if the process is running in user priviledge.
  // TODO(horo): Check in mac that mlock is really necessary.
  static int MaybeMLock(const void *addr, size_t len);

  static int MaybeMUnlock(const void *addr, size_t len);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SystemUtil);
};

}  // namespace mozc

#endif  // MOZC_BASE_SYSTEM_UTIL_H_

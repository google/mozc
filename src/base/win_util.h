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

#ifndef MOZC_BASE_WIN_UTIL_H
#define MOZC_BASE_WIN_UTIL_H

#if defined(OS_WIN)
#include <windows.h>

#include <string>

#include "base/port.h"
#include "testing/base/public/gunit_prod.h"
// for FRIEND_TEST()

namespace mozc {
class WinUtil {
 public:
  // Load a DLL which has the specified base-name and is located in the
  // system directory.
  // If the function succeeds, the return value is a handle to the module.
  // You should call FreeLibrary with the handle.
  // If the function fails, the return value is NULL.
  static HMODULE LoadSystemLibrary(const wstring &base_filename);

  // Load a DLL which has the specified base-name and is located in the
  // Mozc server directory.
  // If the function succeeds, the return value is a handle to the module.
  // You should call FreeLibrary with the handle.
  // If the function fails, the return value is NULL.
  static HMODULE LoadMozcLibrary(const wstring &base_filename);

  // If a DLL which has the specified base-name and located in the system
  // directory is loaded in the caller process, retrieve its module handle.
  // If the function succeeds, the return value is a handle to the module
  // without incrementing its reference count so that you should not call
  // FreeLibrary with the handle.
  // If the function fails, the return value is NULL.
  static HMODULE GetSystemModuleHandle(const wstring &base_filename);

  // A variant ot GetSystemModuleHandle except that this method increments
  // reference count of the target DLL.
  static HMODULE GetSystemModuleHandleAndIncrementRefCount(
      const wstring &base_filename);

  // Retrieve whether the calling thread hold loader lock or not.
  // Return true if the state is retrieved successfuly.
  // Otherwise, the state of loader lock is unknown.
  // NOTE: |lock_held| may be false if the DLL is loaded as
  // implicit link.
  static bool IsDLLSynchronizationHeld(bool *lock_held);

  // Log off the current user.
  // Return true if the operation successfully finished.
  static bool Logoff();

  // Returns true if |lhs| and |rhs| are treated as the same string by the OS.
  // This function internally uses CompareStringOrdinal, or
  // RtlCompareUnicodeString as a fallback, or _wcsicmp_l with LANG "English"
  // as a final fallback, as Michael Kaplan recommended in his blog.
  // http://blogs.msdn.com/b/michkap/archive/2007/09/14/4900107.aspx
  // See the comment and implementation of Win32EqualString, NativeEqualString
  // and CrtEqualString for details.
  // Although this function ignores the rest part of given string when NUL
  // character is found, you should not pass such a string in principle.
  static bool SystemEqualString(
      const wstring &lhs, const wstring &rhs, bool ignore_case);

  // Returns true if succeeds to determine whether the current process has
  // a process token which seems to be one for service process.  Otherwise,
  // returns false.
  static bool IsServiceProcess(bool *is_service);

  // Returns true if succeeds to determine whether the current process has
  // a thread token which seems to be one for service thread.  Otherwise,
  // returns false.
  static bool IsServiceThread(bool *is_service);

  // This is a utility function to check IsServiceProcess and IsServiceThread
  // for the current process and thread.
  static bool IsServiceAccount(bool *is_service);

  // Returns true if succeeds to determine whether the |hToken| is one of the
  // known service or not.  Otherwise, returns false.
  static bool IsServiceUser(HANDLE hToken, bool *is_service);

  // Returns true if succeeds to determine whether the process specified by
  // |process_handle| is in immersive mode or not. Otherwise, returns false.
  static bool IsProcessImmersive(HANDLE process_handle, bool *is_immersive);

  // Returns true if succeeds to determine whether the process specified by
  // |process_handle| is running under AppContainer sandbox environment or not.
  // Otherwise, returns false.
  static bool IsProcessInAppContainer(HANDLE process_handle,
                                      bool *in_appcontainer);

  // Returns true if CUAS (Cicero Unaware Application Support) is enabled.
  // Note: This method was previously defined in win32/base/imm_util.h but
  // moved to here because UsateStats depends on this method.
  static bool IsCuasEnabled();

  // Returns true if |info| is filled with a valid file information that
  // describes |path|. |path| can be a directory or a file.
  static bool GetFileSystemInfoFromPath(const wstring &path,
                                        BY_HANDLE_FILE_INFORMATION *info);

  // Returns true if |left_path| and |right_path| are the same file system
  // object. This method takes hard-link into consideration.
  // Returns false if either |left_path| or |right_path| does not exist even
  // when |left_path| == |right_path|.
  static bool AreEqualFileSystemObject(const wstring &left_path,
                                       const wstring &right_path);

  // Returns true if the file or directory specified by |dos_path| exists and
  // its NT path is retrieved as |nt_path|. This function can work only on
  // Vista and later.
  static bool GetNtPath(const wstring &dos_path, wstring *nt_path);

  // Returns true if the process specified by |pid| exists and its *initial*
  // NT path is retrieved as |nt_path|. Note that even when the process path is
  // renamed after the process is launched, the *initial* path is retrieved.
  // This is important whem MSI changes paths of executables.
  static bool GetProcessInitialNtPath(DWORD pid, wstring *nt_path);

 private:
  // Compares |lhs| with |rhs| by CompareStringOrdinal and returns the result
  // in |are_equal|.  If |ignore_case| is true, this function uses system
  // upper-case table for case-insensitive equality like Win32 path names or
  // registry names.
  // Returns false if CompareStringOrdinal is not available.
  // http://msdn.microsoft.com/en-us/library/ff561854.aspx
  static bool Win32EqualString(const wstring &lhs, const wstring &rhs,
                               bool ignore_case, bool *are_equal);

  // Compares |lhs| with |rhs| by RtlEqualUnicodeString and returns the result
  // in |are_equal|.
  // Returns false if RtlEqualUnicodeString is not available.
  // http://msdn.microsoft.com/en-us/library/ff561854.aspx
  static bool NativeEqualString(const wstring &lhs, const wstring &rhs,
                                bool ignore_case, bool *are_equal);

  // Compares |lhs| with |rhs| by CRT functions and returns the result
  // in |are_equal|.  Note that this function internally uses _wcsicmp_l
  // with LANG "English" to check the case-insensitive equality if
  // |ignore_case| is true.  Unfortunately, _wcsicmp_l with LANG "English" is
  // not compatible with CompareStringOrdinal/RtlEqualUnicodeString in terms of
  // U+03C2 (GREEK SMALL LETTER FINAL SIGMA) and U+03A3 (GREEK CAPITAL LETTER
  // SIGMA).  See the following article for details.
  // http://blogs.msdn.com/b/michkap/archive/2005/05/26/421987.aspx
  static void CrtEqualString(const wstring &lhs, const wstring &rhs,
                             bool ignore_case, bool *are_equal);

  FRIEND_TEST(WinUtilTest, Win32EqualStringTest);
  FRIEND_TEST(WinUtilTest, NativeEqualStringTest);
  FRIEND_TEST(WinUtilTest, CrtEqualStringTest);

  DISALLOW_IMPLICIT_CONSTRUCTORS(WinUtil);
};

// Initializes COM in the constructor (STA), and uninitializes COM in the
// destructor.
class ScopedCOMInitializer {
 public:
  ScopedCOMInitializer();
  ScopedCOMInitializer::~ScopedCOMInitializer();

  // Returns the error code from CoInitialize(NULL)
  // (called in constructor)
  inline HRESULT error_code() const {
    return hr_;
  }

 protected:
  HRESULT hr_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedCOMInitializer);
};
}  // namespace mozc

#endif  // OS_WIN
#endif  // MOZC_BASE_WIN_UTIL_H

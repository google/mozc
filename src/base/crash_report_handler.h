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

#ifndef MOZC_BASE_WIN32_CRASH_REPORT_HANDLER_H_
#define MOZC_BASE_WIN32_CRASH_REPORT_HANDLER_H_

#ifdef OS_WINDOWS
#include <windows.h>
#endif  // OS_WINDOWS

#include <string>

namespace mozc {
class CrashReportHandler {
 public:
  // Increments the reference count for per-process ExceptionHandler and
  // initialize it if it does not exist.
  // Returns true if a new ExceptionHandler is created.
  // |check_address| is supported only on Windows and ignored on other
  // platforms.
  // If |check_address| is true the address where exception occurrs is checked
  // and the crash report is not sent if the address is out of the module.
  // This function is thread-safe only if the critical section is set by
  // SetCriticalSection on Windows and NOT thread-safe on Mac.
  // |check_address| is just ignored on Mac.
  static bool Initialize(bool check_address);

  // Returns true if ExceptionHandler is installed and available.
  static bool IsInitialized();

  // Decrements the reference count for per-process ExceptionHandler and delete
  // it if the reference count reaches zero.
  // Returns true if ExceptionHandler is deleted.
  // This function is thread-safe only if the critical section is set by
  // SetCriticalSection on Windows and NOT thread-safe on Mac.
  static bool Uninitialize();

#ifdef OS_WINDOWS
  // Set the CRITICAL_SECTION struct used when initializing or uninitializing
  // ExceptionHandler.
  static void SetCriticalSection(CRITICAL_SECTION *critical_section);
#endif  // OS_WINDOWS

 private:
  // Disallow all constructors, destructors, and operator=.
  CrashReportHandler();
  ~CrashReportHandler();
};
}  // namespace mozc

#endif  // MOZC_BASE_WIN32_CRASH_REPORT_HANDLER_H_

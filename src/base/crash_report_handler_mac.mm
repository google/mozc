// Copyright 2010-2014, Google Inc.
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

#ifdef OS_MACOSX

#import "base/crash_report_handler.h"

#import <Cocoa/Cocoa.h>
#if defined(GOOGLE_JAPANESE_INPUT_BUILD)
#import <GoogleBreakpad/GoogleBreakpad.h>
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

namespace mozc {

#if defined(GOOGLE_JAPANESE_INPUT_BUILD)
// The reference count for GoogleBreakpad
int g_reference_count = 0;

GoogleBreakpadRef g_breakpad = NULL;
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

bool CrashReportHandler::Initialize(bool check_address) {
#if defined(GOOGLE_JAPANESE_INPUT_BUILD)
  ++g_reference_count;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSDictionary *plist = [[NSBundle mainBundle] infoDictionary];
  if (1 == g_reference_count && NULL != plist && NULL == g_breakpad) {
    g_breakpad = GoogleBreakpadCreate(plist);
    [pool release];
    return true;
  }
  [pool release];
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
  return false;
}

bool CrashReportHandler::Uninitialize() {
#if defined(GOOGLE_JAPANESE_INPUT_BUILD)
  --g_reference_count;
  if (0 == g_reference_count && NULL != g_breakpad) {
    GoogleBreakpadRelease(g_breakpad);
    g_breakpad = NULL;
    return true;
  }
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
  return false;
}

}  // namespace mozc

#endif  // OS_MACOSX

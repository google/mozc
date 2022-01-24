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

#ifdef __APPLE__

#import "base/crash_report_handler.h"

#import "base/const.h"

#import <Breakpad/Breakpad.h>
#import <CoreFoundation/CoreFoundation.h>

namespace mozc {

// The reference count for Breakpad
int g_reference_count = 0;

BreakpadRef g_breakpad = nullptr;

bool CrashReportHandler::Initialize(bool check_address) {
  ++g_reference_count;
  @autoreleasepool {
    NSMutableDictionary *plist = [[[NSBundle mainBundle] infoDictionary] mutableCopy];
    if (g_reference_count == 1 && plist != nullptr && g_breakpad == nullptr) {
      // Create a crash reports directory under tmpdir, and set it to the plist
      NSString *tmpDir = NSTemporaryDirectory();
      // crashDir will be $TMPDIR/GoogleJapaneseInput/CrashReports
      NSString *crashDir =
          [NSString pathWithComponents:@[ tmpDir, @kProductPrefix, @"CrashReports" ]];
      [[NSFileManager defaultManager] createDirectoryAtPath:crashDir
                                withIntermediateDirectories:YES
                                                 attributes:nil
                                                      error:NULL];
      [plist setValue:crashDir forKey:@BREAKPAD_DUMP_DIRECTORY];
      g_breakpad = BreakpadCreate(plist);
      return true;
    }
  }
  return false;
}

bool CrashReportHandler::Uninitialize() {
  --g_reference_count;
  if (g_reference_count == 0 && g_breakpad != nullptr) {
    BreakpadRelease(g_breakpad);
    g_breakpad = nullptr;
    return true;
  }
  return false;
}

}  // namespace mozc

#endif  // __APPLE__

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

#include "base/crash_report_util.h"

#include <stddef.h>
#ifdef OS_WIN
#include <windows.h>
#endif  // OS_WIN

#include <cctype>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/system_util.h"
#include "base/util.h"

namespace mozc {

string CrashReportUtil::GetCrashReportDirectory() {
  const char kCrashReportDirectory[] = "CrashReports";
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            kCrashReportDirectory);
}

bool CrashReportUtil::ValidateCrashId(const string &crash_id) {
  // valid crash_id would be like "170ca4b0-d49e-49c3-b815-909dcd5ad6fa"
  const int kCrashIdSize = 36;
  if (crash_id.size() != kCrashIdSize) {
    return false;
  }
  for (size_t i = 0; i < crash_id.size(); ++i) {
    const char c = crash_id[i];
    // 8, 13, 18 and 23 are the positions of the hyphen.
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if ('-' != c) {
        return false;
      }
      continue;
    }
    // characters used in crash_id need to be lower hex numbers.
    if (!isxdigit(c) || (isalpha(c) && isupper(c))) {
      return false;
    }
  }
  return true;
}

bool CrashReportUtil::ValidateVersion(const string &version) {
  // valid version would be like "1.2.3.4"
  vector<string> numbers;
  Util::SplitStringUsing(version, ".", &numbers);
  if (numbers.size() != 4) {
    return false;
  }
  for (size_t i = 0; i < numbers.size(); ++i) {
    const string &number = numbers[i];
    if (number.size() < 1) {
      return false;
    }
    if (number[0] == '0' && number.size() > 1) {
      return false;
    }
    for (size_t j = 0; j < number.size(); ++j) {
      if (!isdigit(number[j])) {
        return false;
      }
    }
  }
  return true;
}

bool CrashReportUtil::Abort() {
#ifdef DEBUG
#ifdef OS_WIN
  ::RaiseException(EXCEPTION_BREAKPOINT, EXCEPTION_NONCONTINUABLE, 0, NULL);
#else
  // NOTE: This function is intended for testing breakpad.
  // Throwing an exception does not meet this purposes since IMKit framework
  // catches the exception if this function is called within the framework.
  // TODO(mazda): Find a better way to make the application crash.
  char *p = NULL;
  *p = 0xff;
#endif  // OS_WIN
  return true;
#else
  return false;
#endif  // DEBUG
}
}  // namespace mozc

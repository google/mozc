// Copyright 2010-2012, Google Inc.
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

#ifdef OS_WINDOWS
#include <windows.h>
#endif  // OS_WINDOWS

#include <cctype>
#include <string>

#include "base/crash_report_handler.h"
#include "base/file_stream.h"
#include "base/number_util.h"
#include "base/util.h"

namespace {
const char kDumpFileExtension[] = ".dmp";
const int kDateSize = 8;

}  // namespace

namespace mozc {

void CrashReportUtil::InstallBreakpad() {
  // TODO(nona): Support breakpad for official branding build on Linux.
#if defined(GOOGLE_JAPANESE_INPUT_BUILD) && !defined(OS_LINUX)
  CrashReportHandler::Initialize(false);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD && !OS_LINUX
}

string CrashReportUtil::GetCrashReportDirectory() {
  const char kCrashReportDirectory[] = "CrashReports";
  return Util::JoinPath(Util::GetUserProfileDirectory(),
                        kCrashReportDirectory);
}

bool CrashReportUtil::CrashDumpExists() {
#ifdef OS_WINDOWS
  WIN32_FIND_DATA find_data = {0};
  const string apattern =
      Util::JoinPath(GetCrashReportDirectory(), "*_*.*.*.*") +
      kDumpFileExtension;
  wstring pattern;
  Util::UTF8ToWide(apattern.c_str(), &pattern);
  HANDLE handle = ::FindFirstFile(pattern.c_str(), &find_data);
  const bool result = (INVALID_HANDLE_VALUE != handle);
  if (result) {
    ::FindClose(handle);
  }
  return result;
#endif  // OS_WINDOWS
  // TODO(mazda): implements Mac and Linux version;
  return false;
}

int CrashReportUtil::GetCurrentDate() {
#ifdef OS_WINDOWS
  SYSTEMTIME systime;
  GetSystemTime(&systime);
  return systime.wYear * 10000 + systime.wMonth * 100 + systime.wDay;
#endif  // OS_WINDOWS
  // TODO(mazda): implements Mac and Linux version;
  return 0;
}

string CrashReportUtil::GetLatestReportPath() {
  const char kLatestReport[] = "LatestReport";
  return Util::JoinPath(GetCrashReportDirectory(), kLatestReport);
}

bool CrashReportUtil::WriteLatestReport(int date) {
  const string current_date_str = NumberUtil::SimpleItoa(date);
  if (kDateSize != current_date_str.size()) {
    return false;
  }
  OutputFileStream file(CrashReportUtil::GetLatestReportPath().c_str());
  if (!file) {
    return false;
  }
  file << current_date_str;
  return true;
}

bool CrashReportUtil::ReadLatestReport(int *date) {
  if (NULL == date) {
    return false;
  }
  InputFileStream file(CrashReportUtil::GetLatestReportPath().c_str());
  if (!file) {
    return false;
  }
  string current_date;
  file >> current_date;
  if (kDateSize!= current_date.size()) {
    return false;
  }
  *date = NumberUtil::SimpleAtoi(current_date);
  return true;
}

string CrashReportUtil::EncodeDumpFileName(const string &crash_id,
                                           const string &version) {
  return crash_id + "_" + version + kDumpFileExtension;
}

bool CrashReportUtil::DecodeDumpFileName(const string &filename,
                                         string *crash_id,
                                         string *version) {
  if (NULL == crash_id && NULL == version) {
    return false;
  }
  const size_t kDumpFileExtensionSize = arraysize(kDumpFileExtension) - 1;
  if (filename.size() < kDumpFileExtensionSize) {
    return false;
  }
  // filename would be like "170ca4b0-d49e-49c3-b815-909dcd5ad6fa_1.2.3.4.dmp".
  vector<string> crash_id_version;
  Util::SplitStringUsing(
      filename.substr(0, filename.size() - kDumpFileExtensionSize),
      "_",
      &crash_id_version);
  if (crash_id_version.size() != 2) {
    return false;
  }
  if (!ValidateCrashId(crash_id_version[0]) ||
      !ValidateVersion(crash_id_version[1])) {
    return false;
  }
  if (NULL != crash_id) {
    *crash_id = crash_id_version[0];
  }
  if (NULL != version) {
    *version = crash_id_version[1];
  }
  return true;
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
#ifdef OS_WINDOWS
  ::RaiseException(EXCEPTION_BREAKPOINT, EXCEPTION_NONCONTINUABLE, 0, NULL);
#else
  // NOTE: This function is intended for testing breakpad.
  // Throwing an exception does not meet this purposes since IMKit framework
  // catches the exception if this function is called within the framework.
  // TODO(mazda): Find a better way to make the application crash.
  char *p = NULL;
  *p = 0xff;
#endif  // OS_WINDOWS
  return true;
#else
  return false;
#endif  // DEBUG
}
}  // namespace mozc

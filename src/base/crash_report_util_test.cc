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

#ifdef OS_WIN

#include "base/crash_report_util.h"

#include <string>

#include "testing/base/public/gunit.h"

namespace mozc {

TEST(CrashReportUtilTest, ValdateCrashId) {
  EXPECT_TRUE(CrashReportUtil::ValidateCrashId(
      "170ca4b0-d49e-49c3-b815-909dcd5ad6fa"));
  EXPECT_TRUE(CrashReportUtil::ValidateCrashId(
      "272adcee-6e4c-4f78-9f66-d2912c8dbca9"));
  EXPECT_TRUE(CrashReportUtil::ValidateCrashId(
      "91899d33-61a4-41ee-b819-23aa31a6092e"));

  // empty string
  EXPECT_FALSE(CrashReportUtil::ValidateCrashId(""));

  // capital characters
  EXPECT_FALSE(CrashReportUtil::ValidateCrashId(
      "170CA4B0-D49E-49C3-B815-909DCD5AD6FA"));
  EXPECT_FALSE(CrashReportUtil::ValidateCrashId(
      "272ADCEE-6E4C-4F78-9F66-D2912C8DBCA9"));
  EXPECT_FALSE(CrashReportUtil::ValidateCrashId(
      "91899D33-61A4-41EE-B819-23AA31A6092E"));

  // wrong id length
  EXPECT_FALSE(CrashReportUtil::ValidateCrashId(
      "a170ca4b0-d49e-49c3-b815-909dcd5ad6f"));
  EXPECT_FALSE(CrashReportUtil::ValidateCrashId(
      "272adcee-b6e4c-4f78-9f66-d2912c8dbca9a"));

  // no hyphen
  EXPECT_FALSE(CrashReportUtil::ValidateCrashId(
      "170ca4b0ad49ea49c3ab815a909dcd5ad6fa"));

  // wrong hyphen positions
  EXPECT_FALSE(CrashReportUtil::ValidateCrashId(
      "170ca4b-0d49e-49c3-b815-909dcd5ad6fa"));
  EXPECT_FALSE(CrashReportUtil::ValidateCrashId(
      "272adcee-6e4c4-f78-9f66-d2912c8dbca9"));
  EXPECT_FALSE(CrashReportUtil::ValidateCrashId(
      "91899d33-61a4-41eeb-819-23aa31a6092e"));

  // non hexadecimal value
  EXPECT_FALSE(CrashReportUtil::ValidateCrashId(
      "g70ca4b0-d49e-49c3-b815-909dcd5ad6fa"));
  EXPECT_FALSE(CrashReportUtil::ValidateCrashId(
      "272adcee-he4c-4f78-9f66-d2912c8dbca9"));
  EXPECT_FALSE(CrashReportUtil::ValidateCrashId(
      "91899d33-61a4-i1ee-b819-23aa31a6092e"));
}

TEST(CrashReportUtilTest, ValidateVersion) {
  EXPECT_TRUE(CrashReportUtil::ValidateVersion("0.0.0.0"));
  EXPECT_TRUE(CrashReportUtil::ValidateVersion("1.0.0.0"));
  EXPECT_TRUE(CrashReportUtil::ValidateVersion("0.2.0.0"));
  EXPECT_TRUE(CrashReportUtil::ValidateVersion("0.0.3.0"));
  EXPECT_TRUE(CrashReportUtil::ValidateVersion("0.0.0.4"));
  EXPECT_TRUE(CrashReportUtil::ValidateVersion("1.2.3.4"));
  EXPECT_TRUE(CrashReportUtil::ValidateVersion("11.2.3.4"));
  EXPECT_TRUE(CrashReportUtil::ValidateVersion("1.22.3.4"));
  EXPECT_TRUE(CrashReportUtil::ValidateVersion("1.2.33.4"));
  EXPECT_TRUE(CrashReportUtil::ValidateVersion("1.2.3.44"));
  EXPECT_TRUE(CrashReportUtil::ValidateVersion("1.2.3.44"));

  EXPECT_FALSE(CrashReportUtil::ValidateVersion(""));
  EXPECT_FALSE(CrashReportUtil::ValidateVersion("0"));
  EXPECT_FALSE(CrashReportUtil::ValidateVersion("0.0"));
  EXPECT_FALSE(CrashReportUtil::ValidateVersion("1.2.3"));
  EXPECT_FALSE(CrashReportUtil::ValidateVersion("1.2.3.4.5"));
  EXPECT_FALSE(CrashReportUtil::ValidateVersion("01.2.3.4"));
  EXPECT_FALSE(CrashReportUtil::ValidateVersion("1.02.3.4"));
  EXPECT_FALSE(CrashReportUtil::ValidateVersion("1.2.03.4"));
  EXPECT_FALSE(CrashReportUtil::ValidateVersion("1.2.3.04"));
}
}  // namespace mozc

#endif  // OS_WIN

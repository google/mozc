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

#include "base/version.h"

#include <algorithm>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "base/number_util.h"

// Import the generated version_def.h.
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/version_def.h"

#ifdef _WIN32
#include "base/win32/wide_char.h"
#endif  // _WIN32

namespace mozc {
namespace {

bool StringAsIntegerComparator(absl::string_view lhs, absl::string_view rhs) {
  return NumberUtil::SimpleAtoi(lhs) < NumberUtil::SimpleAtoi(rhs);
}

}  // namespace

std::string Version::GetMozcVersion() { return version::kMozcVersion; }

#ifdef _WIN32
std::wstring Version::GetMozcVersionW() {
  return win32::Utf8ToWide(version::kMozcVersion);
}
#endif  // _WIN32

int Version::GetMozcVersionMajor() { return version::kMozcVersionMajor; }

int Version::GetMozcVersionMinor() { return version::kMozcVersionMinor; }

int Version::GetMozcVersionBuildNumber() {
  return version::kMozcVersionBuildNumber;
}

int Version::GetMozcVersionRevision() { return version::kMozcVersionRevision; }

const char* Version::GetMozcEngineVersion() {
  return version::kMozcEngineVersion;
}

bool Version::CompareVersion(const std::string& lhs, const std::string& rhs) {
  if (lhs == rhs) {
    return false;
  }
  if (absl::StrContains(lhs, "Unknown") || absl::StrContains(rhs, "Unknown")) {
    LOG(WARNING) << "Unknown is given as version";
    return false;
  }
  const std::vector<absl::string_view> vlhs =
      absl::StrSplit(lhs, '.', absl::SkipEmpty());
  const std::vector<absl::string_view> vrhs =
      absl::StrSplit(rhs, '.', absl::SkipEmpty());
  return std::lexicographical_compare(vlhs.begin(), vlhs.end(), vrhs.begin(),
                                      vrhs.end(), StringAsIntegerComparator);
}

}  // namespace mozc

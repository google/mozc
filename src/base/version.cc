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

#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"

// Import the generated version_def.h.
#include "base/version_def.h"

namespace mozc {

namespace {
bool StringAsIntegerComparator(const std::string &lhs, const std::string &rhs) {
  return NumberUtil::SimpleAtoi(lhs) < NumberUtil::SimpleAtoi(rhs);
}
}  // namespace

std::string Version::GetMozcVersion() { return version::kMozcVersion; }

#ifdef OS_WIN
std::wstring Version::GetMozcVersionW() {
  std::wstring version;
  Util::UTF8ToWide(version::kMozcVersion, &version);
  return version;
}
#endif

int Version::GetMozcVersionMajor() { return version::kMozcVersionMajor; }

int Version::GetMozcVersionMinor() { return version::kMozcVersionMinor; }

int Version::GetMozcVersionBuildNumber() {
  return version::kMozcVersionBuildNumber;
}

int Version::GetMozcVersionRevision() { return version::kMozcVersionRevision; }

const char *Version::GetMozcEngineVersion() {
  return version::kMozcEngineVersion;
}

bool Version::CompareVersion(const std::string &lhs, const std::string &rhs) {
  if (lhs == rhs) {
    return false;
  }
  if (lhs.find("Unknown") != std::string::npos ||
      rhs.find("Unknown") != std::string::npos) {
    LOG(WARNING) << "Unknown is given as version";
    return false;
  }
  std::vector<std::string> vlhs;
  Util::SplitStringUsing(lhs, ".", &vlhs);
  std::vector<std::string> vrhs;
  Util::SplitStringUsing(rhs, ".", &vrhs);
  return std::lexicographical_compare(vlhs.begin(), vlhs.end(), vrhs.begin(),
                                      vrhs.end(), StringAsIntegerComparator);
}

}  // namespace mozc

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

#ifndef MOZC_BASE_VERSION_H_
#define MOZC_BASE_VERSION_H_

#include <string>

namespace mozc {

class Version {
 public:
  Version(const Version&) = delete;
  Version& operator=(const Version&) = delete;

  // Get current mozc version (former called MOZC_VERSION)
  static std::string GetMozcVersion();

#ifdef _WIN32
  // Get current mozc version (former called MOZC_VERSION) by std::wstring
  static std::wstring GetMozcVersionW();
#endif  // _WIN32

  static int GetMozcVersionMajor();
  static int GetMozcVersionMinor();
  static int GetMozcVersionBuildNumber();
  static int GetMozcVersionRevision();
  static const char* GetMozcEngineVersion();

  // Returns true if lhs is less than rhs in the lexical order.
  // CompareVersion("1.2.3.4", "1.2.3.4") => false
  // CompareVersion("1.2.3.4", "5.2.3.4") => true
  // CompareVersion("1.25.3.4", "1.2.3.4") => false
  static bool CompareVersion(const std::string& lhs, const std::string& rhs);
};

}  // namespace mozc

#endif  // MOZC_BASE_VERSION_H_

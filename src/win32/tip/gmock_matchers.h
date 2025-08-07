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

#ifndef MOZC_WIN32_TIP_GMOCK_MATCHERS_H_
#define MOZC_WIN32_TIP_GMOCK_MATCHERS_H_

#include <ctffunc.h>
#include <wil/com.h>
#include <wil/resource.h>

#include <ostream>
#include <string>
#include <string_view>

#include "base/win32/hresult.h"
#include "base/win32/wide_char.h"
#include "testing/gunit.h"

namespace mozc::win32::tsf {

class CandidateStringIndexMatcher {
 public:
  explicit CandidateStringIndexMatcher(ULONG expected) : expected_(expected) {}

  template <typename Pointer>
  bool MatchAndExplain(const Pointer& candidate,
                       ::testing::MatchResultListener* listener) const {
    auto* ptr = wil::com_raw_ptr(candidate);
    if (ptr == nullptr) {
      *listener << "is nullptr";
      return false;
    }
    ULONG actual = 0;
    if (HResult hr(ptr->GetIndex(&actual)); hr.Failed()) {
      *listener << "GetIndex() failed: " << hr;
      return false;
    }
    return actual == expected_;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "GetIndex() returns " << expected_;
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "GetIndex() does not return " << expected_;
  }

 private:
  ULONG expected_;
};

inline ::testing::PolymorphicMatcher<CandidateStringIndexMatcher>
CandidateStringIndexIs(ULONG expected) {
  return ::testing::MakePolymorphicMatcher(
      CandidateStringIndexMatcher(expected));
}

class CandidateStringMatcher {
 public:
  explicit CandidateStringMatcher(std::wstring_view expected)
      : expected_(expected) {}

  template <typename Pointer>
  bool MatchAndExplain(const Pointer& candidate,
                       ::testing::MatchResultListener* listener) const {
    auto* ptr = wil::com_raw_ptr(candidate);
    if (ptr == nullptr) {
      *listener << "is nullptr";
      return false;
    }
    wil::unique_bstr actual;
    if (HResult hr(ptr->GetString(actual.put())); hr.Failed()) {
      *listener << "GetString() failed: " << hr;
      return false;
    }
    return actual.get() == expected_;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "GetString() returns " << WideToUtf8(expected_);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "GetString() does not return " << WideToUtf8(expected_);
  }

 private:
  std::wstring expected_;
};

inline ::testing::PolymorphicMatcher<CandidateStringMatcher> CandidateStringIs(
    std::wstring_view expected) {
  return ::testing::MakePolymorphicMatcher(CandidateStringMatcher(expected));
}

}  // namespace mozc::win32::tsf

#endif  // MOZC_WIN32_TIP_GMOCK_MATCHERS_H_

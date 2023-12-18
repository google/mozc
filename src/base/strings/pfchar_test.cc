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

#include "base/strings/pfchar.h"

#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

#ifdef _WIN32
#include <windows.h>
#else  // _WIN32
#include <unistd.h>
#endif  // !_WIN32

namespace mozc {
namespace {

using testing::IsEmpty;
using testing::Not;

// Testing with an actual use case.
pfstring GetHostName() {
  pfstring result;
#ifdef _WIN32
  DWORD size = 0;
  EXPECT_FALSE(GetComputerNameExW(ComputerNameDnsHostname, nullptr, &size));
  EXPECT_EQ(GetLastError(), ERROR_MORE_DATA);
  result.resize(size);
  EXPECT_TRUE(
      GetComputerNameExW(ComputerNameDnsHostname, result.data(), &size));
  result.resize(size);
#else   // _WIN32
  char buf[_POSIX_HOST_NAME_MAX];
  EXPECT_EQ(gethostname(buf, sizeof(buf)), 0);
  result.assign(buf);
#endif  // !_WIN32
  return result;
}

TEST(PFCharTest, GetHostName) {
  const pfstring s = GetHostName();
  EXPECT_THAT(to_string(s), Not(IsEmpty()));
}

TEST(PFCharTest, ToString) {
  constexpr absl::string_view expected = "test string";
  const pfstring ps = PF_STRING("test string");
  EXPECT_EQ(to_string(ps), expected);
  EXPECT_EQ(to_string(std::move(ps)), expected);
}

TEST(PFCharTest, ToPFString) {
  constexpr pfstring_view expected = PF_STRING("test string");
  const std::string s = "test string";
  EXPECT_EQ(to_pfstring(s), expected);
  EXPECT_EQ(to_pfstring(std::move(s)), expected);
}

}  // namespace
}  // namespace mozc

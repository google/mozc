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

#include "base/win32/hresult.h"

#include <windows.h>

#include <sstream>
#include <utility>

#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc::win32 {
namespace {

TEST(HResultTest, ErrorCodes) {
  constexpr HResult hr = HResultFail();
  EXPECT_EQ(hr, E_FAIL);
  EXPECT_EQ(hr.hr(), E_FAIL);
  EXPECT_FALSE(hr.Succeeded());
  EXPECT_TRUE(hr.Failed());

  HResult hr1 = HResultOk(), hr2 = HResultUnexpected();
  EXPECT_EQ(hr1.hr(), S_OK);
  EXPECT_TRUE(hr1.Succeeded());
  EXPECT_FALSE(hr1.Failed());
  EXPECT_EQ(hr2.hr(), E_UNEXPECTED);
  EXPECT_FALSE(hr2.Succeeded());
  EXPECT_TRUE(hr2.Failed());

  using std::swap;
  swap(hr1, hr2);
  EXPECT_EQ(hr1.hr(), E_UNEXPECTED);
  EXPECT_EQ(hr2.hr(), S_OK);

  HResult hr3 = HResultWin32(ERROR_SUCCESS);
  EXPECT_TRUE(hr3.Succeeded());
  EXPECT_FALSE(hr3.Failed());
  EXPECT_EQ(hr3.hr(), S_OK);
}

TEST(HResultTest, ReturnIfErrorHResult) {
  auto f = []() -> HRESULT {
    RETURN_IF_FAILED_HRESULT(S_OK);
    RETURN_IF_FAILED_HRESULT(E_FAIL);
    return HResultFalse();
  };
  EXPECT_EQ(f(), HResultFail());
}

TEST(HResultTest, ToString) {
  HResult hr = HResultOk();
  EXPECT_EQ(hr.ToString(), "Success: S_OK");
  hr = HResultFalse();
  EXPECT_EQ(hr.ToString(), "Success: S_FALSE");

  hr = HResult(2);
  EXPECT_EQ(hr.ToString(), "Success: 0x00000002");

  hr = HResultFail();
  EXPECT_EQ(hr.ToString(), "Failure: E_FAIL");

  hr = HResultWin32(ERROR_ALREADY_EXISTS);
  std::stringstream ss;
  ss << hr;
  EXPECT_EQ(ss.str(),
            "Failure: Cannot create a file when that file already "
            "exists. (0x800700b7)");
}

}  // namespace
}  // namespace mozc::win32

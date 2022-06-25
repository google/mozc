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

#include "base/win_api_test_helper.h"

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

constexpr DWORD kFakeWindowsVersion = 0x12345678;
DWORD WINAPI GetVersionHook() { return kFakeWindowsVersion; }

// Suppress optimizations to avoid a test failure with some environments.
// See b/236203361 for details.
#pragma optimize("", off)
TEST(WinAPITestHelperTest, BasicTest) {
  std::vector<WinAPITestHelper::HookRequest> requests;
  requests.push_back(DEFINE_HOOK("kernel32.dll", GetVersion, GetVersionHook));

  auto restore_info =
      WinAPITestHelper::DoHook(::GetModuleHandle(nullptr), requests);
  EXPECT_EQ(GetVersion(), kFakeWindowsVersion);

  WinAPITestHelper::RestoreHook(restore_info);
  restore_info = nullptr;

  EXPECT_NE(GetVersion(), kFakeWindowsVersion);
}
#pragma optimize("", on)

}  // namespace
}  // namespace mozc

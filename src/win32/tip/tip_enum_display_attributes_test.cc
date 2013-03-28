// Copyright 2010-2013, Google Inc.
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

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_enum_display_attributes.h"

namespace mozc {
namespace win32 {
namespace tsf {

using ATL::CComPtr;

class TipEnumDisplayAttributesTest : public testing::Test {
 protected:
  static void SetUpTestCase() {
    TipDllModule::InitForUnitTest();
  }
};

TEST(TipEnumDisplayAttributesTest, BasicTest) {
  TipEnumDisplayAttributes enum_display_attribute;
  ASSERT_TRUE(SUCCEEDED(enum_display_attribute.Reset()));

  while (true) {
    CComPtr<ITfDisplayAttributeInfo> info;
    ULONG fetched = 0;
    const HRESULT result = enum_display_attribute.Next(1, &info, &fetched);
    EXPECT_TRUE(SUCCEEDED(result));
    if (result == S_OK) {
      EXPECT_EQ(1, fetched);
      EXPECT_NE(nullptr, info);
    } else {
      EXPECT_EQ(S_FALSE, result);
      EXPECT_EQ(0, fetched);
      EXPECT_EQ(nullptr, info);
      break;
    }
  }
}

TEST(TipEnumDisplayAttributesTest, NextTest) {
  TipEnumDisplayAttributes enum_display_attribute;

  ITfDisplayAttributeInfo *infolist[4] = {};

  ULONG fetched = 0;
  const HRESULT result = enum_display_attribute.Next(
      arraysize(infolist), infolist, &fetched);
  EXPECT_EQ(S_FALSE, result);
  EXPECT_EQ(2, fetched);

  EXPECT_NE(nullptr, infolist[0]);
  EXPECT_NE(nullptr, infolist[1]);
  EXPECT_EQ(nullptr, infolist[2]);
  EXPECT_EQ(nullptr, infolist[3]);

  // Clean up.
  for (size_t i = 0; i < arraysize(infolist); ++i) {
    if (infolist[i] != nullptr) {
      infolist[i]->Release();
      infolist[i] = nullptr;
    }
  }
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

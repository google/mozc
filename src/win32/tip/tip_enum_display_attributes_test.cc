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

#include "win32/tip/tip_enum_display_attributes.h"

#include <objbase.h>
#include <wil/com.h>

#include <iterator>

#include "testing/gunit.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

TEST(TipEnumDisplayAttributesTest, BasicTest) {
  TipEnumDisplayAttributes enum_display_attribute;
  ASSERT_TRUE(SUCCEEDED(enum_display_attribute.Reset()));

  while (true) {
    wil::com_ptr_nothrow<ITfDisplayAttributeInfo> info;
    ULONG fetched = 0;
    const HRESULT result = enum_display_attribute.Next(1, &info, &fetched);
    EXPECT_TRUE(SUCCEEDED(result));
    if (result == S_OK) {
      EXPECT_EQ(fetched, 1);
      EXPECT_NE(info, nullptr);
    } else {
      EXPECT_EQ(result, S_FALSE);
      EXPECT_EQ(fetched, 0);
      EXPECT_EQ(info, nullptr);
      break;
    }
  }
}

TEST(TipEnumDisplayAttributesTest, NextTest) {
  TipEnumDisplayAttributes enum_display_attribute;

  ITfDisplayAttributeInfo *infolist[4] = {};

  ULONG fetched = 0;
  const HRESULT result =
      enum_display_attribute.Next(std::size(infolist), infolist, &fetched);
  EXPECT_EQ(result, S_FALSE);
  EXPECT_EQ(fetched, 2);

  EXPECT_NE(infolist[0], nullptr);
  EXPECT_NE(infolist[1], nullptr);
  EXPECT_EQ(infolist[2], nullptr);
  EXPECT_EQ(infolist[3], nullptr);

  // Clean up.
  for (auto &i : infolist) {
    if (i != nullptr) {
      i->Release();
      i = nullptr;
    }
  }
}

}  // namespace
}  // namespace tsf
}  // namespace win32
}  // namespace mozc

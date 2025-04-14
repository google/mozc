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

#include "win32/tip/tip_display_attributes.h"

#include <guiddef.h>
#include <msctf.h>
#include <wil/resource.h>

#include <string>

#include "testing/gunit.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

class TestableTipDisplayAttribute : public TipDisplayAttribute {
 public:
  TestableTipDisplayAttribute(const GUID &guid,
                              const TF_DISPLAYATTRIBUTE &attribute,
                              const std::wstring &description)
      : TipDisplayAttribute(guid, attribute, description) {}
};

constexpr TF_DISPLAYATTRIBUTE kTestAttribute = {
    {TF_CT_NONE, {}},  // text color
    {TF_CT_NONE, {}},  // background color
    TF_LS_DOT,         // underline style
    FALSE,             // underline boldness
    {TF_CT_NONE, {}},  // underline color
    TF_ATTR_INPUT      // attribute info
};

constexpr TF_DISPLAYATTRIBUTE kTestUserAttribute = {
    {TF_CT_NONE, {}},         // text color
    {TF_CT_NONE, {}},         // background color
    TF_LS_SOLID,              // underline style
    TRUE,                     // underline boldness
    {TF_CT_NONE, {}},         // underline color
    TF_ATTR_TARGET_CONVERTED  // attribute info
};

// {4D20DBEC-C60E-4DAC-B456-9222521E5036}
constexpr GUID kTestGuid = {0x4d20dbec,
                            0xc60e,
                            0x4dac,
                            {0xb4, 0x56, 0x92, 0x22, 0x52, 0x1e, 0x50, 0x36}};

constexpr wchar_t kTestDescription[] = L"This is test description!";

bool IsSameColor(const TF_DA_COLOR &color1, const TF_DA_COLOR &color2) {
  return color1.cr == color2.cr && color1.nIndex == color2.nIndex &&
         color1.type == color2.type;
}

}  // namespace

TEST(TipDisplayAttributesTest, BasicTest) {
  TestableTipDisplayAttribute attribute(kTestGuid, kTestAttribute,
                                        kTestDescription);

  wil::unique_bstr desc;
  EXPECT_EQ(attribute.GetDescription(desc.put()), S_OK);
  EXPECT_STREQ(kTestDescription, desc.get());

  GUID guid;
  EXPECT_EQ(attribute.GetGUID(&guid), S_OK);
  EXPECT_NE(::IsEqualGUID(kTestGuid, guid), FALSE);

  TF_DISPLAYATTRIBUTE info;
  EXPECT_EQ(attribute.GetAttributeInfo(&info), S_OK);
  EXPECT_EQ(info.bAttr, kTestAttribute.bAttr);
  EXPECT_TRUE(IsSameColor(kTestAttribute.crBk, info.crBk));
  EXPECT_TRUE(IsSameColor(kTestAttribute.crLine, info.crLine));
  EXPECT_TRUE(IsSameColor(kTestAttribute.crText, info.crText));
  EXPECT_EQ(info.fBoldLine, kTestAttribute.fBoldLine);
  EXPECT_EQ(info.lsStyle, kTestAttribute.lsStyle);
}

TEST(TipDisplayAttributesTest, SetAttributeInfo) {
  TestableTipDisplayAttribute attribute(kTestGuid, kTestAttribute,
                                        kTestDescription);

  EXPECT_EQ(attribute.SetAttributeInfo(&kTestUserAttribute), S_OK);

  TF_DISPLAYATTRIBUTE info;
  EXPECT_EQ(attribute.GetAttributeInfo(&info), S_OK);
  EXPECT_EQ(info.bAttr, kTestUserAttribute.bAttr);
  EXPECT_TRUE(IsSameColor(kTestUserAttribute.crBk, info.crBk));
  EXPECT_TRUE(IsSameColor(kTestUserAttribute.crLine, info.crLine));
  EXPECT_TRUE(IsSameColor(kTestUserAttribute.crText, info.crText));
  EXPECT_EQ(info.fBoldLine, kTestUserAttribute.fBoldLine);
  EXPECT_EQ(info.lsStyle, kTestUserAttribute.lsStyle);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

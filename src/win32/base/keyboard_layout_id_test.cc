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

#include "win32/base/keyboard_layout_id.h"
#include "base/util.h"

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace win32 {
TEST(KeyboardLayoutIDTest, Validation) {
  // default constructor.
  {
    KeyboardLayoutID klid;
    EXPECT_FALSE(klid.has_id());
  }

  // constructor for the integer form.
  {
    KeyboardLayoutID klid(0);
    EXPECT_TRUE(klid.has_id());
    EXPECT_EQ(0x00000000, klid.id());
    EXPECT_EQ(std::wstring(L"00000000"), klid.ToString());
  }

  // constructor for the text form.
  {
    KeyboardLayoutID klid(L"00000000");
    EXPECT_TRUE(klid.has_id());
    EXPECT_EQ(0x00000000, klid.id());
    EXPECT_EQ(std::wstring(L"00000000"), klid.ToString());
  }

  {
    KeyboardLayoutID klid(0);
    EXPECT_TRUE(klid.has_id());

    // Can clear the id.
    klid.clear_id();
    EXPECT_FALSE(klid.has_id());

    // Can reassign the id.
    klid.set_id(1);
    EXPECT_TRUE(klid.has_id());
    EXPECT_EQ(0x00000001, klid.id());
    EXPECT_EQ(std::wstring(L"00000001"), klid.ToString());

    // Can copy the instance.
    KeyboardLayoutID another_klid(L"00000002");
    klid = another_klid;
    EXPECT_TRUE(klid.has_id());
    EXPECT_EQ(0x00000002, klid.id());
    EXPECT_EQ(std::wstring(L"00000002"), klid.ToString());
  }
}

TEST(KeyboardLayoutIDTest, ConvertFromString) {
  KeyboardLayoutID klid;

  EXPECT_TRUE(klid.Parse(L"E0220411"));
  EXPECT_TRUE(klid.has_id());
  EXPECT_EQ(0xE0220411, klid.id());
  EXPECT_EQ(std::wstring(L"E0220411"), klid.ToString());

  EXPECT_TRUE(klid.Parse(L"e0220411"));
  EXPECT_EQ(0xE0220411, klid.id());
  EXPECT_TRUE(klid.has_id());
  // This should be capitalised
  EXPECT_EQ(std::wstring(L"E0220411"), klid.ToString());

  // Do not reject any KLID unless it has an invalid text form.
  // The caller is responsible for checking the existence of this KLID in the
  // current system even if |has_id()| returns true.
  EXPECT_TRUE(klid.Parse(L"00000000"));
  EXPECT_TRUE(klid.has_id());
  EXPECT_EQ(0x00000000, klid.id());
  EXPECT_EQ(std::wstring(L"00000000"), klid.ToString());

  // Invalid text form.  Should be rejected.
  EXPECT_FALSE(klid.Parse(L"123"));
  EXPECT_FALSE(klid.has_id());

  // Invalid text form.  Should be rejected.
  EXPECT_FALSE(klid.Parse(L"E0220 411"));
  EXPECT_FALSE(klid.has_id());

  // Invalid text form.  Should be rejected.
  EXPECT_FALSE(klid.Parse(L"E0G00GLE"));
  EXPECT_FALSE(klid.has_id());

  // Invalid text form.  Should be rejected.
  EXPECT_FALSE(klid.Parse(L""));
  EXPECT_FALSE(klid.has_id());
}

TEST(KeyboardLayoutIDTest, ConvertFromInteger) {
  KeyboardLayoutID klid;

  klid.set_id(0xE0220411);
  EXPECT_TRUE(klid.has_id());
  EXPECT_EQ(0xE0220411, klid.id());
  EXPECT_EQ(std::wstring(L"E0220411"), klid.ToString());

  klid.set_id(0x00000123);
  EXPECT_TRUE(klid.has_id());
  EXPECT_EQ(0x00000123, klid.id());
  EXPECT_EQ(std::wstring(L"00000123"), klid.ToString());

  // Do not reject any KLID because the integer form never be invalid as
  // opposed to text form.
  // The caller is responsible for checking the existence of this KLID in the
  // current system even if |has_id()| returns true.
  klid.set_id(0xFFFFFFFF);
  EXPECT_TRUE(klid.has_id());
  EXPECT_EQ(0xFFFFFFFF, klid.id());
  EXPECT_EQ(std::wstring(L"FFFFFFFF"), klid.ToString());
}
}  // namespace win32
}  // namespace mozc

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

#include "win32/base/text_icon.h"

#include <windows.h>
#include <wil/resource.h>

#include <cstddef>
#include <string>

#include "base/win32/win_font_test_helper.h"
#include "testing/gunit.h"

namespace mozc {
namespace win32 {
namespace {

using ::testing::AssertionFailure;
using ::testing::AssertionSuccess;

class TextIconTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    // On Windows XP, the availability of typical Japanese fonts such are as
    // MS Gothic depends on the language edition and language packs.
    // So we will register a private font for unit test.
    WinFontTestHelper::Initialize();
  }

  static void TearDownTestCase() {
    // Free private fonts although the system automatically frees them when
    // this process is terminated.
    WinFontTestHelper::Uninitialize();
  }

 protected:
  static std::string GetTestFontName() {
    return WinFontTestHelper::GetTestFontName();
  }
};

::testing::AssertionResult ExpectMonochromeIcon(const wil::unique_hicon &icon,
                                                size_t size) {
  if (!icon) {
    return AssertionFailure() << "|icon| is nullptr.";
  }

  ICONINFO info = {};
  if (::GetIconInfo(icon.get(), &info) == 0) {
    return AssertionFailure() << "GetIconInfo failed.";
  }
  wil::unique_hbitmap xor_bmp(info.hbmColor);
  wil::unique_hbitmap and_bmp(info.hbmMask);

  if (!xor_bmp) {
    return AssertionFailure()
           << "XOR bitmap (hbmColor) should not be nullptr. This icon causes a "
              "GDI "
              "handle leak when it is passed to ITfLangBarItemButton::GetIcon.";
  }

  if (!and_bmp) {
    return AssertionFailure() << "AND bitmap (hbmMask) should not be nullptr.";
  }

  {
    BITMAP xor_bmp_info = {};
    if (::GetObject(xor_bmp.get(), sizeof(xor_bmp_info), &xor_bmp_info) == 0) {
      return AssertionFailure() << "GetObject for xor_bmp failed.";
    }

    // There seems no way to retrieve the true color depth of given icon object.
    // As far as we've observed on Windows XP and Windows 7, |info.hbmColor|
    // returned from GetIconInfo always have the same color depth to display
    // regardless of the original color depth.

    if (xor_bmp_info.bmWidth != size) {
      return AssertionFailure()
             << "XOR bitmap (hbmColor) does not have expected width."
             << " expected: " << size << " actual:   " << xor_bmp_info.bmWidth;
    }

    if (xor_bmp_info.bmHeight != size) {
      return AssertionFailure()
             << "XOR bitmap (hbmColor) does not have expected height."
             << " expected: " << size << " actual:   " << xor_bmp_info.bmHeight;
    }
  }

  {
    BITMAP and_bmp_info = {};
    if (::GetObject(and_bmp.get(), sizeof(and_bmp_info), &and_bmp_info) == 0) {
      return AssertionFailure() << "GetObject for and_bmp failed.";
    }

    if (and_bmp_info.bmBitsPixel != 1) {
      return AssertionFailure()
             << "AND bitmap (hbmMask) does not have expected bit depth."
             << " expected: " << 1 << " actual:   " << and_bmp_info.bmBitsPixel;
    }

    if (and_bmp_info.bmWidth != size) {
      return AssertionFailure()
             << "AND bitmap (hbmMask) does not have expected width."
             << " expected: " << size << " actual:   " << and_bmp_info.bmWidth;
    }

    if (and_bmp_info.bmHeight != size) {
      return AssertionFailure()
             << "AND bitmap (hbmMask) does not have expected height."
             << " expected: " << size << " actual:   " << and_bmp_info.bmHeight;
    }
  }

  return AssertionSuccess();
}

TEST_F(TextIconTest, CreateMonochromeIcon) {
  constexpr size_t kIconSize = 20;
  wil::unique_hicon icon(TextIcon::CreateMonochromeIcon(
      kIconSize, kIconSize, "A", GetTestFontName(), RGB(0xff, 0x00, 0xff)));
  EXPECT_TRUE(ExpectMonochromeIcon(icon, kIconSize));
}

}  // namespace
}  // namespace win32
}  // namespace mozc

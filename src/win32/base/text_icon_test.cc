// Copyright 2010-2020, Google Inc.
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

// clang-format off
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>
#include <atlgdi.h>
// clang-format on

#include "base/logging.h"
#include "base/mmap.h"
#include "base/util.h"
#include "base/win_font_test_helper.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace win32 {
namespace {

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using ::WTL::CBitmap;
using ::WTL::CIcon;

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
  static string GetGothicFontName() {
    return WinFontTestHelper::GetIPAexGothicFontName();
  }
};

::testing::AssertionResult ExpectMonochromeIcon(const CIcon &icon,
                                                size_t size) {
  if (icon.IsNull()) {
    return AssertionFailure() << "|icon| is nullptr.";
  }

  ICONINFO info = {};
  if (!icon.GetIconInfo(&info)) {
    return AssertionFailure() << "GetIconInfo failed.";
  }
  CBitmap xor_bmp = info.hbmColor;
  CBitmap and_bmp = info.hbmMask;

  if (xor_bmp.IsNull()) {
    return AssertionFailure()
           << "XOR bitmap (hbmColor) should not be nullptr. This icon causes a "
              "GDI "
              "handle leak when it is passed to ITfLangBarItemButton::GetIcon.";
  }

  if (and_bmp.IsNull()) {
    return AssertionFailure() << "AND bitmap (hbmMask) should not be nullptr.";
  }

  {
    BITMAP xor_bmp_info = {};
    if (!xor_bmp.GetBitmap(xor_bmp_info)) {
      return AssertionFailure() << "GetBitmap failed.";
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
    if (!and_bmp.GetBitmap(and_bmp_info)) {
      return AssertionFailure() << "GetBitmap failed.";
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

#define EXPECT_MONOCHROME_ICON(icon, icon_size) \
  EXPECT_TRUE(ExpectMonochromeIcon((icon), (icon_size)))

TEST_F(TextIconTest, CreateMonochromeIcon) {
  {
    const size_t kIconSize = 20;
    CIcon icon = TextIcon::CreateMonochromeIcon(
        kIconSize, kIconSize, "A", GetGothicFontName(), RGB(0xff, 0x00, 0xff));
    EXPECT_MONOCHROME_ICON(icon, kIconSize);
  }

  {
    const char kText[] = "あ";
    const size_t kIconSize = 20;
    CIcon icon = TextIcon::CreateMonochromeIcon(kIconSize, kIconSize, kText,
                                                GetGothicFontName(),
                                                RGB(0xff, 0x00, 0xff));
    EXPECT_MONOCHROME_ICON(icon, kIconSize);
  }
}

}  // namespace
}  // namespace win32
}  // namespace mozc

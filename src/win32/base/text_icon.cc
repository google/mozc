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

#include <safeint.h>
#include <wil/resource.h>
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "base/win32/wide_char.h"

namespace mozc {
namespace win32 {
namespace {

using ::msl::utilities::SafeCast;
using ::msl::utilities::SafeMultiply;

RGBQUAD ToRGBQuad(DWORD color_ref) {
  const RGBQUAD rgbquad = {GetBValue(color_ref), GetGValue(color_ref),
                           GetRValue(color_ref), 0xff};
  return rgbquad;
}

HICON CreateMonochromeIconInternal(int bitmap_width, int bitmap_height,
                                   absl::string_view text,
                                   absl::string_view fontname,
                                   COLORREF text_color) {
  struct MonochromeBitmapInfo {
    BITMAPINFOHEADER header;
    RGBQUAD color_palette[2];
  };

  uint8_t *src_dib_buffer = nullptr;
  wil::unique_hbitmap src_dib;

  // Step 1. Create a src black-and-white DIB as follows.
  //  - This is a top-down DIB.
  //  - pixel bit is 0 if the pixel should be opaque for text image.
  //  - pixel bit is 1 if the pixel should be transparent.
  //  - |src_dib_buffer| is a 4-byte aligned bitmap image.
  {
    constexpr DWORD kBackgroundColor = RGB(0x00, 0x00, 0x00);
    constexpr DWORD kForegroundColor = RGB(0xff, 0xff, 0xff);

    MonochromeBitmapInfo info = {};
    info.header.biSize = sizeof(info.header);
    info.header.biWidth = bitmap_width;
    info.header.biHeight = -bitmap_height;  // negavive value for top-down BMP
    info.header.biPlanes = 1;
    info.header.biBitCount = 1;
    info.header.biCompression = BI_RGB;
    info.header.biSizeImage = 0;
    // Note: these color is not directly used for the final output. All we need
    // to do here is to use different color and to make sure all the background
    // pixels are filled with 1.
    info.color_palette[0] = ToRGBQuad(kForegroundColor);
    info.color_palette[1] = ToRGBQuad(kBackgroundColor);

    src_dib.reset(::CreateDIBSection(
        nullptr, reinterpret_cast<const BITMAPINFO *>(&info), DIB_RGB_COLORS,
        reinterpret_cast<void **>(&src_dib_buffer), nullptr, 0));
    if (!src_dib.is_valid()) {
      return nullptr;
    }

    wil::unique_hdc dc(::CreateCompatibleDC(nullptr));
    if (!dc.is_valid()) {
      return nullptr;
    }

    wil::unique_select_object old_bitmap(
        wil::SelectObject(dc.get(), src_dib.get()));
    LOGFONT logfont = {};
    {
      logfont.lfWeight = FW_NORMAL;
      logfont.lfCharSet = DEFAULT_CHARSET;
      logfont.lfHeight = bitmap_height;
      logfont.lfQuality = NONANTIALIASED_QUALITY;
      const std::wstring wide_fontname = win32::Utf8ToWide(fontname);
      const errno_t error = wcscpy_s(logfont.lfFaceName, wide_fontname.c_str());
      if (error != 0) {
        return nullptr;
      }
    }

    wil::unique_hfont font_handle(::CreateFontIndirect(&logfont));
    if (!font_handle.is_valid()) {
      return nullptr;
    }

    wil::unique_select_object old_font(
        wil::SelectObject(dc.get(), font_handle.get()));
    ::SetBkMode(dc.get(), OPAQUE);
    ::SetBkColor(dc.get(), kBackgroundColor);
    ::SetTextColor(dc.get(), kForegroundColor);
    const std::wstring wide_text = win32::Utf8ToWide(text);
    RECT rect = {0, 0, bitmap_width, bitmap_height};
    ::ExtTextOutW(dc.get(), 0, 0, ETO_OPAQUE, &rect, nullptr, 0, nullptr);
    ::DrawTextW(dc.get(), wide_text.c_str(), wide_text.size(), &rect,
                DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_CENTER);
    ::GdiFlush();
  }

  BITMAP src_bmp_info = {};
  if (::GetObject(src_dib.get(), sizeof(src_bmp_info), &src_bmp_info) == 0) {
    return nullptr;
  }

  // Step 2. Create XOR bitmap.
  //  - This is a top-down DIB.
  //  - pixel bit is 0 if the pixel should be opaque for text image.
  //    - color palette for this pixel should be |text_color|.
  //  - pixel bit is 1 if the pixel should be transparent.
  //    - color palette for this pixel should be RGB(0, 0, 0), which has null
  //      effect when XOR operation is done.
  wil::unique_hbitmap xor_dib;
  {
    MonochromeBitmapInfo info = {};
    info.header.biSize = sizeof(info.header);
    info.header.biWidth = bitmap_width;
    info.header.biHeight = -bitmap_height;  // negavive value for top-down BMP
    info.header.biPlanes = 1;
    info.header.biBitCount = 1;
    info.header.biCompression = BI_RGB;
    info.header.biSizeImage = 0;
    // Foreground pixel: should be initialized with the given |text_color|.
    info.color_palette[0] = ToRGBQuad(text_color);
    // Background pixel: should be 0, which has null effect in XOR operation.
    info.color_palette[1] = ToRGBQuad(0);

    uint8_t *xor_dib_buffer = nullptr;
    xor_dib.reset(::CreateDIBSection(
        nullptr, reinterpret_cast<const BITMAPINFO *>(&info), DIB_RGB_COLORS,
        reinterpret_cast<void **>(&xor_dib_buffer), nullptr, 0));
    if (!xor_dib.is_valid()) {
      return nullptr;
    }

    // Make sure that |xor_dib| and |src_dib| have the same pixel format.
    BITMAP xor_dib_info = {};
    if (!::GetObject(xor_dib.get(), sizeof(xor_dib_info), &xor_dib_info) ||
        (xor_dib_info.bmBitsPixel != src_bmp_info.bmBitsPixel) ||
        (xor_dib_info.bmWidthBytes != src_bmp_info.bmWidthBytes) ||
        (xor_dib_info.bmHeight != src_bmp_info.bmHeight)) {
      return nullptr;
    }

    // Here we should be able to copy the buffer safely.
    const size_t data_len = bitmap_height * src_bmp_info.bmWidthBytes;
    ::memcpy(xor_dib_buffer, src_dib_buffer, data_len);
  }

  // Step 3. Create AND bitmap.
  //  - This is a top-down DDB.
  //  - pixel bit is 0 if the pixel should be opaque for text image.
  //  - pixel bit is 1 if the pixel should be transparent.
  wil::unique_hbitmap mask_ddb;
  {
    // Note: each line should be aligned with 2-byte for DDB while DIB uses
    // 4-byte alignment. Here we need to do alignment conversion.
    const size_t mask_buffer_stride = (bitmap_width + 0x0f) / 16 * 2;
    const size_t mask_buffer_size = mask_buffer_stride * bitmap_width;
    std::unique_ptr<uint8_t[]> mask_buffer(new uint8_t[mask_buffer_size]);
    for (size_t y = 0; y < bitmap_height; ++y) {
      for (size_t x = 0; x < bitmap_width; ++x) {
        const uint8_t *src_line_start =
            src_dib_buffer + src_bmp_info.bmWidthBytes * y;
        uint8_t *dest_line_start = mask_buffer.get() + mask_buffer_stride * y;
        ::memcpy(dest_line_start, src_line_start, mask_buffer_stride);
      }
    }
    mask_ddb.reset(
        ::CreateBitmap(bitmap_width, bitmap_height, 1, 1, mask_buffer.get()));
    if (!mask_ddb.is_valid()) {
      return nullptr;
    }
  }

  // Step 4. Create a GDI ICON object.
  {
    ICONINFO info = {};
    info.fIcon = TRUE;
    info.hbmColor = xor_dib.get();
    info.hbmMask = mask_ddb.get();
    info.xHotspot = 0;
    info.yHotspot = 0;
    return ::CreateIconIndirect(&info);
  }
}

}  // namespace

// static
HICON TextIcon::CreateMonochromeIcon(size_t width, size_t height,
                                     absl::string_view text,
                                     absl::string_view fontname,
                                     COLORREF text_color) {
  int safe_width = 0;
  int safe_height = 0;
  int safe_num_pixels = 0;
  if (!SafeCast(width, safe_width) || !SafeCast(height, safe_height) ||
      !SafeMultiply(safe_width, safe_height, safe_num_pixels)) {
    LOG(ERROR) << "Requested size is too large." << " width: " << width
               << " height: " << height;
    return nullptr;
  }

  return CreateMonochromeIconInternal(safe_width, safe_height, text, fontname,
                                      text_color);
}

}  // namespace win32
}  // namespace mozc

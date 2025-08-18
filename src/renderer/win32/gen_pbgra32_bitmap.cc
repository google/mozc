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

#include <objbase.h>
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include "absl/base/attributes.h"
#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "base/file_stream.h"
#include "base/init_mozc.h"
#include "base/win32/wide_char.h"

ABSL_FLAG(std::string, src, "", "path to the input PNG file");
ABSL_FLAG(std::string, dest, "", "path to the output BMP file");

using ::std::max;
using ::std::min;

// gdiplus.h must be placed here because it internally depends on
// global min/max functions.
// TODO(yukawa): Use WIC (Windows Imaging Component) instead of GDI+.
#include <gdiplus.h>  // NOLINT

namespace {

using ::mozc::win32::Utf8ToWide;

constexpr int kErrorLevelSuccess = 0;
constexpr int kErrorLevelFail = 1;

constexpr uint32_t kMaxBitmapWidth = 16384;
constexpr uint32_t kMaxBitmapHeight = 16384;

bool ConvertMain() {
  std::unique_ptr<Gdiplus::Bitmap> image(
      Gdiplus::Bitmap::FromFile(Utf8ToWide(absl::GetFlag(FLAGS_src)).c_str()));

  const uint32_t width_original = image->GetWidth();
  if (width_original > kMaxBitmapWidth) {
    LOG(ERROR) << "Too long width: " << width_original;
    return false;
  }
  const int32_t width = static_cast<int32_t>(width_original);

  const uint32_t height_original = image->GetHeight();
  if (height_original > kMaxBitmapHeight) {
    LOG(ERROR) << "Too long height: " << height_original;
    return false;
  }
  const int32_t height = static_cast<int32_t>(height_original);

  const uint32_t num_pixels = static_cast<uint32_t>(width * height);
  const uint32_t pixel_data_bytes = num_pixels * 4;

  // Pack the PBGR32Bitmap to match the actual file BMP file format.
  // In MSVC, include pshpack2.h and poppack.h.
  // In clang, we use ABSL_ATTRIBUTE_PACKED.
  // pshpack: https://msdn.microsoft.com/en-us/library/2e70t5y1.aspx
#if !ABSL_HAVE_ATTRIBUTE(packed)
#include <pshpack2.h>  // NOLINT
#endif                 // !ABSL_HAVE_ATTRIBUTE(packed)
  struct ABSL_ATTRIBUTE_PACKED PBGR32Bitmap {
    uint16_t file_signature;
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t pixel_data_offset;
    uint32_t header_size;
    int32_t width;
    int32_t height;
    uint16_t num_planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t pixel_data_size;
    int32_t pixel_per_meter_x;
    int32_t pixel_per_meter_y;
    uint32_t num_pallete;
    uint32_t important_color;
  };
  static_assert(sizeof(PBGR32Bitmap) == 54);
#if !ABSL_HAVE_ATTRIBUTE(packed)
#include <poppack.h>  // NOLINT
#endif                // !ABSL_HAVE_ATTRIBUTE(packed)

  PBGR32Bitmap header = {};
  header.file_signature = 0x4d42;  // 'BM'
  header.file_size = sizeof(header) + pixel_data_bytes;
  header.pixel_data_offset = sizeof(header);
  header.header_size = sizeof(header) - offsetof(PBGR32Bitmap, header_size);
  header.width = width;
  header.height = height;
  header.num_planes = 1;
  header.bit_count = 32;
  header.pixel_data_size = pixel_data_bytes;

  mozc::OutputFileStream output_file(
      absl::GetFlag(FLAGS_dest),
      std::ios::out | std::ios::binary | std::ios::trunc);
  if (!output_file.good()) {
    return false;
  }

  output_file.write(reinterpret_cast<const char*>(&header), sizeof(header));
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      Gdiplus::Color color;
      image->GetPixel(x, height - y - 1, &color);
      output_file << static_cast<uint8_t>(color.GetB() / 255.0 * color.GetA());
      output_file << static_cast<uint8_t>(color.GetG() / 255.0 * color.GetA());
      output_file << static_cast<uint8_t>(color.GetR() / 255.0 * color.GetA());
      output_file << color.GetA();
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if (absl::GetFlag(FLAGS_src).empty()) {
    std::cout << "Specify --src option";
    return kErrorLevelFail;
  }
  if (absl::GetFlag(FLAGS_dest).empty()) {
    std::cout << "Specify --dest option";
    return kErrorLevelFail;
  }

  ULONG_PTR gdiplus_token;
  Gdiplus::GdiplusStartupInput input;
  Gdiplus::GdiplusStartup(&gdiplus_token, &input, nullptr);

  const bool result = ConvertMain();

  Gdiplus::GdiplusShutdown(gdiplus_token);
  return result ? kErrorLevelSuccess : kErrorLevelFail;
}

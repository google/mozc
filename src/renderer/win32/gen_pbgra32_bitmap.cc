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

#include <Windows.h>
#include <objbase.h>

#include <algorithm>
#include <memory>

#include "base/file_stream.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/util.h"
#include "absl/flags/flag.h"

ABSL_FLAG(std::string, src, "", "path to the input PNG file");
ABSL_FLAG(std::string, dest, "", "path to the output BMP file");

using ::std::max;
using ::std::min;

// gdiplus.h must be placed here because it internally depends on
// global min/max functions.
// TODO(yukawa): Use WIC (Windows Imaging Component) instead of GDI+.
#include <gdiplus.h>  // NOLINT

namespace {

const int kErrorLevelSuccess = 0;
const int kErrorLevelFail = 1;

const uint32 kMaxBitmapWidth = 16384;
const uint32 kMaxBitmapHeight = 16384;

bool ConvertMain() {
  std::wstring wide_src;
  mozc::Util::UTF8ToWide(absl::GetFlag(FLAGS_src), &wide_src);
  std::unique_ptr<Gdiplus::Bitmap> image(
      Gdiplus::Bitmap::FromFile(wide_src.c_str()));

  const uint32 width_original = image->GetWidth();
  if (width_original > kMaxBitmapWidth) {
    LOG(ERROR) << "Too long width: " << width_original;
    return false;
  }
  const int32 width = static_cast<int32>(width_original);

  const uint32 height_original = image->GetHeight();
  if (height_original > kMaxBitmapHeight) {
    LOG(ERROR) << "Too long height: " << height_original;
    return false;
  }
  const int32 height = static_cast<int32>(height_original);

  const uint32 num_pixels = static_cast<uint32>(width * height);
  const uint32 pixel_data_bytes = num_pixels * 4;

  // Use <pshpack2.h> header to match the actual file BMP file format,
  // which uses 2 byte packing.  Include <poppack.h> to restore the
  // current packing mode.
  // c.f. https://msdn.microsoft.com/en-us/library/2e70t5y1.aspx
#include <pshpack2.h>  // NOLINT
  struct PBGR32Bitmap {
    uint16 file_signature;
    uint32 file_size;
    uint16 reserved1;
    uint16 reserved2;
    uint32 pixel_data_offset;
    uint32 header_size;
    int32 width;
    int32 height;
    uint16 num_planes;
    uint16 bit_count;
    uint32 compression;
    uint32 pixel_data_size;
    int32 pixel_per_meter_x;
    int32 pixel_per_meter_y;
    uint32 num_pallete;
    uint32 important_color;
  };
#include <poppack.h>  // NOLINT

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
      absl::GetFlag(FLAGS_dest).c_str(),
      std::ios::out | std::ios::binary | std::ios::trunc);
  if (!output_file.good()) {
    return false;
  }

  output_file.write(reinterpret_cast<const char *>(&header), sizeof(header));
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      Gdiplus::Color color;
      image->GetPixel(x, height - y - 1, &color);
      const size_t index = (y * width + x) * 4;
      output_file << static_cast<uint8>(color.GetB() / 255.0 * color.GetA());
      output_file << static_cast<uint8>(color.GetG() / 255.0 * color.GetA());
      output_file << static_cast<uint8>(color.GetR() / 255.0 * color.GetA());
      output_file << color.GetA();
    }
  }
  return true;
}

}  // namespace

int main(int argc, char **argv) {
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

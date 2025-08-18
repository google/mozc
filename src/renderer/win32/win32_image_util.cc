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

#include "renderer/win32/win32_image_util.h"

#include <wil/resource.h>
#include <windows.h>

#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "base/coordinates.h"
#include "base/win32/wide_char.h"

namespace mozc {
namespace renderer {
namespace win32 {
namespace {

using ::mozc::renderer::win32::internal::GaussianBlur;
using ::mozc::renderer::win32::internal::SafeFrameBuffer;
using ::mozc::renderer::win32::internal::SubdivisionalPixel;
using ::mozc::renderer::win32::internal::TextLabel;

Rect GetBalloonBoundingRect(
    double left, double top, double width, double height,
    double balloon_tail_height, double balloon_tail_width,
    BalloonImage::BalloonImageInfo::TailDirection balloon_tail) {
  double real_left = left;
  if (balloon_tail == BalloonImage::BalloonImageInfo::kLeft) {
    real_left -= balloon_tail_height;
  }
  const int int_left = static_cast<int>(floor(real_left));

  double real_top = top;
  if (balloon_tail == BalloonImage::BalloonImageInfo::kTop) {
    real_top -= balloon_tail_height;
  }
  const int int_top = static_cast<int>(floor(real_top));

  double real_right = left + width;
  if (balloon_tail == BalloonImage::BalloonImageInfo::kRight) {
    real_right += balloon_tail_height;
  }
  const int int_right = static_cast<int>(ceil(real_right));

  double real_bottom = top + height;
  if (balloon_tail == BalloonImage::BalloonImageInfo::kBottom) {
    real_bottom += balloon_tail_height;
  }
  const int int_bottom = static_cast<int>(ceil(real_bottom));

  return Rect(int_left, int_top, int_right - int_left, int_bottom - int_top);
}

LONG GetHeightFromDeciPoint(LONG height_dp, HDC dc_handle) {
  POINT transformed = {
      .x = 0,
      .y = ::MulDiv(::GetDeviceCaps(dc_handle, LOGPIXELSY), height_dp, 720),
  };
  POINT origin = {.x = 0, .y = 0};
  ::DPtoLP(dc_handle, &transformed, 1);
  ::DPtoLP(dc_handle, &origin, 1);
  return -std::abs(transformed.y - origin.y);
}

class Balloon {
 public:
  Balloon(double left, double top, double width, double height,
          double frame_thickness, double corner_radius,
          double balloon_tail_height, double balloon_tail_width,
          RGBColor frame_color, RGBColor inside_color,
          BalloonImage::BalloonImageInfo::TailDirection balloon_tail)
      : left_(left),
        top_(top),
        width_(width),
        height_(height),
        frame_thickness_(frame_thickness),
        corner_radius_(corner_radius),
        balloon_tail_height_(balloon_tail_height),
        balloon_tail_width_(balloon_tail_width),
        frame_color_(frame_color),
        inside_color_(inside_color),
        balloon_tail_(balloon_tail),
        bounding_rect_(GetBalloonBoundingRect(
            left, top, width, height, balloon_tail_height, balloon_tail_width,
            balloon_tail)) {}

  Balloon(const Balloon&) = delete;
  Balloon& operator=(const Balloon&) = delete;

  void RenderPixel(int x, int y, SubdivisionalPixel* pixel) const {
    {
      const PixelType fast_check_type = GetPixelTypeInternalFast(x, y);
      switch (fast_check_type) {
        case kInside:
          pixel->SetPixel(inside_color_);
          break;
        case kOutside:
          return;
        default:
          break;
      }
    }
    for (SubdivisionalPixel::SubdivisionalPixelIterator it(x, y); !it.Done();
         it.Next()) {
      const PixelType type = GetPixelTypeInternal(it.GetX(), it.GetY());
      switch (type) {
        case kFrame:
          pixel->SetSubdivisionalPixel(it.GetFraction(), frame_color_);
          break;
        case kInside:
          pixel->SetSubdivisionalPixel(it.GetFraction(), inside_color_);
          break;
        default:
          break;
      }
    }
  }

  const Rect& bounding_rect() const { return bounding_rect_; }

  double GetTailX() const {
    switch (balloon_tail_) {
      case BalloonImage::BalloonImageInfo::kTop:
        return left_ + width_ / 2.0;
      case BalloonImage::BalloonImageInfo::kRight:
        return left_ + width_ + balloon_tail_height_;
      case BalloonImage::BalloonImageInfo::kBottom:
        return left_ + width_ / 2.0;
      case BalloonImage::BalloonImageInfo::kLeft:
        return left_ - balloon_tail_height_;
      default:
        ABSL_UNREACHABLE();
    }
  }
  double GetTailY() const {
    switch (balloon_tail_) {
      case BalloonImage::BalloonImageInfo::kTop:
        return top_ - balloon_tail_height_;
      case BalloonImage::BalloonImageInfo::kRight:
        return top_ + height_ / 2.0;
      case BalloonImage::BalloonImageInfo::kBottom:
        return top_ + height_ + balloon_tail_height_;
      case BalloonImage::BalloonImageInfo::kLeft:
        return top_ + height_ / 2.0;
      default:
        ABSL_UNREACHABLE();
    }
  }

 private:
  enum PixelType {
    kUnknown,
    kOutside,
    kFrame,
    kInside,
  };

  // Convert |x|, |y|, |width| and |height| as if the balloon tail is upside
  // and the center of the rectangle is at the origin.
  void Normalize(double* x, double* y, double* width, double* height) const {
    const double src_x = *x;
    const double src_y = *y;
    const double src_width = *width;
    const double src_height = *height;
    switch (balloon_tail_) {
      case BalloonImage::BalloonImageInfo::kTop:
        *x = (left_ + width_ / 2) - src_x;
        *y = (top_ + height_ / 2) - src_y;
        break;
      case BalloonImage::BalloonImageInfo::kRight:
        *x = (top_ + height_ / 2) - src_y;
        *y = src_x - (left_ + width_ / 2);
        *width = src_height;
        *height = src_width;
        break;
      case BalloonImage::BalloonImageInfo::kBottom:
        *x = src_x - (left_ + width_ / 2);
        *y = src_y - (top_ + height_ / 2);
        break;
      case BalloonImage::BalloonImageInfo::kLeft:
        *x = src_y - (top_ + height_ / 2);
        *y = (left_ + width_ / 2) - src_x;
        *width = src_height;
        *height = src_width;
        break;
    }
  }

  // Returns PixelType as a quick determination. If kUnknown is returned, you
  // need to investigate the pixel type more precisely.
  PixelType GetPixelTypeInternalFast(int x, int y) const {
    const double frame = std::max(corner_radius_, frame_thickness_);
    if ((left_ + frame) < x && (x + 1) < (left_ + width_ - frame) &&
        (top_ + frame) < y && (y + 1) < (top_ + height_ - frame)) {
      return kInside;
    }

    switch (balloon_tail_) {
      case BalloonImage::BalloonImageInfo::kTop:
        if (x < left_ || left_ + width_ < x) {
          return kOutside;
        }
        if (y < top_ - balloon_tail_height_ || top_ + height_ < y) {
          return kOutside;
        }
        break;
      case BalloonImage::BalloonImageInfo::kRight:
        if (x < left_ || left_ + width_ + balloon_tail_height_ < x) {
          return kOutside;
        }
        if (y < top_ || top_ + height_ < y) {
          return kOutside;
        }
        break;
      case BalloonImage::BalloonImageInfo::kBottom:
        if (x < left_ || left_ + width_ < x) {
          return kOutside;
        }
        if (y < top_ || top_ + height_ + balloon_tail_height_ < y) {
          return kOutside;
        }
        break;
      case BalloonImage::BalloonImageInfo::kLeft:
        if (x < left_ - balloon_tail_height_ || left_ + width_ < x) {
          return kOutside;
        }
        if (y < top_ || top_ + height_ < y) {
          return kOutside;
        }
        break;
    }
    return kUnknown;
  }

  // Returns PixelType as full determination. This function is slow but works
  // for all cases.
  PixelType GetPixelTypeInternal(double x, double y) const {
    double w = width_;
    double h = height_;

    // Normalize parameters so that we can determine the param as if the
    // balloon's tail is always on top.
    Normalize(&x, &y, &w, &h);

    const double half_width = w / 2.0;
    const double half_height = h / 2.0;
    const double half_tail_width = balloon_tail_width_ / 2.0;

    // From symmetry
    const double abs_x = abs(x);

    if (abs_x > half_width) {
      return kOutside;
    }

    // Check if (x, y) is on the balloon's tail.
    if (balloon_tail_height_ > 0 && half_tail_width > 0) {
      if ((abs_x < half_tail_width) && (y > (half_height - frame_thickness_)) &&
          (y < (half_height + balloon_tail_height_))) {
        const double ratio = balloon_tail_height_ / half_tail_width;
        const double nx = abs_x;
        const double ny = y - half_height - balloon_tail_height_;
        const double outer_line = -ratio * nx;
        const double inner_line =
            outer_line - frame_thickness_ * sqrt(1.0 + ratio * ratio);
        if (ny > outer_line) {
          return kOutside;
        }
        if (ny < inner_line) {
          return kInside;
        }
        return kFrame;
      }
    }

    // Check if (x, y) is not on tail. So |y| can be normalized from symmetry.
    const double abs_y = abs(y);
    if (abs_y > half_height) {
      return kOutside;
    }

    // Check if (x, y) is just outside at the corner.
    if (corner_radius_ > 0.0) {
      const double rx = abs_x - (half_width - corner_radius_);
      if (rx > 0.0) {
        const double ry = abs_y - (half_height - corner_radius_);
        if (ry > 0.0) {
          const double radius_sq = rx * rx + ry * ry;
          if (radius_sq > corner_radius_ * corner_radius_) {
            return kOutside;
          } else {
            const double inner_radius = (corner_radius_ - frame_thickness_);
            if (radius_sq < inner_radius * inner_radius) {
              return kInside;
            }
            return kFrame;
          }
        }
      }
    }

    // Check if (x, y) is on boarder or not.
    if (abs_x > (half_width - frame_thickness_)) {
      return kFrame;
    }
    if (abs_y > (half_height - frame_thickness_)) {
      return kFrame;
    }

    // (x, y) is inside.
    return kInside;
  }

  const double left_;
  const double top_;
  const double width_;
  const double height_;
  const double frame_thickness_;
  const double corner_radius_;
  const double balloon_tail_height_;
  const double balloon_tail_width_;
  const RGBColor frame_color_;
  const RGBColor inside_color_;
  const BalloonImage::BalloonImageInfo::TailDirection balloon_tail_;
  const Rect bounding_rect_;
};

Rect GetBoundingRect(double left, double top, double width, double height) {
  const int int_left = static_cast<int>(floor(left));
  const int int_top = static_cast<int>(floor(top));
  const int int_right = static_cast<int>(ceil(left + width));
  const int int_bottom = static_cast<int>(ceil(top + height));
  return Rect(int_left, int_top, int_right - int_left, int_bottom - int_top);
}

// Core logic to render 1-bit text glyphs for sub-pixel rendering. Caller takes
// the ownerships of the returned pointers.
std::vector<std::unique_ptr<TextLabel::BinarySubdivisionalPixel>> Get1bitGlyph(
    double left, double top, double width, double height,
    const std::string& text, const std::string& fontname, size_t font_point) {
  constexpr size_t kDivision = SubdivisionalPixel::kDivision;

  const Rect& bounding_rect = GetBoundingRect(left, top, width, height);
  const int pix_width = bounding_rect.Width();
  const int pix_height = bounding_rect.Height();

  std::vector<std::unique_ptr<TextLabel::BinarySubdivisionalPixel>> pixels(
      pix_width * pix_height);
  if (text.empty()) {
    return pixels;
  }

  const int bitmap_width = pix_width * kDivision;
  const int bitmap_height = pix_height * kDivision;

  struct MonochromeBitmapInfo {
    BITMAPINFOHEADER header;
    RGBQUAD color_palette[2];
  };

  const RGBQUAD kBackgroundColor = {0x00, 0x00, 0x00, 0x00};
  const RGBQUAD kForegroundColor = {0xff, 0xff, 0xff, 0x00};

  MonochromeBitmapInfo bitmap_info = {};
  bitmap_info.header.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.header.biWidth = bitmap_width;
  bitmap_info.header.biHeight = -bitmap_height;  // top-down BMP
  bitmap_info.header.biPlanes = 1;
  bitmap_info.header.biBitCount = 1;  // Color palettes must have 2 entries.
  bitmap_info.header.biCompression = BI_RGB;
  bitmap_info.header.biSizeImage = 0;
  bitmap_info.color_palette[0] = kBackgroundColor;  // black
  bitmap_info.color_palette[1] = kForegroundColor;  // white

  uint8_t* buffer = nullptr;
  wil::unique_hbitmap dib(::CreateDIBSection(
      nullptr, reinterpret_cast<const BITMAPINFO*>(&bitmap_info),
      DIB_RGB_COLORS, reinterpret_cast<void**>(&buffer), nullptr, 0));

  wil::unique_hdc dc(::CreateCompatibleDC(nullptr));
  wil::unique_select_object old_bitmap(wil::SelectObject(dc.get(), dib.get()));

  const std::wstring wide_fontname = mozc::win32::Utf8ToWide(fontname);
  LOGFONT logfont = {};
  logfont.lfWeight = FW_NORMAL;
  logfont.lfCharSet = DEFAULT_CHARSET;
  logfont.lfHeight =
      GetHeightFromDeciPoint(font_point * 10 * kDivision, dc.get());
  logfont.lfQuality = NONANTIALIASED_QUALITY;
  const errno_t error = wcscpy_s(logfont.lfFaceName, wide_fontname.c_str());
  if (error != 0) {
    LOG(ERROR) << "Failed to copy fontname: " << fontname;
    return pixels;
  }

  wil::unique_hfont font_handle(::CreateFontIndirectW(&logfont));
  wil::unique_select_object old_font(
      wil::SelectObject(dc.get(), font_handle.get()));
  const int rect_left =
      static_cast<int>(floor((left - bounding_rect.Left()) * kDivision));
  const int rect_top =
      static_cast<int>(floor((top - bounding_rect.Top()) * kDivision));
  RECT rect = {rect_left, rect_top,
               rect_left + static_cast<int>(ceil(width * kDivision)),
               rect_top + static_cast<int>(ceil(height * kDivision))};
  ::SetBkMode(dc.get(), TRANSPARENT);
  ::SetTextColor(dc.get(), RGB(255, 255, 255));
  const std::wstring wide_text = mozc::win32::Utf8ToWide(text);
  ::DrawTextW(dc.get(), wide_text.c_str(), wide_text.size(), &rect,
              DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_CENTER);
  ::GdiFlush();

  // DIB section requires 32-bit alignment for each line.
  const size_t bitmap_stride = ((bitmap_width + 31) / 32) * 32;

  for (size_t pix_y = 0; pix_y < pix_height; ++pix_y) {
    for (size_t pix_x = 0; pix_x < pix_width; ++pix_x) {
      const size_t pix_index = pix_y * pix_width + pix_x;
      TextLabel::BinarySubdivisionalPixel* sub_pixels = nullptr;
      for (size_t subpix_y = 0; subpix_y < kDivision; ++subpix_y) {
        const size_t y = pix_y * kDivision + subpix_y;
        for (size_t subpix_x = 0; subpix_x < kDivision; ++subpix_x) {
          const size_t x = pix_x * kDivision + subpix_x;
          const size_t index = y * bitmap_stride + x;
          const size_t byte_index = index / 8;
          const size_t bit_index = index - byte_index * 8;
          if (((buffer[byte_index] >> bit_index) & 1) == 0) {
            continue;
          }
          if (sub_pixels == nullptr) {
            auto ptr = std::make_unique<TextLabel::BinarySubdivisionalPixel>();
            sub_pixels = ptr.get();
            pixels[pix_index] = std::move(ptr);
          }
          sub_pixels->set(subpix_y * kDivision + subpix_x);
        }
      }
    }
  }
  return pixels;
}

double Gauss(double sigma, double x, double y) {
  const double sigma_coef = 0.5 / (sigma * sigma);
  constexpr double kInvPi = 0.31830988618379067153776752674503;
  return kInvPi * sigma_coef * exp(-(x * x + y * y) * sigma_coef);
}

}  // namespace

RGBColor::RGBColor() : r(ValueType()), g(ValueType()), b(ValueType()) {}

RGBColor::RGBColor(ValueType red, ValueType green, ValueType blue)
    : r(red), g(green), b(blue) {}

bool RGBColor::operator==(const RGBColor& that) const {
  return r == that.r && g == that.g && b == that.b;
}

bool RGBColor::operator!=(const RGBColor& that) const {
  return !(*this == that);
}

const RGBColor RGBColor::kBlack(0, 0, 0);
const RGBColor RGBColor::kWhite(255, 255, 255);

ARGBColor::ARGBColor() : a(0), r(0), g(0), b(0) {}

ARGBColor::ARGBColor(ValueType alpha, ValueType red, ValueType green,
                     ValueType blue)
    : a(alpha), r(red), g(green), b(blue) {}

ARGBColor::ARGBColor(const RGBColor& color, ValueType alpha)
    : a(alpha), r(color.r), g(color.g), b(color.b) {}

bool ARGBColor::operator==(const ARGBColor& that) const {
  return a == that.a && r == that.r && g == that.g && b == that.b;
}

bool ARGBColor::operator!=(const ARGBColor& that) const {
  return !(*this == that);
}

const ARGBColor ARGBColor::kBlack(255, 0, 0, 0);
const ARGBColor ARGBColor::kWhite(255, 255, 255, 255);

BalloonImage::BalloonImageInfo::BalloonImageInfo()
    : frame_color(RGBColor(1, 122, 204)),
      inside_color(RGBColor::kWhite),
      blur_color(RGBColor(196, 196, 255)),
      blur_alpha(1.0),
      label_size(13),
      rect_width(45.0),
      rect_height(45.0),
      frame_thickness(1.0),
      corner_radius(0.0),
      tail_height(5.0),
      tail_width(10.0),
      tail_direction(BalloonImage::BalloonImageInfo::kTop),
      blur_sigma(3.0),
      blur_offset_x(0),
      blur_offset_y(0) {}

// static
HBITMAP BalloonImage::Create(const BalloonImageInfo& info, POINT* tail_offset) {
  return CreateInternal(info, tail_offset, nullptr, nullptr);
}

// static
HBITMAP BalloonImage::CreateInternal(const BalloonImageInfo& info,
                                     POINT* tail_offset, SIZE* size,
                                     std::vector<ARGBColor>* arbg_buffer) {
  // Base point. You can set arbitrary position.
  constexpr double kLeft = 10.0;
  constexpr double kTop = 10.0;

  const Balloon balloon(
      kLeft, kTop, std::max(info.rect_width, 0.0),
      std::max(info.rect_height, 0.0), std::max(info.frame_thickness, 0.0),
      std::max(info.corner_radius, 0.0), std::max(info.tail_height, 0.0),
      std::max(info.tail_width, 0.0), info.frame_color, info.inside_color,
      info.tail_direction);

  const TextLabel label(
      kLeft + info.frame_thickness, kTop + info.frame_thickness,
      info.rect_width - 2.0 * info.frame_thickness,
      info.rect_height - 2.0 * info.frame_thickness, info.label,
      info.label_font, info.label_size, info.label_color);

  // Render image into a temporary buffer |frame_buffer|.
  const Rect& rect = balloon.bounding_rect();
  SafeFrameBuffer frame_buffer(rect);
  for (int y = rect.Top(); y < rect.Bottom(); ++y) {
    for (int x = rect.Left(); x < rect.Right(); ++x) {
      SubdivisionalPixel sub_pixel;
      balloon.RenderPixel(x, y, &sub_pixel);
      label.RenderPixel(x, y, &sub_pixel);
      const RGBColor color = sub_pixel.GetPixelColor();
      const double coverage = sub_pixel.GetCoverage();
      // As ARGBColor::ValueType is integer type, add +0.5 for rounding off.
      static_assert(ARGBColor::ValueType(0.5) == ARGBColor::ValueType(),
                    "ARGBColor::ValueType should be integer type");
      const ARGBColor::ValueType alpha =
          static_cast<ARGBColor::ValueType>(coverage * 255.0 + 0.5);
      frame_buffer.SetPixel(x, y, ARGBColor(color, alpha));
    }
  }

  // Hereafter, we apply Gaussian blur.
  GaussianBlur blur(info.blur_sigma);

  const int begin_x =
      rect.Left() - std::max(blur.cutoff_length() - info.blur_offset_x, 0);
  const int begin_y =
      rect.Top() - std::max(blur.cutoff_length() - info.blur_offset_y, 0);
  const int end_x =
      rect.Right() + std::max(blur.cutoff_length() + info.blur_offset_x, 0);
  const int end_y =
      rect.Bottom() + std::max(blur.cutoff_length() + info.blur_offset_y, 0);

  const int bmp_width = end_x - begin_x;
  const int bmp_height = end_y - begin_y;

  if (size != nullptr) {
    size->cx = bmp_width;
    size->cy = bmp_height;
  }
  if (arbg_buffer != nullptr) {
    arbg_buffer->clear();
    arbg_buffer->resize(bmp_width * bmp_height);
  }

  // Note that +0.5 for rounding off is not necessary here.
  if (tail_offset != nullptr) {
    tail_offset->x = static_cast<int>(floor(balloon.GetTailX() - begin_x));
    tail_offset->y = static_cast<int>(floor(balloon.GetTailY() - begin_y));
  }

  // GDI native alpha image is Premultiplied BGRA.
  struct PBGRA {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
  };

  BITMAPINFO bitmap_info = {};
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.bmiHeader.biWidth = bmp_width;
  bitmap_info.bmiHeader.biHeight = -bmp_height;  // top-down BMP
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;
  bitmap_info.bmiHeader.biSizeImage = 0;
  PBGRA* buffer = nullptr;
  wil::unique_hbitmap dib(
      ::CreateDIBSection(nullptr, &bitmap_info, DIB_RGB_COLORS,
                         reinterpret_cast<void**>(&buffer), nullptr, 0));

  struct Accessor {
   public:
    Accessor(const SafeFrameBuffer& frame_buffer, int offset_x, int offset_y)
        : frame_buffer_(frame_buffer),
          offset_x_(offset_x),
          offset_y_(offset_y) {}
    double operator()(int x, int y) const {
      return frame_buffer_.GetPixel(x + offset_x_, y + offset_y_).a;
    }

   private:
    const SafeFrameBuffer& frame_buffer_;
    const int offset_x_;
    const int offset_y_;
  };

  const double normalized_blur_alpha = std::clamp(info.blur_alpha, 0.0, 1.0);
  Accessor accessor(frame_buffer, -info.blur_offset_x, -info.blur_offset_y);
  for (int y = begin_y; y < begin_y + bmp_height; ++y) {
    for (int x = begin_x; x < begin_x + bmp_width; ++x) {
      const int bmp_x = x - begin_x;
      const int bmp_y = y - begin_y;
      const size_t index = bmp_y * bmp_width + bmp_x;
      PBGRA* dest = &buffer[index];
      const ARGBColor fore_color = frame_buffer.GetPixel(x, y);
      double alpha = 0.0;
      double r = 0.0;
      double g = 0.0;
      double b = 0.0;
      if (fore_color.a == 255) {
        // Foreground color only.
        alpha = fore_color.a;
        r = fore_color.r;
        g = fore_color.g;
        b = fore_color.b;
      } else if (fore_color.a == 0) {
        // Background (blur) color only.
        if (info.blur_sigma > 0.0) {
          r = info.blur_color.r;
          g = info.blur_color.g;
          b = info.blur_color.b;
        } else {
          // Do not set blur color.
          r = 0.0;
          g = 0.0;
          b = 0.0;
        }
        alpha = normalized_blur_alpha * blur.Apply(x, y, accessor);
      } else {
        // Foreground color and background blur are mixed.
        const double fore_alpha = fore_color.a / 255.0;
        const double bg_alpha =
            normalized_blur_alpha * blur.Apply(x, y, accessor) / 255.0;
        const double norm = fore_alpha + bg_alpha - fore_alpha * bg_alpha;
        const double factor = (1.0 - fore_alpha) * bg_alpha;
        alpha = norm * 255.0;
        r = (factor * info.blur_color.r + fore_alpha * fore_color.r) / norm;
        g = (factor * info.blur_color.g + fore_alpha * fore_color.g) / norm;
        b = (factor * info.blur_color.b + fore_alpha * fore_color.b) / norm;
      }

      // Store BGRA image
      {
        const double norm_alpha = alpha / 255.0;
        // As ARGBColor::ValueType is integer type, add +0.5 for rounding off.
        static_assert(ARGBColor::ValueType(0.5) == ARGBColor::ValueType(),
                      "ARGBColor::ValueType should be integer type");
        dest->b = static_cast<RGBColor::ValueType>(norm_alpha * b + 0.5);
        dest->g = static_cast<RGBColor::ValueType>(norm_alpha * g + 0.5);
        dest->r = static_cast<RGBColor::ValueType>(norm_alpha * r + 0.5);
        dest->a = static_cast<RGBColor::ValueType>(alpha + 0.5);
      }

      // Store the original ARGB image for unit test.
      if (arbg_buffer != nullptr) {
        // As ARGBColor::ValueType is integer type, add +0.5 for rounding off.
        static_assert(ARGBColor::ValueType(0.5) == ARGBColor::ValueType(),
                      "ARGBColor::ValueType should be integer type");
        arbg_buffer->at(index) =
            ARGBColor(static_cast<ARGBColor::ValueType>(alpha + 0.5),
                      static_cast<ARGBColor::ValueType>(r + 0.5),
                      static_cast<ARGBColor::ValueType>(g + 0.5),
                      static_cast<ARGBColor::ValueType>(b + 0.5));
      }
    }
  }

  return dib.release();
}

namespace internal {

SubdivisionalPixel::Fraction2D::Fraction2D() : x(0), y(0) {}

SubdivisionalPixel::Fraction2D::Fraction2D(size_t x_frac, size_t y_frac)
    : x(x_frac), y(y_frac) {}

SubdivisionalPixel::SubdivisionalPixelIterator::SubdivisionalPixelIterator(
    int base_x, int base_y)
    : base_x_(base_x), base_y_(base_y), numerator_x_(0), numerator_y_(0) {}

SubdivisionalPixel::Fraction2D
SubdivisionalPixel::SubdivisionalPixelIterator::GetFraction() const {
  return Fraction2D(numerator_x_, numerator_y_);
}

double SubdivisionalPixel::SubdivisionalPixelIterator::GetX() const {
  return base_x_ + (numerator_x_ + 0.5) / kDivision;
}

double SubdivisionalPixel::SubdivisionalPixelIterator::GetY() const {
  return base_y_ + (numerator_y_ + 0.5) / kDivision;
}

size_t SubdivisionalPixel::SubdivisionalPixelIterator::GetIndex() const {
  return numerator_y_ * kDivision + numerator_x_;
}

void SubdivisionalPixel::SubdivisionalPixelIterator::Next() {
  ++numerator_x_;
  if (numerator_x_ == kDivision) {
    numerator_x_ = 0;
    ++numerator_y_;
  }
}

bool SubdivisionalPixel::SubdivisionalPixelIterator::Done() const {
  return numerator_y_ >= kDivision;
}

SubdivisionalPixel::SubdivisionalPixel() : single_color_(ColorType::kBlack) {}

const double SubdivisionalPixel::GetCoverage() const {
  switch (GetFillType()) {
    case kEmpty:
      return 0.0;
    case kSingleColor:
      return filled_.count() / static_cast<double>(kTotalPixels);
    case kMultipleColors:
      return filled_.count() / static_cast<double>(kTotalPixels);
    default:
      return 0.0;
  }
}

const SubdivisionalPixel::ColorType SubdivisionalPixel::GetPixelColor() const {
  switch (GetFillType()) {
    case kEmpty:
      return ColorType::kBlack;
    case kSingleColor:
      return single_color_;
    case kMultipleColors: {
      double r = 0;
      double g = 0;
      double b = 0;
      for (size_t i = 0; i < filled_.size(); ++i) {
        if (!filled_.test(i)) {
          continue;
        }
        r += colors_[i].r;
        g += colors_[i].g;
        b += colors_[i].b;
      }
      // As ColorType::ValueType is integer type, add +0.5 for rounding off.
      static_assert(ARGBColor::ValueType(0.5) == ARGBColor::ValueType(),
                    "ARGBColor::ValueType should be integer type");
      return ColorType(ColorType::ValueType(r / filled_.count() + 0.5),
                       ColorType::ValueType(g / filled_.count() + 0.5),
                       ColorType::ValueType(b / filled_.count() + 0.5));
    }
    default:
      return ColorType::kBlack;
  }
}

void SubdivisionalPixel::SetPixel(const ColorType& color) {
  filled_.set();
  colors_.reset();
  single_color_ = color;
}

void SubdivisionalPixel::SetSubdivisionalPixel(const Fraction2D& frac,
                                               const ColorType& color) {
  const size_t index = GetIndex(frac);
  switch (GetFillType()) {
    case kEmpty:
      filled_.set(index);
      colors_.reset();
      single_color_ = color;
      break;
    case kSingleColor:
      if (single_color_ != color) {
        colors_.reset(new ColorType[kTotalPixels]);
        for (size_t i = 0; i < filled_.size(); ++i) {
          if (filled_.test(i)) {
            colors_[i] = single_color_;
          }
        }
        single_color_ = ColorType::kBlack;
        colors_[index] = color;
      }
      filled_.set(index);
      break;
    case kMultipleColors:
      filled_.set(index);
      colors_[index] = color;
      break;
  }
}

void SubdivisionalPixel::SetColorToFilledPixels(const ColorType& color) {
  switch (GetFillType()) {
    case kSingleColor:
      single_color_ = color;
      break;
    case kMultipleColors:
      colors_.reset();
      single_color_ = color;
      break;
    default:
      break;
  }
}

SubdivisionalPixel::FillType SubdivisionalPixel::GetFillType() const {
  if (filled_.none()) {
    return kEmpty;
  }
  if (colors_.get() == nullptr) {
    return kSingleColor;
  }
  return kMultipleColors;
}

// static
size_t SubdivisionalPixel::GetIndex(const Fraction2D& offset) {
  return kDivision * offset.y + offset.x;
}

GaussianBlur::GaussianBlur(double sigma)
    : sigma_(sigma), cutoff_length_(static_cast<int>(ceil(3.0 * sigma))) {
  if (sigma <= 0.0) {
    matrix_.push_back(MatrixElement(0, 0, 1.0));
    return;
  }

  matrix_.reserve(GetMatrixLength() * GetMatrixLength());

  for (int y = -cutoff_length_; y <= cutoff_length_; ++y) {
    for (int x = -cutoff_length_; x <= cutoff_length_; ++x) {
      matrix_.push_back(MatrixElement(x, y, Gauss(sigma_, x, y)));
    }
  }

  // To minimize the loss of trailing digits, sort coefficients just in case.
  // For most cases, this doesn't make a difference though.
  struct MatrixElementSorter {
    bool operator()(const MatrixElement& l, const MatrixElement& r) const {
      return l.coefficient < r.coefficient;
    }
  };
  MatrixElementSorter sorter;
  std::sort(matrix_.begin(), matrix_.end(), sorter);

  double sum = 0.0;
  for (Matrix::const_iterator it = matrix_.begin(); it != matrix_.end(); ++it) {
    sum += it->coefficient;
  }

  // Normalize
  for (Matrix::iterator it = matrix_.begin(); it != matrix_.end(); ++it) {
    it->coefficient /= sum;
  }
}

int GaussianBlur::cutoff_length() const { return cutoff_length_; }

GaussianBlur::MatrixElement::MatrixElement()
    : offset_x(0), offset_y(0), coefficient(0.0) {}

GaussianBlur::MatrixElement::MatrixElement(int x, int y, double c)
    : offset_x(x), offset_y(y), coefficient(c) {}

size_t GaussianBlur::GetMatrixLength() const { return 2 * cutoff_length_ + 1; }

SafeFrameBuffer::SafeFrameBuffer(const Rect& rect)
    : rect_(rect), buffer_(new ARGBColor[rect.Width() * rect.Height()]) {}

ARGBColor SafeFrameBuffer::GetPixel(int x, int y) const {
  if (!IsInWindow(x, y)) {
    return ARGBColor();
  }
  return buffer_[GetIndex(x, y)];
}

void SafeFrameBuffer::SetPixel(int x, int y, const ARGBColor& color) {
  if (!IsInWindow(x, y)) {
    return;
  }
  buffer_[GetIndex(x, y)] = color;
}

bool SafeFrameBuffer::IsInWindow(int x, int y) const {
  if (x < rect_.Left() || rect_.Right() <= x) {
    return false;
  }
  if (y < rect_.Top() || rect_.Bottom() <= y) {
    return false;
  }
  return true;
}

size_t SafeFrameBuffer::GetIndex(int x, int y) const {
  DCHECK(IsInWindow(x, y));
  return (y - rect_.Top()) * rect_.Width() + (x - rect_.Left());
}

TextLabel::TextLabel(double left, double top, double width, double height,
                     const std::string& text, const std::string& font,
                     size_t font_point, const RGBColor text_color)
    : pixels_(Get1bitGlyph(left, top, width, height, text, font, font_point)),
      bounding_rect_(GetBoundingRect(left, top, width, height)),
      text_color_(text_color) {}

TextLabel::~TextLabel() = default;

void TextLabel::RenderPixel(int x, int y, SubdivisionalPixel* dest) const {
  if (x < bounding_rect_.Left() || bounding_rect_.Right() <= x ||
      y < bounding_rect_.Top() || bounding_rect_.Bottom() <= y) {
    return;
  }
  const int pix_width = bounding_rect_.Width();
  const size_t index =
      (y - bounding_rect_.Top()) * pix_width + (x - bounding_rect_.Left());
  const BinarySubdivisionalPixel* sub_pixels = pixels_[index].get();
  if (sub_pixels == nullptr) {
    return;
  }
  for (SubdivisionalPixel::SubdivisionalPixelIterator it(x, y); !it.Done();
       it.Next()) {
    if (sub_pixels->test(it.GetIndex())) {
      dest->SetSubdivisionalPixel(it.GetFraction(), text_color_);
    }
  }
}

const Rect& TextLabel::bounding_rect() const { return bounding_rect_; }

}  // namespace internal
}  // namespace win32
}  // namespace renderer
}  // namespace mozc

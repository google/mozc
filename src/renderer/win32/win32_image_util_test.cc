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

#include <atlbase.h>
#include <atltypes.h>
#include <wil/resource.h>

#include <cstdlib>

#undef StrCat

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/protobuf/message.h"
#include "base/protobuf/text_format.h"
#include "base/win32/wide_char.h"
#include "base/win32/win_font_test_helper.h"
#include "data/test/renderer/win32/test_spec.pb.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

using ::std::max;
using ::std::min;

// gdiplus.h must be placed here because it internally depends on
// global min/max functions.
// TODO(yukawa): Use WIC (Windows Imaging Component) instead of GDI+.
#include <gdiplus.h>  // NOLINT

namespace mozc {
namespace renderer {
namespace win32 {
namespace {

using ::mozc::renderer::win32::internal::GaussianBlur;
using ::mozc::renderer::win32::internal::SafeFrameBuffer;
using ::mozc::renderer::win32::internal::SubdivisionalPixel;
using ::mozc::renderer::win32::internal::TextLabel;
using ::mozc::win32::Utf8ToWide;
using ::mozc::win32::WideToUtf8;

using BalloonImageInfo = BalloonImage::BalloonImageInfo;
using SubdivisionalPixelIterator =
    SubdivisionalPixel::SubdivisionalPixelIterator;
using TestSpec = ::mozc::test::renderer::win32::TestSpec;

class BalloonImageTest : public ::testing::Test,
                         public ::testing::WithParamInterface<const char*> {
 public:
  static void SetUpTestCase() {
    InitGdiplus();

    // On Windows XP, the availability of typical Japanese fonts such are as
    // MS Gothic depends on the language edition and language packs.
    // So we will register a private font for unit test.
    EXPECT_TRUE(WinFontTestHelper::Initialize());
  }

  static void TearDownTestCase() {
    // Free private fonts although the system automatically frees them when
    // this process is terminated.
    WinFontTestHelper::Uninitialize();

    UninitGdiplus();
  }

 protected:
  class TestableBalloonImage : public BalloonImage {
   public:
    using BalloonImage::CreateInternal;
  };

  static void SaveTestImage(const BalloonImageInfo& info,
                            const std::wstring& filename) {
    CPoint tail_offset;
    CSize size;
    std::vector<ARGBColor> buffer;
    wil::unique_hbitmap dib(TestableBalloonImage::CreateInternal(
        info, &tail_offset, &size, &buffer));

    TestSpec spec = TestSpec();
    BalloonInfoToTextProto(info, &spec);
    spec.mutable_output()->set_tail_offset_x(tail_offset.x);
    spec.mutable_output()->set_tail_offset_y(tail_offset.y);

    Gdiplus::Bitmap bitmap(size.cx, size.cy);
    for (size_t y = 0; y < size.cy; ++y) {
      for (size_t x = 0; x < size.cx; ++x) {
        ARGBColor argb = buffer[y * size.cx + x];
        const Gdiplus::Color color(argb.a, argb.r, argb.g, argb.b);
        bitmap.SetPixel(x, y, color);
      }
    }

    bitmap.Save(filename.c_str(), &clsid_png_);

    OutputFileStream os(absl::StrCat(WideToUtf8(filename), ".textproto"));
    os << ::mozc::protobuf::Utf8Format(spec);
  }

  static void BalloonInfoToTextProto(const BalloonImageInfo& info,
                                     TestSpec* spec) {
    TestSpec::Input* input = spec->mutable_input();
    input->set_frame_color(ColorToInteger(info.frame_color));
    input->set_inside_color(ColorToInteger(info.inside_color));
    input->set_label_color(ColorToInteger(info.label_color));
    input->set_blur_color(ColorToInteger(info.blur_color));
    input->set_blur_alpha(info.blur_alpha);
    input->set_label_size(info.label_size);
    input->set_label_font(info.label_font);
    input->set_label(info.label);
    input->set_rect_width(info.rect_width);
    input->set_rect_height(info.rect_height);
    input->set_frame_thickness(info.frame_thickness);
    input->set_corner_radius(info.corner_radius);
    input->set_tail_height(info.tail_height);
    input->set_tail_width(info.tail_width);
    input->set_tail_direction(TranslateEnum(info.tail_direction));
    input->set_blur_sigma(info.blur_sigma);
    input->set_blur_offset_x(info.blur_offset_x);
    input->set_blur_offset_y(info.blur_offset_y);
  }

  static void TextProtoToBalloonInfo(const TestSpec& spec,
                                     BalloonImageInfo* info) {
    const TestSpec::Input& input = spec.input();
    *info = BalloonImageInfo();
    info->frame_color = IntegerToColor(input.frame_color());
    info->inside_color = IntegerToColor(input.inside_color());
    info->label_color = IntegerToColor(input.label_color());
    info->blur_color = IntegerToColor(input.blur_color());
    info->blur_alpha = input.blur_alpha();
    info->label_size = input.label_size();
    info->label_font = input.label_font();
    info->label = input.label();
    info->rect_width = input.rect_width();
    info->rect_height = input.rect_height();
    info->frame_thickness = input.frame_thickness();
    info->corner_radius = input.corner_radius();
    info->tail_height = input.tail_height();
    info->tail_width = input.tail_width();
    info->tail_direction = TranslateEnum(input.tail_direction());
    info->blur_sigma = input.blur_sigma();
    info->blur_offset_x = input.blur_offset_x();
    info->blur_offset_y = input.blur_offset_y();
  }

 private:
  static bool GetEncoderClsid(const std::wstring format, CLSID* clsid) {
    UINT num_codecs = 0;
    UINT codecs_buffer_size = 0;
    Gdiplus::GetImageEncodersSize(&num_codecs, &codecs_buffer_size);
    if (codecs_buffer_size == 0) {
      return false;
    }

    std::unique_ptr<uint8_t[]> codesc_buffer(new uint8_t[codecs_buffer_size]);
    Gdiplus::ImageCodecInfo* codecs =
        reinterpret_cast<Gdiplus::ImageCodecInfo*>(codesc_buffer.get());

    Gdiplus::GetImageEncoders(num_codecs, codecs_buffer_size, codecs);
    for (size_t i = 0; i < num_codecs; ++i) {
      const Gdiplus::ImageCodecInfo& info = codecs[i];
      if (format == info.MimeType) {
        *clsid = info.Clsid;
        return true;
      }
    }
    return false;
  }

  static void InitGdiplus() {
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&gdiplus_token_, &input, nullptr);

    if (!GetEncoderClsid(L"image/png", &clsid_png_)) {
      clsid_png_ = CLSID_NULL;
    }
    if (!GetEncoderClsid(L"image/bmp", &clsid_bmp_)) {
      clsid_bmp_ = CLSID_NULL;
    }
  }

  static void UninitGdiplus() { Gdiplus::GdiplusShutdown(gdiplus_token_); }

  static uint32_t ColorToInteger(RGBColor color) {
    return static_cast<uint32_t>(color.r) << 16 |
           static_cast<uint32_t>(color.g) << 8 | static_cast<uint32_t>(color.b);
  }

  static RGBColor IntegerToColor(uint32_t color) {
    return RGBColor((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
  }

  static TestSpec::TailDirection TranslateEnum(
      BalloonImageInfo::TailDirection direction) {
    switch (direction) {
      case BalloonImageInfo::kTop:
        return TestSpec::TOP;
      case BalloonImageInfo::kBottom:
        return TestSpec::BOTTOM;
      case BalloonImageInfo::kLeft:
        return TestSpec::LEFT;
      case BalloonImageInfo::kRight:
        return TestSpec::RIGHT;
      default:
        LOG(FATAL) << "Unexpected direction: " << direction;
    }
  }

  static BalloonImageInfo::TailDirection TranslateEnum(
      TestSpec::TailDirection direction) {
    switch (direction) {
      case TestSpec::UNSPECIFIED:
        LOG(FATAL) << "TailDirection must be set.";
      case TestSpec::TOP:
        return BalloonImageInfo::TailDirection::kTop;
      case TestSpec::BOTTOM:
        return BalloonImageInfo::TailDirection::kBottom;
      case TestSpec::LEFT:
        return BalloonImageInfo::TailDirection::kLeft;
      case TestSpec::RIGHT:
        return BalloonImageInfo::TailDirection::kRight;
      default:
        LOG(FATAL) << "Unexpected direction: " << direction;
    }
  }

  static CLSID clsid_png_;
  static CLSID clsid_bmp_;

  static ULONG_PTR gdiplus_token_;
};

CLSID BalloonImageTest::clsid_png_;
CLSID BalloonImageTest::clsid_bmp_;
ULONG_PTR BalloonImageTest::gdiplus_token_;

// Tests should be passed.
const char* kRenderingResultList[] = {
    "balloon_blur_alpha_-1.png",
    "balloon_blur_alpha_0.png",
    "balloon_blur_alpha_10.png",
    "balloon_blur_color_32_64_128.png",
    "balloon_blur_offset_-20_-10.png",
    "balloon_blur_offset_0_0.png",
    "balloon_blur_offset_20_5.png",
    "balloon_blur_sigma_0.0.png",
    "balloon_blur_sigma_0.5.png",
    "balloon_blur_sigma_1.0.png",
    "balloon_blur_sigma_2.0.png",
    "balloon_frame_thickness_-1.png",
    "balloon_frame_thickness_0.png",
    "balloon_frame_thickness_1.5.png",
    "balloon_frame_thickness_3.png",
    "balloon_inside_color_32_64_128.png",
    "balloon_no_label.png",
    "balloon_tail_bottom.png",
    "balloon_tail_left.png",
    "balloon_tail_right.png",
    "balloon_tail_top.png",
    "balloon_tail_width_height_-10_-10.png",
    "balloon_tail_width_height_0_0.png",
    "balloon_tail_width_height_10_20.png",
    "balloon_width_height_40_30.png",
};

INSTANTIATE_TEST_CASE_P(BalloonImageParameters, BalloonImageTest,
                        ::testing::ValuesIn(kRenderingResultList));

TEST_P(BalloonImageTest, TestImpl) {
  const std::string& expected_image_path = mozc::testing::GetSourceFileOrDie(
      {"data", "test", "renderer", "win32", GetParam()});
  const std::string textproto_path = expected_image_path + ".textproto";
  ASSERT_OK(FileUtil::FileExists(textproto_path))
      << "Manifest file is not found: " << textproto_path;

  TestSpec spec;
  {
    absl::StatusOr<std::string> data = FileUtil::GetContents(textproto_path);
    ASSERT_OK(data);
    ASSERT_TRUE(
        mozc::protobuf::TextFormat::ParseFromString(data.value(), &spec));
  }
  BalloonImageInfo info;
  TextProtoToBalloonInfo(spec, &info);

  CPoint actual_tail_offset;
  CSize actual_size;
  std::vector<ARGBColor> actual_buffer;
  wil::unique_hbitmap dib(TestableBalloonImage::CreateInternal(
      info, &actual_tail_offset, &actual_size, &actual_buffer));

  EXPECT_EQ(actual_tail_offset.x, spec.output().tail_offset_x());
  EXPECT_EQ(actual_tail_offset.y, spec.output().tail_offset_y());

  Gdiplus::Bitmap bitmap(Utf8ToWide(expected_image_path).c_str());

  ASSERT_EQ(actual_size.cx, bitmap.GetWidth());
  ASSERT_EQ(actual_size.cy, bitmap.GetHeight());

  for (size_t y = 0; y < actual_size.cy; ++y) {
    for (size_t x = 0; x < actual_size.cx; ++x) {
      ARGBColor argb = actual_buffer[y * actual_size.cx + x];
      Gdiplus::Color color;
      ASSERT_EQ(bitmap.GetPixel(x, y, &color), Gdiplus::Ok);
      EXPECT_EQ(argb.a, color.GetA()) << "(x, y): (" << x << ", " << y << ")";
      EXPECT_EQ(argb.r, color.GetR()) << "(x, y): (" << x << ", " << y << ")";
      EXPECT_EQ(argb.g, color.GetG()) << "(x, y): (" << x << ", " << y << ")";
      EXPECT_EQ(argb.b, color.GetB()) << "(x, y): (" << x << ", " << y << ")";
    }
  }
}

TEST(RGBColorTest, BasicTest) {
  EXPECT_NE(RGBColor::kBlack, RGBColor::kWhite);
  EXPECT_EQ(RGBColor::kWhite, RGBColor::kWhite);
}

TEST(ARGBColorTest, BasicTest) {
  EXPECT_NE(ARGBColor::kBlack, ARGBColor::kWhite);
  EXPECT_EQ(ARGBColor::kWhite, ARGBColor::kWhite);
}

TEST(SubdivisionalPixelTest, BasicTest) {
  const RGBColor kBlue(0, 0, 255);
  const RGBColor kGreen(0, 255, 0);

  SubdivisionalPixel sub_pixel;
  EXPECT_EQ(sub_pixel.GetCoverage(), 0.0)
      << "Should be zero for an empty pixel";
  EXPECT_EQ(sub_pixel.GetPixelColor(), RGBColor::kBlack)
      << "Should be black for an empty pixel";

  // SetSubdivisionalPixel sets only sub-pixel specified.
  sub_pixel.SetSubdivisionalPixel(SubdivisionalPixel::Fraction2D(0, 0),
                                  RGBColor::kWhite);
  EXPECT_NEAR(1.0 / 255.0, sub_pixel.GetCoverage(), 0.1);
  EXPECT_EQ(sub_pixel.GetPixelColor(), RGBColor::kWhite);

  sub_pixel.SetColorToFilledPixels(kGreen);
  EXPECT_NEAR(1.0 / 255.0, sub_pixel.GetCoverage(), 0.1);
  EXPECT_EQ(sub_pixel.GetPixelColor(), kGreen);

  // SetPixel sets all the sub-pixels.
  sub_pixel.SetPixel(kBlue);
  EXPECT_NEAR(1.0, sub_pixel.GetCoverage(), 0.01);
  EXPECT_EQ(sub_pixel.GetPixelColor(), kBlue);

  sub_pixel.SetSubdivisionalPixel(SubdivisionalPixel::Fraction2D(0, 0),
                                  RGBColor::kWhite);
  EXPECT_NEAR(1.0, sub_pixel.GetCoverage(), 0.01);
  EXPECT_EQ(sub_pixel.GetPixelColor().r, 1);

  sub_pixel.SetColorToFilledPixels(kBlue);
  EXPECT_NEAR(1.0, sub_pixel.GetCoverage(), 0.01);
  EXPECT_EQ(sub_pixel.GetPixelColor(), kBlue);
}

TEST(SubdivisionalPixelTest, IteratorTest) {
  const RGBColor kBlue(0, 0, 255);

  SubdivisionalPixel sub_pixel;
  size_t count = 0;
  for (SubdivisionalPixelIterator it(0, 0); !it.Done(); it.Next()) {
    EXPECT_LE(0, it.GetFraction().x);
    EXPECT_LE(0, it.GetFraction().y);
    EXPECT_GT(SubdivisionalPixel::kDivision, it.GetFraction().x);
    EXPECT_GT(SubdivisionalPixel::kDivision, it.GetFraction().y);
    EXPECT_LE(0.0, it.GetX());
    EXPECT_LE(0.0, it.GetY());
    EXPECT_GE(1.0, it.GetX());
    EXPECT_GE(1.0, it.GetY());
    ++count;
  }
  EXPECT_EQ(count, SubdivisionalPixel::kTotalPixels);
}

TEST(GaussianBlurTest, NoBlurTest) {
  // When Gaussian blur sigma is 0.0, no blur effect should be applied.
  GaussianBlur blur(0.0);

  struct Source {
    Source() : call_count_(0) {}
    double operator()(int x, int y) const {
      EXPECT_EQ(x, 0);
      EXPECT_EQ(y, 0);
      ++call_count_;
      return 1.0;
    }
    mutable int call_count_;
  };

  Source source;
  EXPECT_EQ(blur.Apply(0, 0, source), 1.0);
  EXPECT_EQ(source.call_count_, 1);
}

TEST(GaussianBlurTest, InvalidParamBlurTest) {
  // When Gaussian blur sigma is invalid (a negative value), no blur effect
  // should be applied.

  GaussianBlur blur(-100.0);
  struct Source {
    Source() : call_count_(0) {}
    double operator()(int x, int y) const {
      EXPECT_EQ(x, 0);
      EXPECT_EQ(y, 0);
      ++call_count_;
      return 1.0;
    }
    mutable int call_count_;
  };

  Source source;
  EXPECT_EQ(blur.Apply(0, 0, source), 1.0);
  EXPECT_EQ(source.call_count_, 1);
}

TEST(GaussianBlurTest, NormalBlurTest) {
  GaussianBlur blur(1.0);
  struct Source {
    explicit Source(int cutoff_length)
        : call_count_(0), cutoff_length_(cutoff_length) {}
    double operator()(int x, int y) const {
      EXPECT_GE(cutoff_length_, abs(x));
      EXPECT_GE(cutoff_length_, abs(y));
      ++call_count_;
      return 1.0;
    }
    mutable size_t call_count_;
    int cutoff_length_;
  };

  Source source(blur.cutoff_length());
  EXPECT_NEAR(1.0, blur.Apply(0, 0, source), 0.1);
  const size_t matrix_length = blur.cutoff_length() * 2 + 1;
  EXPECT_EQ(source.call_count_, matrix_length * matrix_length);
}

TEST(SafeFrameBufferTest, BasicTest) {
  const ARGBColor kTransparent(0, 0, 0, 0);
  constexpr int kLeft = -10;
  constexpr int kTop = -20;
  constexpr int kWidth = 50;
  constexpr int kHeight = 100;
  SafeFrameBuffer buffer(Rect(kLeft, kTop, kWidth, kHeight));

  EXPECT_EQ(buffer.GetPixel(kLeft, kTop), kTransparent)
      << "Initial color should be transparent";
  buffer.SetPixel(kLeft, kTop, ARGBColor::kWhite);
  EXPECT_EQ(buffer.GetPixel(kLeft, kTop), ARGBColor::kWhite);

  buffer.SetPixel(kLeft + kWidth, kTop, ARGBColor::kWhite);
  EXPECT_EQ(buffer.GetPixel(kLeft + kWidth, kTop), kTransparent)
      << "(left + width) is outside.";

  buffer.SetPixel(kLeft, kTop + kHeight, ARGBColor::kWhite);
  EXPECT_EQ(buffer.GetPixel(kLeft, kTop + kHeight), kTransparent)
      << "(top + height) is outside.";

  buffer.SetPixel(kLeft - 10, kTop - 10, ARGBColor::kWhite);
  EXPECT_EQ(buffer.GetPixel(kLeft - 10, kTop - 10), kTransparent)
      << "Outside pixel should be kept as transparent.";
}

TEST(TextLabelTest, BoundingBoxTest) {
  const TextLabel label(-10.5, -5.1, 10.5, 5.0, "text", "font name", 10,
                        RGBColor::kWhite);
  EXPECT_EQ(label.bounding_rect().Left(), -11);
  EXPECT_EQ(label.bounding_rect().Top(), -6);
  EXPECT_EQ(label.bounding_rect().Right(), 0);
  EXPECT_EQ(label.bounding_rect().Bottom(), 0);
}

}  // namespace
}  // namespace win32
}  // namespace renderer
}  // namespace mozc

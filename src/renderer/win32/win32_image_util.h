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

#ifndef MOZC_RENDERER_WIN32_WIN32_IMAGE_UTIL_H_
#define MOZC_RENDERER_WIN32_WIN32_IMAGE_UTIL_H_

#include <windows.h>

#include <bitset>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/coordinates.h"

namespace mozc {
namespace renderer {
namespace win32 {

// A 24 bit-per-pixel format that is used in BalloonImage generator.
// When we enable Skia as a back-end of graphics system, we might want to
// migrate to Skia's pixel format instead of our own ones.
struct RGBColor {
  typedef uint8_t ValueType;
  ValueType r;
  ValueType g;
  ValueType b;
  RGBColor();
  RGBColor(ValueType red, ValueType green, ValueType blue);

  bool operator==(const RGBColor& that) const;
  bool operator!=(const RGBColor& that) const;
  static const RGBColor kBlack;
  static const RGBColor kWhite;
};

// A 32 bit-per-pixel format that is used in BalloonImage generator.
// When we enable Skia as a back-end of graphics system, we might want to
// migrate to Skia's pixel format instead of our own ones.
struct ARGBColor {
  typedef uint8_t ValueType;
  ValueType a;
  ValueType r;
  ValueType g;
  ValueType b;
  ARGBColor();
  ARGBColor(ValueType alpha, ValueType red, ValueType green, ValueType blue);
  ARGBColor(const RGBColor& color, ValueType alpha);

  bool operator==(const ARGBColor& that) const;
  bool operator!=(const ARGBColor& that) const;
  static const ARGBColor kBlack;
  static const ARGBColor kWhite;
};

// A utility class to generate balloon-like image based on various parameters.
// You can generate the following shapes with this class.
// - Rectangle w/ and w/o balloon tail
// - Rounded rectangle w/ and w/o balloon tail
// - Circle
// You can put text label inside the balloon.
// You can also put 2D Gaussian blur effect with arbitrary color and opacity.
// See the comments of BalloonImageInfo for details.
class BalloonImage {
 public:
  struct BalloonImageInfo {
    enum TailDirection {
      kTop = 0,
      kRight,
      kBottom,
      kLeft,
    };

    BalloonImageInfo();

    RGBColor frame_color;
    RGBColor inside_color;
    RGBColor label_color;
    RGBColor blur_color;
    // Factor to blur color as a factor in [0.0, 1.0].
    double blur_alpha;
    // Size of the label text in points.
    int label_size;
    // Font name of the label text.
    std::string label_font;
    // Label text in UTF-8.
    std::string label;
    // Width of the bounding box of the balloon except for its tail.
    double rect_width;
    // Height of the bounding box of the balloon except for its tail.
    double rect_height;
    // Frame thickness in pixels. Set 0.0 to render a frame-less balloon.
    double frame_thickness;
    // Corner radius in pixels. Set 0.0 to render a solid balloon.
    double corner_radius;
    // Height of the tail in pixels. This is vertical length if
    // |tail_direction| is kTop or kBottom and horizontal length if
    // that is kRight or kLeft. Set 0.0 to render a tail-less balloon.
    double tail_height;
    // Width of the tail in pixels. This is horizontal length if
    // |tail_direction| is kTop or kBottom and vertical length if that is
    // kRight or kLeft. Set 0.0 to render a tail-less balloon.
    double tail_width;
    TailDirection tail_direction;
    // Sigma parameter in pixels of the 2D Gaussian function. Set 0 to disable
    // blur effect.
    double blur_sigma;
    // Horizontal offset in pixels with which blur image is placed. A positive
    // offset moves the blur rightward (positive way in X coordinate).
    int blur_offset_x;
    // Vertical offset in pixels with which blur image is placed. A positive
    // offset moves the blur downward (positive way in Y coordinate).
    int blur_offset_y;
  };

  BalloonImage() = delete;
  BalloonImage(const BalloonImage&) = delete;
  BalloonImage& operator=(const BalloonImage&) = delete;

  // Returns a bitmap handle to a DIB section that contains generated balloon
  // image. Returns nullptr if fails.
  // |tail_offset| is an offset pixels from the top-left corner of the bitmap.
  // Note that the returned image is premultiplied-alpha format because GDI APIs
  // including Layered Window APIs expect this format.
  static HBITMAP Create(const BalloonImageInfo& info, POINT* tail_offset);

 protected:
  // A variant of Create method only for unit test. You can specify the label
  // font and retrieve the rendering result as ARGB image. Note that
  // |arbg_buffer| is not premultiplied-alpha format so that we can compare
  // the rendering result with expected ones more precisely than PBGRA format
  // used in the returned bitmap handle.
  static HBITMAP CreateInternal(const BalloonImageInfo& info,
                                POINT* tail_offset, SIZE* size,
                                std::vector<ARGBColor>* arbg_buffer);
};

// Following types are declared in this header so that unit test can access
// them. You should not use these classes directly.
namespace internal {

// A container of pixels that is useful to implement over-sampling-based
// anti-aliasing, where this class can be used as a virtual pixel. Basic concept
// of the implementation is simple: each empty sub-pixel in this container is
// concidered as a *transparent" area. It means that the opacity of the entire
// region can be calculated as the ratio of non-empty pixels to all pixels.
// This interpretation is consistent with the typical alpha blending function
//   C = (1 - a) Cb + a Cf,
// where Cb is background color and Cf is foreground color and a is the alpha
// factor and C is the final visible color. This class can calculate 'a' and
// 'Cf'
class SubdivisionalPixel {
 public:
  typedef RGBColor ColorType;
  static const size_t kDivision = 16;
  static const size_t kTotalPixels = kDivision * kDivision;

  // A pair of indices to specify the coordinate of each sub-pixel. That is
  // [0, SubdivisionalPixel::kDivision) x [0, SubdivisionalPixel::kDivision)
  struct Fraction2D {
   public:
    Fraction2D();
    Fraction2D(size_t x_frac, size_t y_frac);
    static const size_t kDivision = SubdivisionalPixel::kDivision;
    const size_t x;
    const size_t y;
  };

  // An iterator to enumerate each sub-pixel in left-right, right-bottom order.
  class SubdivisionalPixelIterator {
   public:
    // The pair of |base_x| and |base_y| is the position of (0, 0).
    SubdivisionalPixelIterator(int base_x, int base_y);

    // Returns the indices of the current sub-pixel.
    Fraction2D GetFraction() const;

    // Returns the center of the current sub-pixel.
    double GetX() const;
    double GetY() const;

    size_t GetIndex() const;

    void Next();
    bool Done() const;

   private:
    const int base_x_;
    const int base_y_;
    size_t numerator_x_;
    size_t numerator_y_;
  };

  SubdivisionalPixel();
  SubdivisionalPixel(const SubdivisionalPixel&) = delete;
  SubdivisionalPixel& operator=(const SubdivisionalPixel&) = delete;

  // Returns the coverage of this entire region as [0.0, 1.0].
  const double GetCoverage() const;
  // Returns the pixel color as the mean of filled sub-pixels.
  const ColorType GetPixelColor() const;

  // Sets |color| to all the sub-pixels.
  void SetPixel(const ColorType& color);

  // Sets |color| to one sub-pixel specified by |frac|.
  void SetSubdivisionalPixel(const Fraction2D& frac, const ColorType& color);

  // Sets |color| to all the filled sub-pixels.
  void SetColorToFilledPixels(const ColorType& color);

 private:
  enum FillType {
    kEmpty,
    kSingleColor,
    kMultipleColors,
  };
  FillType GetFillType() const;

  static size_t GetIndex(const Fraction2D& offset);

  // A bit vector that indicates each sub-pixel is filled or not.
  std::bitset<kTotalPixels> filled_;
  std::unique_ptr<ColorType[]> colors_;
  ColorType single_color_;
};

// An implementation of Gaussian blur filter.
class GaussianBlur {
 public:
  // Sigma parameter in pixels of the 2D Gaussian function. Set 0 to disable
  // blur effect.
  explicit GaussianBlur(double sigma);

  GaussianBlur(const GaussianBlur&) = delete;
  GaussianBlur& operator=(const GaussianBlur&) = delete;

  // Returns the cut-off length to construct the convolution matrix. If the
  // returned value is x, (2 * x + 1)^2 matrix will be used.
  int cutoff_length() const;

  // Returns the blurred value of |f(x, y)|, where |f| can be any function-like
  // object.
  template <typename Function>
  double Apply(int x, int y, const Function& f) const {
    double sum = 0.0;
    for (Matrix::const_iterator it = matrix_.begin(); it != matrix_.end();
         ++it) {
      sum += it->coefficient * f(x + it->offset_x, y + it->offset_y);
    }
    return sum;
  }

 private:
  // An element type of convolution matrix.
  struct MatrixElement {
    int offset_x;
    int offset_y;
    double coefficient;
    MatrixElement();
    MatrixElement(int x, int y, double c);
  };
  typedef std::vector<MatrixElement> Matrix;

  size_t GetMatrixLength() const;

  const double sigma_;
  const int cutoff_length_;
  Matrix matrix_;
};

// A virtual 2D container of ARGB pixels where out-of-range pixels are treated
// as read-only and transparent.
class SafeFrameBuffer {
 public:
  // Initializes the frame buffer with real backing store as follows.
  //     [left, left + width) x [top, top + height)
  explicit SafeFrameBuffer(const Rect& rect);

  SafeFrameBuffer(const SafeFrameBuffer&) = delete;
  SafeFrameBuffer& operator=(const SafeFrameBuffer&) = delete;

  // Returns the color of the specified pixel. If the pixel is out-of-window,
  // returns a transparent black.
  ARGBColor GetPixel(int x, int y) const;

  // Sets the color into the specified pixel. If it pixel out-of-window, does
  // nothing.
  void SetPixel(int x, int y, const ARGBColor& color);

 private:
  bool IsInWindow(int x, int y) const;
  size_t GetIndex(int x, int y) const;
  const Rect rect_;
  std::unique_ptr<ARGBColor[]> buffer_;
};

// A text rendering utility class that utilize sub-pixel rendering and can
// output the result to SubdivisionalPixel storage.
class TextLabel {
 public:
  typedef std::bitset<SubdivisionalPixel::kDivision *
                      SubdivisionalPixel::kDivision>
      BinarySubdivisionalPixel;

  TextLabel(double left, double top, double width, double height,
            const std::string& text, const std::string& font, size_t font_point,
            const RGBColor text_color);
  TextLabel(const TextLabel&) = delete;
  TextLabel& operator=(const TextLabel&) = delete;
  ~TextLabel();

  // Copies the pixel specified by |x| and |y| to |dest|. Does nothing if the
  // pixel is empty.
  void RenderPixel(int x, int y, SubdivisionalPixel* dest) const;

  // Returns bounding box.
  const Rect& bounding_rect() const;

 private:
  const std::vector<std::unique_ptr<BinarySubdivisionalPixel>> pixels_;
  const Rect bounding_rect_;
  const RGBColor text_color_;
};

}  // namespace internal
}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_WIN32_IMAGE_UTIL_H_

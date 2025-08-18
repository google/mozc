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

// Platform-independent structures for points, sizes, and rectangles
#ifndef MOZC_BASE_COORDINATES_H_
#define MOZC_BASE_COORDINATES_H_

namespace mozc {
struct Point {
  explicit Point(int newx = 0, int newy = 0) : x(newx), y(newy) {}

  int x;
  int y;
};

struct Size {
  explicit Size(int newwidth = 0, int newheight = 0)
      : width(newwidth), height(newheight) {}

  int width;
  int height;
};

struct Rect {
  Rect() = default;
  Rect(int newx, int newy, int newwidth, int newheight)
      : origin(newx, newy), size(newwidth, newheight) {}
  Rect(const Point& o, const Size& s) : origin(o), size(s) {}

  // Accessors
  inline int Width() const { return size.width; }
  inline int Height() const { return size.height; }
  inline int Left() const { return origin.x; }
  inline int Top() const { return origin.y; }
  inline int Right() const { return origin.x + size.width; }
  inline int Bottom() const { return origin.y + size.height; }

  // Other methods
  inline void DeflateRect(int l, int t, int r, int b) {
    origin.x += l;
    origin.y += t;
    size.width -= l + r;
    size.height -= t + b;
  }
  inline void DeflateRect(int x, int y) { DeflateRect(x, y, x, y); }
  inline void DeflateRect(const Size& s) { DeflateRect(s.width, s.height); }
  // Returns true if the right side is less than or equal to the coordinate
  // of the left side, or the coordinate of the bottom side is less than or
  // equal to the coordinate of the top side.
  // This behaviour is compatible to the IsRectEmpty API
  // http://msdn.microsoft.com/en-us/library/dd145017.aspx
  inline bool IsRectEmpty() const {
    return size.width <= 0 || size.height <= 0;
  }
  inline bool PtrInRect(const Point& p) const {
    return p.x >= origin.x && p.x <= origin.x + size.width && p.y >= origin.y &&
           p.y <= origin.y + size.height;
  }

  Point origin;
  Size size;
};
}  // namespace mozc
#endif  // MOZC_BASE_COORDINATES_H_

// Copyright 2010-2013, Google Inc.
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

#ifndef MOZC_RENDERER_UNIX_DRAW_TOOL_INTERFACE_H_
#define MOZC_RENDERER_UNIX_DRAW_TOOL_INTERFACE_H_

#include <gtk/gtk.h>

#include "base/port.h"

namespace mozc {

struct Point;
struct Rect;

namespace renderer {
namespace gtk {

struct RGBA;
class CairoWrapperInterface;

class DrawToolInterface {
 public:
  explicit DrawToolInterface() {}
  virtual ~DrawToolInterface() {}

  // Resets internal cairo context to given one.
  virtual void Reset(CairoWrapperInterface *cairo) = 0;

  // To use double buffering, call Save function before call drawing operations,
  // and call Restore function after all drawing operation is finished.
  virtual void Save() = 0;
  virtual void Restore() = 0;

  // Draws rectangle and fill inside of it with spcified color.
  virtual void FillRect(const Rect &rect, const RGBA &color) = 0;

  // Draws rectangle with specified color.
  virtual void FrameRect(const Rect &rect,
                         const RGBA &color,
                         const uint32 line_width) = 0;

  // Draws line with specified color.
  virtual void DrawLine(const Point &from,
                        const Point &to,
                        const RGBA &color,
                        const uint32 line_width) = 0;
};
}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_UNIX_DRAW_TOOL_INTERFACE_H_

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

#ifndef MOZC_RENDERER_UNIX_DRAW_TOOL_H_
#define MOZC_RENDERER_UNIX_DRAW_TOOL_H_
#include <gtk/gtk.h>

#include "base/base.h"
#include "base/port.h"
#include "renderer/unix/cairo_wrapper_interface.h"
#include "renderer/unix/draw_tool_interface.h"

namespace mozc {
namespace renderer {
namespace gtk {

class DrawToolTest;

class DrawTool : public DrawToolInterface {
 public:
  explicit DrawTool() {}
  virtual ~DrawTool() {}
  virtual void Save();
  virtual void Restore();
  virtual void FillRect(const Rect &rect, const RGBA &color);

  virtual void FrameRect(const Rect &rect,
                         const RGBA &color,
                         const uint32 line_width);

  virtual void DrawLine(const Point &from,
                        const Point &to,
                        const RGBA &color,
                        const uint32 line_width);

  // DrawTool class takes ownership of CairoWrapper.
  virtual void Reset(CairoWrapperInterface *cairo);
 private:
  friend class DrawToolTest;
  void SetColor(const RGBA &color);
  scoped_ptr<CairoWrapperInterface> cairo_;
  DISALLOW_COPY_AND_ASSIGN(DrawTool);
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_UNIX_DRAW_TOOL_H_

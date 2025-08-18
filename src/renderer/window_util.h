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

// WindowUtil: OS-independent utility functions to manage candidate windows

#ifndef MOZC_RENDERER_WINDOW_UTIL_H_
#define MOZC_RENDERER_WINDOW_UTIL_H_

#include "base/coordinates.h"

namespace mozc {
namespace renderer {

class WindowUtil {
 public:
  // Returns the appropriate candidate window position in the screen
  // coordinate.  |zero_point_offset| is the point in the candidate
  // window which should be aligned to the preedit.
  // |working_area| is the available area in the current monitor.  If
  // caller fails to obtain |working_area|, set its width or height as
  // 0.  Then it doesn't care the monitor.
  static Rect GetWindowRectForMainWindowFromPreeditRect(
      const Rect& preedit_rect, const Size& window_size,
      const Point& zero_point_offset, const Rect& working_area);

  // Returns the appropriate candidate window position in the screen
  // coordinate.  |zero_point_offset| is the point in the candidate
  // window which should be aligned to the target point.
  // |working_area| is the available area in the current monitor.  If
  // caller fails to obtain |working_area|, set its width or height as
  // 0.  Then it doesn't care the monitor.
  static Rect GetWindowRectForMainWindowFromTargetPoint(
      const Point& target_point, const Size& window_size,
      const Point& zero_point_offset, const Rect& working_area);

  // Returns the appropriate candidate window position in the screen
  // coordinate.  |zero_point_offset| is the point in the candidate
  // window which should be aligned to the preedit.
  // |working_area| is the available area in the current monitor.  If
  // caller fails to obtain |working_area|, set its width or height as
  // 0.  Then it doesn't care the monitor.
  static Rect GetWindowRectForMainWindowFromTargetPointAndPreedit(
      const Point& target_point, const Rect& preedit_rect,
      const Size& window_size, const Point& zero_point_offset,
      const Rect& working_area, bool vertical);

  // Returns the appropriate cascading window position in the screen
  // coordinate.  |zero_point_offset| is the point in the cascading
  // window which should be aligned to the selected row in the
  // candidate window.
  // |working_area| is the available area in the current monitor.  If
  // caller fails to obtain |working_area|, set its width or height as
  // 0.  Then it doesn't care the monitor.
  static Rect GetWindowRectForCascadingWindow(const Rect& selected_row,
                                              const Size& window_size,
                                              const Point& zero_point_offset,
                                              const Rect& working_area);

  // Returns the appropriate infolist window position in the screen
  // coordinate.  |window_size| is the size of the infolist window.
  // |candidate_rect| is the rect of the candidate window.
  // |working_area| is the available area in the current monitor.  If
  // caller fails to obtain |working_area|, set its width or height as
  // 0.  Then it doesn't care the monitor.
  static Rect GetWindowRectForInfolistWindow(const Size& window_size,
                                             const Rect& candidate_rect,
                                             const Rect& working_area);
};
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WINDOW_UTIL_H_

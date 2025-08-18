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

#include "renderer/window_util.h"

#include "base/coordinates.h"

namespace mozc {
namespace renderer {
namespace {
Rect GetWindowRectForMainWindowFromPreeditRectHorizontal(
    const Point& target_point, const Rect& preedit_rect,
    const Size& window_size, const Point& zero_point_offset,
    const Rect& working_area) {
  Rect window_rect(target_point, window_size);
  window_rect.origin.x -= zero_point_offset.x;
  window_rect.origin.y -= zero_point_offset.y;

  // If monitor_rect has erroneous value, it returns window_rect.
  if (working_area.Height() == 0 || working_area.Width() == 0) {
    return window_rect;
  }

  // If the working area below the preedit does not have enough vertical space
  // to display the candidate window, put the candidate window above
  // the preedit.
  if (working_area.Bottom() < window_rect.Bottom()) {
    window_rect.origin.y -= (window_rect.Height() + preedit_rect.Height());
    // We add zero_point_offset.y twice to keep the same distance
    // above the preedit_rect.
    window_rect.origin.y += zero_point_offset.y * 2;
  }

  if (working_area.Bottom() < window_rect.Bottom()) {
    window_rect.origin.y -= (window_rect.Bottom() - working_area.Bottom());
  }

  if (window_rect.Top() < working_area.Top()) {
    window_rect.origin.y += (working_area.Top() - window_rect.Top());
  }

  if (working_area.Right() < window_rect.Right()) {
    window_rect.origin.x -= (window_rect.Right() - working_area.Right());
  }

  if (window_rect.Left() < working_area.Left()) {
    window_rect.origin.x += (working_area.Left() - window_rect.Left());
  }

  return window_rect;
}

Rect GetWindowRectForMainWindowFromPreeditRectVertical(
    const Point& target_point, const Rect& preedit_rect,
    const Size& window_size, const Point& zero_point_offset,
    const Rect& working_area) {
  Rect window_rect(target_point, window_size);

  // Currently |zero_point_offset| is ignored because the candidate renderer
  // has not supported vertical writing.

  // Since |target_point| is pointing the upper-left of the preedit, move the
  // candidate window to the right side of the preedit.
  window_rect.origin.x += preedit_rect.Width();

  // If monitor_rect has erroneous value, it returns window_rect.
  if (working_area.Height() == 0 || working_area.Width() == 0) {
    return window_rect;
  }

  if (working_area.Right() < window_rect.Right()) {
    window_rect.origin.x -= (window_rect.Width() + preedit_rect.Width());
  }

  if (working_area.Right() < window_rect.Right()) {
    window_rect.origin.x -= (window_rect.Right() - working_area.Right());
  }

  if (window_rect.Left() < working_area.Left()) {
    window_rect.origin.x += (working_area.Left() - window_rect.Left());
  }

  if (working_area.Bottom() < window_rect.Bottom()) {
    window_rect.origin.y -= (window_rect.Bottom() - working_area.Bottom());
  }

  if (window_rect.Top() < working_area.Top()) {
    window_rect.origin.y += (working_area.Top() - window_rect.Top());
  }

  return window_rect;
}
}  // namespace

Rect WindowUtil::GetWindowRectForMainWindowFromPreeditRect(
    const Rect& preedit_rect, const Size& window_size,
    const Point& zero_point_offset, const Rect& working_area) {
  const Point preedit_bottom_left(preedit_rect.Left(), preedit_rect.Bottom());

  return GetWindowRectForMainWindowFromPreeditRectHorizontal(
      preedit_bottom_left, preedit_rect, window_size, zero_point_offset,
      working_area);
}

Rect WindowUtil::GetWindowRectForMainWindowFromTargetPoint(
    const Point& target_point, const Size& window_size,
    const Point& zero_point_offset, const Rect& working_area) {
  Rect window_rect(target_point, window_size);
  window_rect.origin.x -= zero_point_offset.x;
  window_rect.origin.y -= zero_point_offset.y;

  // If monitor_rect has erroneous value, it returns window_rect.
  if (working_area.Height() == 0 || working_area.Width() == 0) {
    return window_rect;
  }

  if (working_area.Bottom() < window_rect.Bottom()) {
    window_rect.origin.y -= (window_rect.Bottom() - working_area.Bottom());
  }

  if (window_rect.Top() < working_area.Top()) {
    window_rect.origin.y += (working_area.Top() - window_rect.Top());
  }

  if (working_area.Right() < window_rect.Right()) {
    window_rect.origin.x -= (window_rect.Right() - working_area.Right());
  }

  if (window_rect.Left() < working_area.Left()) {
    window_rect.origin.x += (working_area.Left() - window_rect.Left());
  }

  return window_rect;
}

Rect WindowUtil::GetWindowRectForMainWindowFromTargetPointAndPreedit(
    const Point& target_point, const Rect& preedit_rect,
    const Size& window_size, const Point& zero_point_offset,
    const Rect& working_area, bool vertical) {
  if (vertical) {
    return GetWindowRectForMainWindowFromPreeditRectVertical(
        target_point, preedit_rect, window_size, zero_point_offset,
        working_area);
  }

  return GetWindowRectForMainWindowFromPreeditRectHorizontal(
      target_point, preedit_rect, window_size, zero_point_offset, working_area);
}

Rect WindowUtil::GetWindowRectForCascadingWindow(const Rect& selected_row,
                                                 const Size& window_size,
                                                 const Point& zero_point_offset,
                                                 const Rect& working_area) {
  const Point row_top_right(selected_row.Right(), selected_row.Top());

  Rect window_rect(row_top_right, window_size);
  window_rect.origin.x -= zero_point_offset.x;
  window_rect.origin.y -= zero_point_offset.y;

  if (working_area.Height() == 0 || working_area.Width() == 0) {
    return window_rect;
  }

  // If the working area right to the specified candidate window does not have
  // enough horizontal space to display the cascading window,
  // put the cascading window left to the candidate window.
  if (working_area.Right() < window_rect.Right()) {
    window_rect.origin.x -= (window_rect.Width() + selected_row.Width());
    // We add zero_point_offset.x twice to keep the same distance
    // left of the selected_row.
    window_rect.origin.x += zero_point_offset.x * 2;
  }

  if (working_area.Bottom() < window_rect.Bottom()) {
    window_rect.origin.y -= (window_rect.Bottom() - working_area.Bottom());
  }

  if (window_rect.Top() < working_area.Top()) {
    window_rect.origin.y += (working_area.Top() - window_rect.Top());
  }

  if (window_rect.Left() < working_area.Left()) {
    window_rect.origin.x += (working_area.Left() - window_rect.Left());
  }

  return window_rect;
}

Rect WindowUtil::GetWindowRectForInfolistWindow(const Size& window_size,
                                                const Rect& candidate_rect,
                                                const Rect& working_area) {
  Point infolist_pos;

  if (working_area.Height() == 0 || working_area.Width() == 0) {
    infolist_pos.x = candidate_rect.Left() + candidate_rect.Width();
    infolist_pos.y = candidate_rect.Top();
    return Rect(infolist_pos, window_size);
  }
  if (candidate_rect.Left() + candidate_rect.Width() + window_size.width >
      working_area.Right()) {
    infolist_pos.x = candidate_rect.Left() - window_size.width;
  } else {
    infolist_pos.x = candidate_rect.Left() + candidate_rect.Width();
  }
  if (candidate_rect.Top() + window_size.height > working_area.Bottom()) {
    infolist_pos.y = working_area.Bottom() - window_size.height;
  } else {
    infolist_pos.y = candidate_rect.Top();
  }
  return Rect(infolist_pos, window_size);
}
}  // namespace renderer
}  // namespace mozc

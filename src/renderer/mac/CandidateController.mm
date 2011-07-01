// Copyright 2010-2011, Google Inc.
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

#import "renderer/mac/CandidateView.h"

#include "base/base.h"
#include "renderer/table_layout.h"
#include "renderer/window_util.h"
#include "session/commands.pb.h"
#include "renderer/mac/CandidateController.h"
#include "renderer/mac/CandidateWindow.h"
#include "renderer/mac/InfolistWindow.h"

namespace mozc {
using client::SendCommandInterface;
using commands::RendererCommand;
using commands::Candidates;

namespace renderer {
namespace mac{

namespace {
const int kMarginAbovePreedit = 10; // pixel

// Find the display including the specified point and if it fails to
// find it, pick up the nearest display.  Returns the geometry of the
// found display.
renderer::Rect GetNearestDisplayRect(int x, int y) {
  CGPoint point;
  point.x = x;
  point.y = y;

  CGDisplayCount count = 1;
  CGDirectDisplayID displayID;
  if (CGGetDisplaysWithPoint(point, 1, &displayID, &count) !=
      kCGErrorSuccess || count == 0) {
    // Not found the display which includes the point.  Try to find
    // the nearest display.

    // Passing NULL means getting the number of active displays.
    CGGetActiveDisplayList(0, NULL, &count);

    CGDirectDisplayID displayIDs[count];
    CGGetActiveDisplayList(count, displayIDs, &count);

    CGFloat nearest_distance = FLT_MAX;
    for (int i = 0; i < count; ++i) {
      CGRect displayRect = CGDisplayBounds(displayIDs[i]);
      CGFloat distance = 0;
      if (point.x < displayRect.origin.x) {
        distance += displayRect.origin.x - point.x;
      } else if (point.x > displayRect.origin.x + displayRect.size.width) {
        distance += point.x - displayRect.origin.x - displayRect.size.width;
      }
      if (point.y < displayRect.origin.y) {
        distance += displayRect.origin.y - point.y;
      } else if (point.y > displayRect.origin.y + displayRect.size.height) {
        distance += point.y - displayRect.origin.y - displayRect.size.height;
      }

      if (distance < nearest_distance) {
        displayID = displayIDs[i];
        nearest_distance = distance;
      }
    }
  }

  CGRect display_rect = CGDisplayBounds(displayID);
  return renderer::Rect(display_rect.origin.x, display_rect.origin.y,
                        display_rect.size.width, display_rect.size.height);
}
}  // anonymous namespace


CandidateController::CandidateController()
    : candidate_window_(new mac::CandidateWindow),
      cascading_window_(new mac::CandidateWindow),
      infolist_window_(new mac::InfolistWindow) {
}

CandidateController::~CandidateController() {
  delete candidate_window_;
  delete cascading_window_;
  delete infolist_window_;
}

bool CandidateController::Activate() {
  // Do nothing
  return true;
}

bool CandidateController::IsAvailable() const {
  return true;
}

void CandidateController::SetSendCommandInterface(
    SendCommandInterface *send_command_interface) {
  candidate_window_->SetSendCommandInterface(send_command_interface);
  cascading_window_->SetSendCommandInterface(send_command_interface);
}

bool CandidateController::ExecCommand(const RendererCommand &command) {
  if (command.type() != RendererCommand::UPDATE) {
    return false;
  }
  command_.CopyFrom(command);

  if (!command_.visible()) {
    candidate_window_->Hide();
    cascading_window_->Hide();
    infolist_window_->Hide();
    return true;
  }

  candidate_window_->SetCandidates(command_.output().candidates());

  bool cascading_visible = false;
  if (command_.output().has_candidates() &&
      command_.output().candidates().has_subcandidates()) {
    cascading_window_->SetCandidates(
        command_.output().candidates().subcandidates());
    cascading_visible = true;
  }

  bool infolist_visible = false;
  if (command_.output().has_candidates() &&
      command_.output().candidates().has_usages() &&
      command_.output().candidates().usages().information_size() > 0) {
    infolist_window_->SetCandidates(command_.output().candidates());
    infolist_visible = true;
  }

  AlignWindows();
  candidate_window_->Show();
  if (cascading_visible) {
    cascading_window_->Show();
  } else {
    cascading_window_->Hide();
  }

  if (infolist_visible && !cascading_visible) {
    const Candidates &candidates = command_.output().candidates();
    if (candidates.has_focused_index() && candidates.candidate_size() > 0) {
      const int focused_row =
        candidates.focused_index() - candidates.candidate(0).index();
      if (candidates.candidate_size() >= focused_row &&
          candidates.candidate(focused_row).has_information_id()) {
        infolist_window_->DelayShow();
      } else {
        infolist_window_->DelayHide();
      }
    } else {
      infolist_window_->DelayHide();
    }
  } else {
    // Hide infolist window immediately.
    infolist_window_->Hide();
  }

  return true;
}


void CandidateController::AlignWindows() {
  // If candidate window is not visible, we do nothing for aligning.
  if (!command_.has_preedit_rectangle()) {
    return;
  }

  const renderer::Size preedit_size(command_.preedit_rectangle().right() -
                                    command_.preedit_rectangle().left(),
                                    command_.preedit_rectangle().bottom() -
                                    command_.preedit_rectangle().top());
  renderer::Rect preedit_rect(
      renderer::Point(command_.preedit_rectangle().left(),
                      command_.preedit_rectangle().top()),
      preedit_size);
  // Currently preedit_rect doesn't care about the text height -- it
  // just means the line under the preedit.  So here we fix the height.
  // TODO(mukai): replace this hack by calculating actual text height.
  preedit_rect.origin.y -= kMarginAbovePreedit;
  preedit_rect.size.height += kMarginAbovePreedit;

  // Find out the nearest display.
  const renderer::Rect display_rect = GetNearestDisplayRect(
      preedit_rect.Left(), preedit_rect.Bottom());

  // Align candidate window.
  // Initialize the the position.  We use (left, bottom) of preedit as
  // the top-left position of the window because we want to show the
  // window just below of the preedit.
  const TableLayout *candidate_layout = candidate_window_->GetTableLayout();
  const renderer::Point candidate_zero_point(
      candidate_layout->GetColumnRect(COLUMN_CANDIDATE).Left(), 0);

  const renderer::Rect candidate_rect =
      WindowUtil::GetWindowRectForMainWindowFromPreeditRect(
          preedit_rect, candidate_window_->GetWindowSize(),
          candidate_zero_point, display_rect);
  candidate_window_->MoveWindow(renderer::Point(candidate_rect.Left(),
                                                candidate_rect.Top()));

  // Align infolist window
  const renderer::Size infolist_size = infolist_window_->GetWindowSize();
  renderer::Point infolist_pos;
  if (candidate_rect.Left() + candidate_rect.Width() + infolist_size.width >
      display_rect.Right()) {
    infolist_pos.x = candidate_rect.Left() - infolist_size.width;
  } else {
    infolist_pos.x = candidate_rect.Left() + candidate_rect.Width();
  }
  if (candidate_rect.Top() + infolist_size.height > display_rect.Bottom()) {
    infolist_pos.y = display_rect.Bottom() - infolist_size.height;
  } else {
    infolist_pos.y = candidate_rect.Top();
  }

  infolist_window_->MoveWindow(infolist_pos);

  // If there is no need to show cascading window, we just finish the
  // function here.
  if (!command_.output().has_candidates() ||
      !command_.output().candidates().candidate_size() > 0 ||
      !command_.output().candidates().has_subcandidates()) {
    return;
  }

  // Fix cascading window position
  // 1. starting position is at the focused row
  const Candidates &candidates = command_.output().candidates();
  const int focused_row =
      candidates.focused_index() - candidates.candidate(0).index();
  renderer::Rect focused_rect = candidate_layout->GetRowRect(focused_row);
  // move the focused_rect to the monitor's coordinates
  focused_rect.origin.x += candidate_rect.origin.x;
  focused_rect.origin.y += candidate_rect.origin.y;
  // focused_rect doesn't have the width for scroll bar
  focused_rect.size.width += candidate_layout->GetVScrollBarRect().Width();

  const renderer::Rect cascading_rect =
      WindowUtil::GetWindowRectForCascadingWindow(
          focused_rect, cascading_window_->GetWindowSize(),
          Point(0, 0), display_rect);
  cascading_window_->MoveWindow(renderer::Point(cascading_rect.Left(),
                                                cascading_rect.Top()));
}


}  // namespace mozc::renderer::mac
}  // namespace mozc::renderer
}  // namespace mozc

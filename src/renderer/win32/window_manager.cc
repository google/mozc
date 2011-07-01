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

#include "renderer/win32/window_manager.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE

#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "renderer/coordinates.h"
#include "renderer/renderer_interface.h"
#include "renderer/window_util.h"
#include "renderer/win32/candidate_window.h"
#include "renderer/win32/composition_window.h"
#include "renderer/win32/win32_renderer_util.h"
#include "session/commands.pb.h"

namespace mozc {
namespace renderer {
namespace win32 {
using WTL::CPoint;
using WTL::CRect;
namespace {
const POINT kInvalidMousePosition = {-65535, -65535};

CRect GetPreeditRect(const commands::RendererCommand &command) {
  const CRect preedit_rectangle(command.preedit_rectangle().left(),
                                command.preedit_rectangle().top(),
                                command.preedit_rectangle().right(),
                                command.preedit_rectangle().bottom());

  if (!command.has_application_info() ||
      !command.application_info().has_target_window_handle()) {
    return preedit_rectangle;
  }

  const HWND target_window_handle = reinterpret_cast<HWND>(
        command.application_info().target_window_handle());
  CRect result;
  Singleton<LayoutManager>().get()->GetRectInPhysicalCoords(
      target_window_handle, preedit_rectangle, &result);
  return result;
}
}  // anonymous namespace

WindowManager::WindowManager()
    : candidates_finger_print_(0),
      thread_id_(0),
      send_command_interface_(NULL),
      last_position_(kInvalidMousePosition),
      main_window_(new CandidateWindow),
      cascading_window_(new CandidateWindow),
      composition_window_list_(CompositionWindowList::CreateInstance()) {
}

WindowManager::~WindowManager() {}

void WindowManager::Initialize() {
  DCHECK(!main_window_->IsWindow());
  DCHECK(!cascading_window_->IsWindow());

  main_window_->Create(NULL);
  main_window_->ShowWindow(SW_HIDE);
  cascading_window_->Create(NULL);
  cascading_window_->ShowWindow(SW_HIDE);
  composition_window_list_->Initialize();
}

void WindowManager::AsyncHideAllWindows() {
  cascading_window_->ShowWindowAsync(SW_HIDE);
  main_window_->ShowWindowAsync(SW_HIDE);
  composition_window_list_->AsyncHide();
}

void WindowManager::AsyncQuitAllWindows() {
  cascading_window_->PostMessage(WM_CLOSE, 0, 0);
  main_window_->PostMessage(WM_CLOSE, 0, 0);
  composition_window_list_->AsyncQuit();
}

void WindowManager::DestroyAllWindows() {
  if (main_window_->IsWindow()) {
    main_window_->DestroyWindow();
  }
  if (cascading_window_->IsWindow()) {
    cascading_window_->DestroyWindow();
  }
  composition_window_list_->Destroy();
}

void WindowManager::HideAllWindows() {
  main_window_->ShowWindow(SW_HIDE);
  cascading_window_->ShowWindow(SW_HIDE);
  composition_window_list_->Hide();
}

// TODO(yukawa): need to rearrange this candidate window when the desktop
//               theme or the display setting has changed.
void WindowManager::UpdateLayout(const commands::RendererCommand &command) {
  const commands::Candidates &candidates = command.output().candidates();

  // Force updating the finger print of our candidate strings
  // when this command comes from another thread.
  if (command.has_application_info() &&
      command.application_info().has_thread_id()) {
      if (command.application_info().thread_id() != thread_id_) {
        candidates_finger_print_ = 0;
      }
      thread_id_ = command.application_info().thread_id();
  }

  // calculate the candidate finger print.
  string buf;
  command.output().candidates().SerializeToString(&buf);
  const uint32 fp = Util::Fingerprint32(buf);

  bool candidate_changed = false;
  if (fp != candidates_finger_print_) {
    candidate_changed = true;
    candidates_finger_print_ = fp;
  }

  if (candidate_changed && candidates.display_type() == commands::MAIN) {
    main_window_->UpdateLayout(candidates);
  }

  DWORD set_windows_pos_flags = 0;
  if (!candidate_changed) {
    // Update the size of this candidate window only when the finger print of
    // the given candidate strings isn't equal to the saved one.
    set_windows_pos_flags |= SWP_NOSIZE;
  }

  const Size main_window_size = main_window_->GetLayoutSize();
  const Rect preedit_rect(GetPreeditRect(command));
  const Size preedit_size(preedit_rect.Width(), preedit_rect.Height());

  // Obtain the monitor's working area
  Rect working_area;
  // Candidate window is placed on the monitor where the bottom left corner of
  // preedit is displayed.
  const Point preedit_bottom_left(preedit_rect.Left(), preedit_rect.Bottom());
  HMONITOR monitor = ::MonitorFromPoint(preedit_bottom_left.ToCPoint(),
                                        MONITOR_DEFAULTTONEAREST);

  MONITORINFO monitor_info;
  ZeroMemory(&monitor_info, sizeof(monitor_info));
  monitor_info.cbSize = CCSIZEOF_STRUCT(MONITORINFO, dwFlags);
  if (::GetMonitorInfo(monitor, &monitor_info) == 0) {
    LOG(ERROR) << "GetMonitorInfo failed. last error = " << GetLastError();
    working_area.size.height = 0;
    working_area.size.width = 0;
  } else {
    working_area = Rect(monitor_info.rcWork);
  }

  // We prefer the left position of candidate strings is aligned to
  // that of preedit.
  const Point main_window_zero_point(
      main_window_->GetCandidateColumnInClientCord().Left(), 0);

  const Rect main_window_rect =
      WindowUtil::GetWindowRectForMainWindowFromPreeditRect(
          preedit_rect, main_window_size, main_window_zero_point, working_area);

  main_window_->SetWindowPos(HWND_TOPMOST,
                             main_window_rect.Left(),
                             main_window_rect.Top(),
                             main_window_rect.Width(),
                             main_window_rect.Height(),
                             set_windows_pos_flags);

  if (candidates.has_subcandidates() &&
      candidates.subcandidates().display_type() == commands::CASCADE) {
    const commands::Candidates &subcandidates = candidates.subcandidates();

    if (candidate_changed) {
      cascading_window_->UpdateLayout(subcandidates);
    }

    // Put the cascading window right to the selected row of this candidate
    // window.
    const Rect selected_row = main_window_->GetSelectionRectInScreenCord();
    const Rect selected_row_with_window_border(
        Point(main_window_rect.Left(), selected_row.Top()),
        Size(main_window_rect.Right() - main_window_rect.Left(),
             selected_row.Top() - selected_row.Bottom()));

    // We prefer the top of client area of the cascading window is
    // aligned to the top of selected candidate in the candidate window.
    const Point cascading_window_zero_point(
        0, cascading_window_->GetFirstRowInClientCord().Top());

    const Size cascading_window_size = cascading_window_->GetLayoutSize();

    // cascading window should be in the same working area as the main window.
    const Rect cascading_window_rect =
        WindowUtil::GetWindowRectForCascadingWindow(
            selected_row_with_window_border, cascading_window_size,
            cascading_window_zero_point, working_area);

    cascading_window_->SetWindowPos(HWND_TOPMOST,
                                    cascading_window_rect.Left(),
                                    cascading_window_rect.Top(),
                                    cascading_window_rect.Width(),
                                    cascading_window_rect.Height(),
                                    set_windows_pos_flags);
    if (candidate_changed) {
      main_window_->Invalidate();
      cascading_window_->Invalidate(FALSE);
    }
    main_window_->ShowWindow(SW_SHOWNA);
    cascading_window_->ShowWindow(SW_SHOWNA);
  } else {
    // no cascading window
    if (candidate_changed) {
      main_window_->Invalidate();
    }
    cascading_window_->ShowWindow(SW_HIDE);
    main_window_->ShowWindow(SW_SHOWNA);
  }
}

// TODO(yukawa): Refactor this method by making a new method in LayoutManager
//   with unit tests so that LayoutManager can handle both composition windows
//   and candidate windows.
void WindowManager::UpdateLayoutIMM32(
    const commands::RendererCommand &command) {
  typedef mozc::commands::RendererCommand::CandidateForm CandidateForm;
  typedef mozc::commands::RendererCommand::ApplicationInfo ApplicationInfo;

  // Hide all UI elements if |command.visible()| is false.
  if (!command.visible()) {
    composition_window_list_->Hide();
    cascading_window_->ShowWindow(SW_HIDE);
    main_window_->ShowWindow(SW_HIDE);
    return;
  }

  // We assume |output| exists in the renderer command
  // for all IMM32 renderer messages.
  DCHECK(command.has_output());
  const commands::Output &output = command.output();

  // We assume |application_info| exists in the renderer command
  // for all IMM32 renderer messages.
  DCHECK(command.has_application_info());

  const commands::RendererCommand::ApplicationInfo &app_info =
      command.application_info();

  const HWND target_window_handle =
      reinterpret_cast<HWND>(app_info.target_window_handle());
  bool show_candidate =
      ((app_info.ui_visibilities() & ApplicationInfo::ShowCandidateWindow) ==
       ApplicationInfo::ShowCandidateWindow);
  bool show_suggest =
      ((app_info.ui_visibilities() & ApplicationInfo::ShowSuggestWindow) ==
        ApplicationInfo::ShowSuggestWindow);
  const bool show_composition =
      ((app_info.ui_visibilities() & ApplicationInfo::ShowCompositionWindow) ==
        ApplicationInfo::ShowCompositionWindow);

  CandidateWindowLayout candidate_layout;
  vector<CompositionWindowLayout> layouts;
  if (show_composition) {
    if (!Singleton<LayoutManager>().get()->LayoutCompositionWindow(
             command, &layouts, &candidate_layout)) {
      candidate_layout.Clear();
      layouts.clear();
      show_candidate = false;
      show_suggest = false;
    }
  }

  // CompositionWindowList::UpdateLayout will hides all windows if
  // |layouts| is empty.
  composition_window_list_->UpdateLayout(layouts);

  if (!output.has_candidates()) {
    // Hide candidate windows because there is no candidate to be displayed.
    cascading_window_->ShowWindow(SW_HIDE);
    main_window_->ShowWindow(SW_HIDE);
    return;
  }

  bool is_suggest = false;
  bool is_convert_or_predict = false;
  if (output.candidates().has_category()) {
    switch (output.candidates().category()) {
      case commands::SUGGESTION:
        is_suggest = true;
        break;
      case commands::CONVERSION:
      case commands::PREDICTION:
        is_convert_or_predict = true;
        break;
      default:
        // do nothing.
        break;
    }
  }

  if (is_suggest && !show_suggest) {
    // The candidate list is for suggestion but the visibility bit is off.
    cascading_window_->ShowWindow(SW_HIDE);
    main_window_->ShowWindow(SW_HIDE);
    return;
  }

  if (is_convert_or_predict && !show_candidate) {
    // The candidate list is for conversion or prediction but the visibility
    // bit is off.
    cascading_window_->ShowWindow(SW_HIDE);
    main_window_->ShowWindow(SW_HIDE);
    return;
  }

  const commands::Candidates &candidates = output.candidates();
  if (candidates.candidate_size() == 0) {
    cascading_window_->ShowWindow(SW_HIDE);
    main_window_->ShowWindow(SW_HIDE);
    return;
  }

  if (!candidate_layout.initialized()) {
    candidate_layout.Clear();
    if (is_suggest) {
        Singleton<LayoutManager>().get()->
            LayoutCandidateWindowForSuggestion(
                app_info, &candidate_layout);
    } else if (is_convert_or_predict) {
        Singleton<LayoutManager>().get()->
            LayoutCandidateWindowForConversion(
                app_info, &candidate_layout);
    }
  }

  if (!candidate_layout.initialized()) {
    cascading_window_->ShowWindow(SW_HIDE);
    main_window_->ShowWindow(SW_HIDE);
    return;
  }

  // Currently, we do not use finger print.
  bool candidate_changed = true;

  if (candidate_changed && (candidates.display_type() == commands::MAIN)) {
    main_window_->UpdateLayout(candidates);
  }
  const Size main_window_size = main_window_->GetLayoutSize();

  const Point target_point(candidate_layout.position());

  // Obtain the monitor's working area
  const HMONITOR monitor = ::MonitorFromPoint(
      target_point.ToCPoint(), MONITOR_DEFAULTTONEAREST);

  Rect working_area;
  MONITORINFO monitor_info = {0};
  monitor_info.cbSize = CCSIZEOF_STRUCT(MONITORINFO, dwFlags);
  if (::GetMonitorInfo(monitor, &monitor_info) == 0) {
    LOG(ERROR) << "GetMonitorInfo failed. last error = " << GetLastError();
    working_area.size.height = 0;
    working_area.size.width = 0;
  } else {
    working_area = Rect(monitor_info.rcWork);
  }

  // We prefer the left position of candidate strings is aligned to
  // that of preedit.
  const Point main_window_zero_point(
      main_window_->GetCandidateColumnInClientCord().Left(), 0);

  Rect main_window_rect;
  if (candidate_layout.has_exclude_region()) {
    Point target_point(candidate_layout.position());
    // Equating |exclusion_area| with |preedit_rect| generally works well and
    // makes most of users happy.
    const Rect preedit_rect(candidate_layout.exclude_region());
    const bool vertical = (LayoutManager::GetWritingDirection(app_info) ==
                           LayoutManager::VERTICAL_WRITING);
    // Sometimes |target_point| is set to the top-left of the exclusion area
    // but WindowUtil does not support this case yet.
    // As a workaround, use |preedit_rect.Bottom()| for y-coordinate of the
    // |target_point|.
    // TODO(yukawa): Fix WindowUtil to support this case.
    // TODO(yukawa): Add more unit tests.
    if (!vertical) {
      target_point.y = preedit_rect.Bottom();
    }
    main_window_rect =
        WindowUtil::GetWindowRectForMainWindowFromTargetPointAndPreedit(
            target_point, preedit_rect, main_window_size,
            main_window_zero_point, working_area, vertical);
  } else {
    main_window_rect =
        WindowUtil::GetWindowRectForMainWindowFromTargetPoint(
            target_point, main_window_size, main_window_zero_point,
            working_area);
  }

  DWORD set_windows_pos_flags = 0;
  main_window_->SetWindowPos(HWND_TOPMOST,
                             main_window_rect.Left(),
                             main_window_rect.Top(),
                             main_window_rect.Width(),
                             main_window_rect.Height(),
                             set_windows_pos_flags);

  if (candidates.has_subcandidates() &&
      candidates.subcandidates().display_type() == commands::CASCADE) {
    const commands::Candidates &subcandidates = candidates.subcandidates();

    if (candidate_changed) {
      cascading_window_->UpdateLayout(subcandidates);
    }

    // Put the cascading window right to the selected row of this candidate
    // window.
    const Rect selected_row = main_window_->GetSelectionRectInScreenCord();
    const Rect selected_row_with_window_border(
        Point(main_window_rect.Left(), selected_row.Top()),
        Size(main_window_rect.Right() - main_window_rect.Left(),
             selected_row.Top() - selected_row.Bottom()));

    // We prefer the top of client area of the cascading window is
    // aligned to the top of selected candidate in the candidate window.
    const Point cascading_window_zero_point(
        0, cascading_window_->GetFirstRowInClientCord().Top());

    const Size cascading_window_size = cascading_window_->GetLayoutSize();

    // cascading window should be in the same working area as the main window.
    const Rect cascading_window_rect =
        WindowUtil::GetWindowRectForCascadingWindow(
            selected_row_with_window_border, cascading_window_size,
            cascading_window_zero_point, working_area);

    cascading_window_->SetWindowPos(HWND_TOPMOST,
                                    cascading_window_rect.Left(),
                                    cascading_window_rect.Top(),
                                    cascading_window_rect.Width(),
                                    cascading_window_rect.Height(),
                                    set_windows_pos_flags);
    if (candidate_changed) {
      main_window_->Invalidate();
      cascading_window_->Invalidate(FALSE);
    }
    main_window_->ShowWindow(SW_SHOWNA);
    cascading_window_->ShowWindow(SW_SHOWNA);
  } else {
    // no cascading window
    if (candidate_changed) {
      main_window_->Invalidate();
    }
    cascading_window_->ShowWindow(SW_HIDE);
    main_window_->ShowWindow(SW_SHOWNA);
  }
}


bool WindowManager::Activate() {
  // TODO(yukawa): to be implemented.
  return true;
}

bool WindowManager::IsAvailable() const {
  // TODO(yukawa): to be implemented.
  return true;
}

void WindowManager::SetSendCommandInterface(
    client::SendCommandInterface *send_command_interface) {
  main_window_->SetSendCommandInterface(send_command_interface);
  cascading_window_->SetSendCommandInterface(send_command_interface);
}

void WindowManager::PreTranslateMessage(const MSG &message) {
  if (message.message != WM_MOUSEMOVE) {
    return;
  }

  // Window manager sometimes generates WM_MOUSEMOVE message when the contents
  // under the mouse cursor has been changed (e.g. the window is moved) so that
  // the mouse handler can update its cursor image based on the contents to
  // which the cursor is newly pointing.
  // See http://blogs.msdn.com/b/oldnewthing/archive/2003/10/01/55108.aspx for
  // details about such kind of phantom WM_MOUSEMOVE.  See also b/3104996.
  // Here we compares the screen coordinate of the mouse cursor with the last
  // one to determine if this WM_MOUSEMOVE is an artificial one or not.
  // If the coordinate is the same, this is an artificial WM_MOUSEMOVE.
  bool is_moving = true;
  const CPoint cursor_pos_in_client_coords(GET_X_LPARAM(message.lParam),
                                           GET_Y_LPARAM(message.lParam));
  CPoint cursor_pos_in_logical_coords;
  if (Singleton<LayoutManager>().get()->ClientPointToScreen(
          message.hwnd, cursor_pos_in_client_coords,
          &cursor_pos_in_logical_coords)) {
    // Since the renderer process is DPI-aware, we can safely use this
    // (logical) coordinates as if it is real (physical) screen coordinates.
    if (cursor_pos_in_logical_coords == last_position_) {
      is_moving = false;
    }
    last_position_ = cursor_pos_in_logical_coords;
  }

  // Notify candidate windows if the cursor is moving or not so that they can
  // filter unnecessary WM_MOUSEMOVE events.
  main_window_->set_mouse_moving(is_moving);
  cascading_window_->set_mouse_moving(is_moving);
}
}  // namespace win32
}  // namespace renderer
}  // namespace mozc

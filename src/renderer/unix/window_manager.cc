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

#include "renderer/unix/window_manager.h"

#include "base/logging.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/unix/gtk_wrapper.h"
#include "renderer/window_util.h"

namespace mozc {
namespace renderer {
namespace gtk {

WindowManager::WindowManager(GtkWindowInterface *candidate_window,
                             GtkWindowInterface *infolist_window,
                             GtkWrapperInterface *gtk)
    : candidate_window_(candidate_window),
      infolist_window_(infolist_window),
      gtk_(gtk) {}

WindowManager::~WindowManager() {}

void WindowManager::Initialize() {
  // Should call ShowWindow function in all window, otherwise each Initialize
  // function will fail.
  ShowAllWindows();
  HideAllWindows();
  candidate_window_->Initialize();
  infolist_window_->Initialize();
}

void WindowManager::HideAllWindows() {
  candidate_window_->HideWindow();
  infolist_window_->HideWindow();
}

void WindowManager::ShowAllWindows() {
  candidate_window_->ShowWindow();
  infolist_window_->ShowWindow();
}

// static
bool WindowManager::ShouldShowCandidateWindow(
    const commands::RendererCommand &command) {
  if (!command.visible()) {
    return false;
  }

  DCHECK(command.has_output());
  const commands::Output &output = command.output();

  if (!output.has_candidates()) {
    return false;
  }

  const commands::Candidates &candidates = output.candidates();
  if (candidates.candidate_size() == 0) {
    return false;
  }

  return true;
}

Rect WindowManager::UpdateCandidateWindow(
    const commands::RendererCommand &command) {
  // TODO(nona): skip update if there are no changes.
  DCHECK(command.has_output());
  DCHECK(command.output().has_candidates());
  const commands::Candidates &candidates = command.output().candidates();
  DCHECK_LT(0, candidates.candidate_size());

  const Size new_window_size = candidate_window_->Update(candidates);

  Point new_window_pos = candidate_window_->GetWindowPos();
  if (command.has_preedit_rectangle()) {
    new_window_pos.x = command.preedit_rectangle().left();
    new_window_pos.y = command.preedit_rectangle().bottom();
  }

  const Rect working_area = GetMonitorRect(new_window_pos.x, new_window_pos.y);
  const Point alignment_base_point_in_local_window_coord(
      candidate_window_->GetCandidateColumnInClientCord().Left(), 0);
  const auto &preedit_rect = command.preedit_rectangle();
  const Rect caret_rect(preedit_rect.left(), preedit_rect.top(),
                        preedit_rect.right() - preedit_rect.left(),
                        preedit_rect.bottom() - preedit_rect.top());
  // |caret_rect| is not always equal to preedit rect but can be an alternative
  // in terms of positional calculation, especially for vertical adjustment in
  // horizontal writing.
  const Rect expected_window_rect_in_screen_coord =
      WindowUtil::GetWindowRectForMainWindowFromTargetPointAndPreedit(
          new_window_pos, caret_rect, new_window_size,
          alignment_base_point_in_local_window_coord, working_area,
          false);  // GTK+ renderer only support horizontal window.
  candidate_window_->Move(expected_window_rect_in_screen_coord.origin);
  candidate_window_->ShowWindow();

  return expected_window_rect_in_screen_coord;
}

// static
bool WindowManager::ShouldShowInfolistWindow(
    const commands::RendererCommand &command) {
  if (!command.output().has_candidates()) {
    return false;
  }

  const commands::Candidates &candidates = command.output().candidates();
  if (candidates.candidate_size() <= 0) {
    return false;
  }

  if (!candidates.has_usages() || !candidates.has_focused_index()) {
    return false;
  }

  if (candidates.usages().information_size() <= 0) {
    return false;
  }

  // Converts candidate's index to column row index.
  const int focused_row =
      candidates.focused_index() - candidates.candidate(0).index();
  if (candidates.candidate_size() < focused_row) {
    return false;
  }

  if (!candidates.candidate(focused_row).has_information_id()) {
    return false;
  }

  return true;
}

Rect WindowManager::GetMonitorRect(gint x, gint y) {
  GtkWidget *window = gtk_->GtkWindowNew(GTK_WINDOW_TOPLEVEL);
  GdkScreen *screen = gtk_->GtkWindowGetScreen(window);
  const gint monitor = gtk_->GdkScreenGetMonitorAtPoint(screen, x, y);
  GdkRectangle screen_rect = {};
  gtk_->GdkScreenGetMonitorGeometry(screen, monitor, &screen_rect);
  return Rect(screen_rect.x, screen_rect.y, screen_rect.width,
              screen_rect.height);
}

void WindowManager::UpdateInfolistWindow(
    const commands::RendererCommand &command,
    const Rect &candidate_window_rect) {
  if (!WindowManager::ShouldShowInfolistWindow(command)) {
    infolist_window_->HideWindow();
    return;
  }

  const commands::Candidates &candidates = command.output().candidates();
  const Size infolist_window_size = infolist_window_->Update(candidates);

  const Rect screen_rect =
      GetMonitorRect(candidate_window_rect.Left(), candidate_window_rect.Top());
  const Rect infolist_rect =
      WindowUtil::WindowUtil::GetWindowRectForInfolistWindow(
          infolist_window_size, candidate_window_rect, screen_rect);
  infolist_window_->Move(infolist_rect.origin);
  infolist_window_->ShowWindow();
}

void WindowManager::UpdateLayout(const commands::RendererCommand &command) {
  if (!ShouldShowCandidateWindow(command)) {
    HideAllWindows();
    return;
  }

  if (command.has_application_info() &&
      command.application_info().has_pango_font_description()) {
    const std::string font_description =
        command.application_info().pango_font_description();
    if (previous_font_description_ != font_description) {
      DVLOG(1) << "Font description is changed"
               << " From:" << previous_font_description_
               << " To  :" << font_description;
      candidate_window_->ReloadFontConfig(font_description);
      infolist_window_->ReloadFontConfig(font_description);
      previous_font_description_.assign(font_description);
    }
  }

  const Rect candidate_window_rect = UpdateCandidateWindow(command);
  UpdateInfolistWindow(command, candidate_window_rect);
}

bool WindowManager::Activate() {
  // TODO(nona): Implement
  return true;
}

bool WindowManager::IsAvailable() const {
  // TODO(nona): Implement
  return true;
}

bool WindowManager::SetSendCommandInterface(
    client::SendCommandInterface *send_command_interface) {
  send_command_interface_ = send_command_interface;
  return candidate_window_->SetSendCommandInterface(send_command_interface_) &&
         infolist_window_->SetSendCommandInterface(send_command_interface_);
}

void WindowManager::SetWindowPos(int x, int y) {
  candidate_window_->Move(Point(x, y));
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

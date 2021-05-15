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

#include "renderer/qt/qt_window_manager.h"

#include "base/logging.h"
#include "protocol/renderer_command.pb.h"

namespace mozc {
namespace renderer {

int QtWindowManager::StartRendererLoop(int argc, char **argv) {
  QApplication app(argc, argv);

  QWidget window;
  window.setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                        Qt::WindowStaysOnTopHint);
  window.setSizePolicy(
      QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));

  window_ = &window;

  QTableWidget table(&window);
  table.horizontalHeader()->hide();
  table.horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  table.verticalHeader()->hide();
  table.setShowGrid(false);
  table.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  table.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  table.setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
  table.move(0, 0);
  table.show();
  candidates_ = &table;

  return app.exec();
}

void QtWindowManager::Initialize() {
  DLOG(INFO) << "Initialize";
}

void QtWindowManager::HideAllWindows() {
  window_->hide();
}

void QtWindowManager::ShowAllWindows() {
  window_->show();
}

// static
bool QtWindowManager::ShouldShowCandidateWindow(
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

namespace {
// Copied from unix/candidate_window.cc
void GetDisplayString(
    const commands::Candidates::Candidate &candidate, std::string &shortcut,
    std::string &value, std::string &description) {
  shortcut.clear();
  value.clear();
  description.clear();

  if (!candidate.has_value()) {
    return;
  }
  value.assign(candidate.value());

  if (!candidate.has_annotation()) {
    return;
  }

  const commands::Annotation &annotation = candidate.annotation();

  if (annotation.has_shortcut()) {
    shortcut.assign(annotation.shortcut());
  }

  if (annotation.has_description()) {
    description.assign(annotation.description());
  }

  if (annotation.has_prefix()) {
    value.assign(annotation.prefix());
    value.append(candidate.value());
  }

  if (annotation.has_suffix()) {
    value.append(annotation.suffix());
  }
}

Rect GetRect(QRect qrect) {
  return Rect(qrect.x(), qrect.y(), qrect.width(), qrect.height());
}
}  // namespace



Rect QtWindowManager::UpdateCandidateWindow(
    const commands::RendererCommand &command) {
  const commands::Candidates &candidates = command.output().candidates();
  std::string shortcut, value, description;
  std::string label;

  const size_t cands_size = candidates.candidate_size();
  window_->hide();
  candidates_->hide();
  candidates_->clear();
  candidates_->setRowCount(cands_size);
  candidates_->setColumnCount(3);
  candidates_->setColumnWidth(0, 20);

  // Fill the candidates
  for (size_t i = 0; i < cands_size; ++i) {
    const commands::Candidates::Candidate &candidate = candidates.candidate(i);
    GetDisplayString(candidate, shortcut, value, description);
    candidates_->setItem(
        i, 0, new QTableWidgetItem(QString::fromUtf8(shortcut.c_str())));
    candidates_->setItem(
        i, 1, new QTableWidgetItem(QString::fromUtf8(value.c_str())));
    candidates_->setItem(
        i, 2, new QTableWidgetItem(QString::fromUtf8(description.c_str())));
  }

  // Set the focused highlight
  if (candidates.has_focused_index()) {
    // Copied from unix/const.h
    const QColor highlight = QColor(0xD1, 0xEA, 0xFF, 0xFF);
    const int focused_row =
        candidates.focused_index() - candidates.candidate(0).index();
    candidates_->item(focused_row, 0)->setBackground(highlight);
    candidates_->item(focused_row, 1)->setBackground(highlight);
    candidates_->item(focused_row, 2)->setBackground(highlight);
  }
  candidates_->resizeColumnsToContents();
  candidates_->show();

  const auto &preedit_rect = command.preedit_rectangle();
  window_->move(preedit_rect.left(), preedit_rect.bottom());
  window_->resize(candidates_->sizeHint());
  window_->show();

  const QRect qrect = window_->geometry();
  const Rect expected_window_rect_in_screen_coord = GetRect(qrect);
  return expected_window_rect_in_screen_coord;
}

bool QtWindowManager::ShouldShowInfolistWindow(
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

Rect QtWindowManager::GetMonitorRect(int x, int y) {
  return GetRect(QGuiApplication::primaryScreen()->geometry());
}

void QtWindowManager::UpdateInfolistWindow(
    const commands::RendererCommand &command,
    const Rect &candidate_window_rect) {
  DLOG(INFO) << "UpdateInfolistWindow";
}

void QtWindowManager::UpdateLayout(const commands::RendererCommand &command) {
  if (!ShouldShowCandidateWindow(command)) {
    HideAllWindows();
    return;
  }

  const Rect candidate_window_rect = UpdateCandidateWindow(command);
  UpdateInfolistWindow(command, candidate_window_rect);
}

bool QtWindowManager::Activate() {
  DLOG(INFO) << "Activate";
  return true;
}

bool QtWindowManager::IsAvailable() const {
  DLOG(INFO) << "IsAvailable";
  return true;
}

bool QtWindowManager::SetSendCommandInterface(
    client::SendCommandInterface *send_command_interface) {
  DLOG(INFO) << "SetSendCommandInterface";
  return true;
}

void QtWindowManager::SetWindowPos(int x, int y) {
  window_->move(x, y);
}

}  // namespace renderer
}  // namespace mozc

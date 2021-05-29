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
#include "renderer/window_util.h"
#include "absl/strings/str_cat.h"

namespace mozc {
namespace renderer {

namespace {
constexpr int kRowHeight = 20;
constexpr int kMarginWidth = 20;
constexpr int kColumn0Width = 20;
constexpr int kColumn3Width = 4;
constexpr int kInfolistWidth = 520;

// #RRGGBB or #AARRGGBB
constexpr char kBackgroundColor[] = "#FFFFFF";
constexpr char kHighlightColor[] = "#D1EAFF";
constexpr char kIndicatorColor[] = "#7FACDD";
constexpr char kFooterBackgroundColor[] = "#EEEEEE";
constexpr char kDescriptionColor[] = "#888888";
constexpr char kShortcutColor[] = "#616161";
constexpr char kShortcutBackgroundColor[] = "#F3F4FF";

}  // namespace

int QtWindowManager::StartRendererLoop(int argc, char **argv) {
  QApplication app(argc, argv);

  QWidget window;
  window.setWindowFlags(Qt::ToolTip |
                        Qt::FramelessWindowHint |
                        Qt::WindowStaysOnTopHint);
  window.setSizePolicy(
      QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));

  window_ = &window;

  QTableWidget table(&window);
  table.horizontalHeader()->hide();
  table.horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  table.verticalHeader()->hide();
  table.verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  table.setShowGrid(false);
  table.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  table.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  table.move(0, 0);
  table.show();
  candidates_ = &table;

  infolist_ = new QTableWidget();
  infolist_->setWindowFlags(Qt::ToolTip |
                            Qt::FramelessWindowHint |
                            Qt::WindowStaysOnTopHint);
  infolist_->horizontalHeader()->hide();
  infolist_->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  infolist_->verticalHeader()->hide();
  infolist_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  infolist_->setShowGrid(false);
  infolist_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  infolist_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  infolist_->setColumnCount(1);
  infolist_->setRowCount(2);
  infolist_->setColumnWidth(0, kInfolistWidth);
  infolist_->show();

  return app.exec();
}

void QtWindowManager::Initialize() {
  DLOG(INFO) << "Initialize";
}

void QtWindowManager::HideAllWindows() {
  window_->hide();
  infolist_->hide();
}

void QtWindowManager::ShowAllWindows() {
  window_->show();
  infolist_->show();
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

Rect GetRect(const QRect &qrect) {
  return Rect(qrect.x(), qrect.y(), qrect.width(), qrect.height());
}

Rect GetRect(const commands::RendererCommand::Rectangle &prect) {
  const int width = prect.right() - prect.left();
  const int height = prect.bottom() - prect.top();
  return Rect(prect.left(), prect.top(), width, height);
}

QString QStr(const std::string &str) {
  return QString::fromUtf8(str.c_str());
}

bool IsUpdated(const commands::RendererCommand &prev_command,
               const commands::RendererCommand &new_command) {
  const commands::Candidates &prev_cands = prev_command.output().candidates();
  const commands::Candidates &new_cands = new_command.output().candidates();
  if (prev_cands.candidate_size() != new_cands.candidate_size()) {
    return true;
  }
  if (prev_cands.candidate(0).id() != new_cands.candidate(0).id()) {
    return true;
  }
  if (prev_cands.candidate(0).value() != new_cands.candidate(0).value()) {
    return true;
  }
  return false;
}

int GetWidth(const QTableWidgetItem *item) {
  QFontMetrics metrics(item->font());
  return metrics.width(item->text());
}

std::string GetIndexGuideString(const commands::Candidates &candidates) {
  if (!candidates.has_footer() || !candidates.footer().index_visible()) {
    return "";
  }

  const int focused_index = candidates.focused_index();
  const int total_items = candidates.size();

  return absl::StrCat(focused_index + 1, "/", total_items);
}

int GetFocusedRow(const commands::Candidates &candidates) {
  if (candidates.has_focused_index()) {
    return candidates.focused_index() - candidates.candidate(0).index();
  }
  return -1;  // No focused row
}

void FillCandidateHighlight(const commands::Candidates &candidates,
                            const int row,
                            QTableWidget *table) {
  if (row < 0) {
    return;
  }

  const bool has_info = candidates.candidate(row).has_information_id();
  const QColor indicator = QColor(kIndicatorColor);

  if (row == GetFocusedRow(candidates)) {
    const QColor highlight = QColor(kHighlightColor);
    table->item(row, 0)->setBackgroundColor(highlight);
    table->item(row, 1)->setBackgroundColor(highlight);
    table->item(row, 2)->setBackgroundColor(highlight);
    table->item(row, 3)->setBackgroundColor(has_info ? indicator : highlight);
    return;
  }

  const QColor background = QColor(kBackgroundColor);
  if (candidates.candidate(row).annotation().shortcut().empty()) {
    table->item(row, 0)->setBackgroundColor(background);
  } else {
    table->item(row, 0)->setBackgroundColor(QColor(kShortcutBackgroundColor));
  }
  table->item(row, 1)->setBackgroundColor(background);
  table->item(row, 2)->setBackgroundColor(background);
  table->item(row, 3)->setBackgroundColor(has_info ? indicator : background);
}

void FillCandidates(const commands::Candidates &candidates,
                    QTableWidget *table) {
  const size_t cands_size = candidates.candidate_size();
  table->clear();
  table->setRowCount(cands_size + 1);  // +1 is for footer.
  table->setColumnCount(4);
  table->setColumnWidth(0, kColumn0Width);  // shortcut
  table->setColumnWidth(3, kColumn3Width);  // infolist indicator

  int max_width1 = 0;
  int max_width2 = 0;

  const QColor shortcut_color = QColor(kShortcutColor);
  const QColor description_color = QColor(kDescriptionColor);
  const QColor footer_bg_color = QColor(kFooterBackgroundColor);

  // Fill the candidates
  std::string shortcut, value, description;
  for (size_t i = 0; i < cands_size; ++i) {
    const commands::Candidates::Candidate &candidate = candidates.candidate(i);
    GetDisplayString(candidate, shortcut, value, description);

    // shortcut
    auto item0 = new QTableWidgetItem(QStr(shortcut));
    item0->setForeground(shortcut_color);
    item0->setTextAlignment(Qt::AlignCenter);
    table->setItem(i, 0, item0);

    // value
    auto item1 = new QTableWidgetItem(QStr(value));
    table->setItem(i, 1, item1);

    // description
    auto item2 = new QTableWidgetItem(QStr(description));
    item2->setForeground(description_color);
    table->setItem(i, 2, item2);

    // indicator
    auto item3 = new QTableWidgetItem();
    table->setItem(i, 3, item3);
    FillCandidateHighlight(candidates, i, table);

    max_width1 = std::max(max_width1, GetWidth(item1));
    max_width2 = std::max(max_width2, GetWidth(item2));
    table->setRowHeight(i, kRowHeight);
  }

  // Footer
  table->setRowHeight(cands_size, kRowHeight);
  for (int i = 0; i < table->columnCount(); ++i) {
    auto footer_item = new QTableWidgetItem();
    footer_item->setBackgroundColor(footer_bg_color);
    table->setItem(cands_size, i, footer_item);
  }
  QTableWidgetItem *footer2 = table->item(cands_size, 2);
  footer2->setText(QStr(GetIndexGuideString(candidates)));
  footer2->setTextAlignment(Qt::AlignRight);
  max_width2 = std::max(max_width2, GetWidth(footer2));

  // Resize
  table->setColumnWidth(1, max_width1 + kMarginWidth);
  table->setColumnWidth(2, max_width2 + kMarginWidth);
  const int magic_number_for_margin = 8;  // Heuristic number for margin
  const int width = kColumn0Width + max_width1 + max_width2 + kColumn3Width +
                    kMarginWidth * 2 + magic_number_for_margin;
  const int height = kRowHeight * (cands_size + 1);  // +1 is for footer.
  table->resize(width, height);
}
}  // namespace

Point QtWindowManager::GetWindowPosition(
    const commands::RendererCommand &command,
    const Size &win_size) {
  const Rect preedit_rect = GetRect(command.preedit_rectangle());
  const Point win_pos = Point(preedit_rect.Left(), preedit_rect.Bottom());
  const Rect monitor_rect = GetMonitorRect(win_pos.x, win_pos.y);
  const Point offset_to_column1(kColumn0Width, 0);

  const Rect adjusted_win_geometry =
      WindowUtil::GetWindowRectForMainWindowFromTargetPointAndPreedit(
          win_pos, preedit_rect, win_size, offset_to_column1, monitor_rect,
          /* vertical */ false);  // Only support horizontal window yet.
  return adjusted_win_geometry.origin;
}

Rect QtWindowManager::UpdateCandidateWindow(
    const commands::RendererCommand &command) {
  const commands::Candidates &candidates = command.output().candidates();

  window_->hide();

  if (IsUpdated(prev_command_, command)) {
    candidates_->hide();
    FillCandidates(candidates, candidates_);
    candidates_->show();

    window_->resize(candidates_->size());

    const Size win_size(candidates_->width(), candidates_->height());
    const Point win_pos = GetWindowPosition(command, win_size);
    window_->move(win_pos.x, win_pos.y);
  } else {
    // Reset the previous focused highlight
    const int prev_focused = GetFocusedRow(prev_command_.output().candidates());
    FillCandidateHighlight(candidates, prev_focused, candidates_);
  }

  // Set the focused highlight
  FillCandidateHighlight(candidates, GetFocusedRow(candidates), candidates_);

  // Footer index
  candidates_->item(candidates_->rowCount() - 1, 2)->setText(
      QStr(GetIndexGuideString(candidates)));

  window_->show();

  prev_command_ = command;

  return GetRect(window_->geometry());
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
  if (!ShouldShowInfolistWindow(command)) {
    infolist_->hide();
    return;
  }

  infolist_->hide();
  infolist_->clear();

  const commands::InformationList &info =
      command.output().candidates().usages();
  const size_t size = info.information_size();

  infolist_->setColumnCount(1);
  infolist_->setColumnWidth(0, kInfolistWidth);
  infolist_->setRowCount(size * 2);
  int total_height = 0;

  for (int i = 0; i < size; ++i) {
    const std::string title = info.information(i).title();
    const std::string desc = info.information(i).description();
    const auto qtitle = new QTableWidgetItem(QString::fromUtf8(title.c_str()));
    const auto qdesc = new QTableWidgetItem(QString::fromUtf8(desc.c_str()));

    const int desc_height = kRowHeight * (GetWidth(qdesc) / kInfolistWidth + 2);
    infolist_->setRowHeight(i * 2, kRowHeight);
    infolist_->setRowHeight(i * 2 + 1, desc_height);
    total_height += (kRowHeight + desc_height);

    infolist_->setItem(0, i * 2, qtitle);
    infolist_->setItem(0, i * 2 + 1, qdesc);

    if (info.focused_index() == i) {
      const QColor highlight = QColor(kHighlightColor);
      qtitle->setBackgroundColor(highlight);
      qdesc->setBackgroundColor(highlight);
    }
  }
  infolist_->move(candidate_window_rect.Right(), candidate_window_rect.Top());
  infolist_->resize(kInfolistWidth, total_height);
  infolist_->show();
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

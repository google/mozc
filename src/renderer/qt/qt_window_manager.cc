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

#include <algorithm>
#include <string>

#include "absl/strings/str_cat.h"
#include "base/logging.h"
#include "protocol/candidates.pb.h"
#include "renderer/renderer_style_handler.h"
#include "renderer/window_util.h"

namespace mozc {
namespace renderer {

namespace {
constexpr int kMarginHeight = 5;
constexpr int kMarginWidth = 20;
constexpr int kColumn0Width = 20;
constexpr int kColumn3Width = 6;
constexpr int kInfolistWidth = 520;

// #RRGGBB or #AARRGGBB
constexpr char kBackgroundColor[] = "#FFFFFF";
constexpr char kHighlightColor[] = "#D1EAFF";
constexpr char kIndicatorColor[] = "#7FACDD";
constexpr char kFooterBackgroundColor[] = "#EEEEEE";
constexpr char kDescriptionColor[] = "#888888";
constexpr char kShortcutColor[] = "#616161";
constexpr char kShortcutBackgroundColor[] = "#F3F4FF";

QString QStr(const std::string &str) { return QString::fromUtf8(str.c_str()); }

QColor QColorFromRGBAColor(RendererStyle::RGBAColor rgba) {
  return QColor(rgba.r(), rgba.g(), rgba.b(), 255 * rgba.a());
}

}  // namespace

QtWindowManager::QtWindowManager() {
  RendererStyleHandler::GetRendererStyle(&style_);
}

void QtWindowManager::OnClicked(int row, int column) {
  DLOG(INFO) << "OnClicked: (" << row << ", " << column << ")";
  if (send_command_interface_ == nullptr) {
    return;
  }
  if (row < 0 || row >= prev_command_.output().candidates().candidate_size()) {
    return;
  }
  const int cand_id = prev_command_.output().candidates().candidate(row).id();
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::SELECT_CANDIDATE);
  command.set_id(cand_id);
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void QtWindowManager::Initialize() {
  const auto initialize_table = [](QTableWidget *table) {
    table->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint |
                          Qt::WindowStaysOnTopHint);
    table->setSelectionMode(QTableWidget::NoSelection);
    table->setShowGrid(false);

    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->horizontalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->horizontalHeader()->setMinimumSectionSize(1);

    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->verticalHeader()->hide();
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setMinimumSectionSize(1);
  };

  candidates_ = new QTableWidget();
  initialize_table(candidates_);
  QObject::connect(candidates_, &QTableWidget::cellClicked,
                   [&](int row, int col) { OnClicked(row, col); });

  infolist_ = new QTableWidget();
  initialize_table(infolist_);
  infolist_->setColumnCount(1);
  infolist_->setRowCount(3);
  infolist_->setColumnWidth(0, kInfolistWidth);
}

void QtWindowManager::HideAllWindows() {
  candidates_->hide();
  infolist_->hide();
}

void QtWindowManager::ShowAllWindows() {
  candidates_->show();
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
void GetDisplayString(const commands::Candidates::Candidate &candidate,
                      std::string &shortcut, std::string &value,
                      std::string &description) {
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

int GetItemWidth(const QTableWidgetItem &item) {
  QFontMetrics metrics(item.font());
  return metrics.boundingRect(item.text()).width() + kMarginWidth;
}

int GetItemHeight(const QTableWidgetItem &item) {
  QFontMetrics metrics(item.font());
  return metrics.height() + kMarginHeight;
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
                            const int row, QTableWidget *table) {
  if (row < 0) {
    return;
  }

  const bool has_info = candidates.candidate(row).has_information_id();
  const QBrush indicator = QBrush(QColor(kIndicatorColor));

  if (row == GetFocusedRow(candidates)) {
    const QBrush highlight = QBrush(QColor(kHighlightColor));
    table->item(row, 0)->setBackground(highlight);
    table->item(row, 1)->setBackground(highlight);
    table->item(row, 2)->setBackground(highlight);
    table->item(row, 3)->setBackground(has_info ? indicator : highlight);
    return;
  }

  const QBrush background = QBrush(QColor(kBackgroundColor));
  if (candidates.candidate(row).annotation().shortcut().empty()) {
    table->item(row, 0)->setBackground(background);
  } else {
    const QBrush shortcut_background = QBrush(QColor(kShortcutBackgroundColor));
    table->item(row, 0)->setBackground(shortcut_background);
  }
  table->item(row, 1)->setBackground(background);
  table->item(row, 2)->setBackground(background);
  table->item(row, 3)->setBackground(has_info ? indicator : background);
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
  int total_height = 0;

  const QBrush shortcut_brush = QBrush(QColor(kShortcutColor));
  const QBrush description_brush = QBrush(QColor(kDescriptionColor));
  const QBrush footer_bg_brush = QBrush(QColor(kFooterBackgroundColor));

  // Fill the candidates
  std::string shortcut, value, description;
  for (size_t i = 0; i < cands_size; ++i) {
    const commands::Candidates::Candidate &candidate = candidates.candidate(i);
    GetDisplayString(candidate, shortcut, value, description);

    // shortcut
    auto item0 = new QTableWidgetItem(QStr(shortcut));
    item0->setForeground(shortcut_brush);
    item0->setTextAlignment(Qt::AlignCenter);
    table->setItem(i, 0, item0);

    // value
    auto item1 = new QTableWidgetItem(QStr(value));
    table->setItem(i, 1, item1);

    // description
    auto item2 = new QTableWidgetItem(QStr(description));
    item2->setForeground(description_brush);
    table->setItem(i, 2, item2);

    // indicator
    auto item3 = new QTableWidgetItem();
    table->setItem(i, 3, item3);
    FillCandidateHighlight(candidates, i, table);

    max_width1 = std::max(max_width1, GetItemWidth(*item1));
    max_width2 = std::max(max_width2, GetItemWidth(*item2));
    const int height = GetItemHeight(*item1);
    table->setRowHeight(i, height);
    total_height += height;
  }

  // Footer
  for (int i = 0; i < table->columnCount(); ++i) {
    auto footer_item = new QTableWidgetItem();
    footer_item->setBackground(footer_bg_brush);
    table->setItem(cands_size, i, footer_item);
  }
  QTableWidgetItem *footer2 = table->item(cands_size, 2);
  footer2->setText(QStr(GetIndexGuideString(candidates)));
  footer2->setTextAlignment(Qt::AlignRight);
  max_width2 = std::max(max_width2, GetItemWidth(*footer2));
  const int footer_height = GetItemHeight(*footer2);
  table->setRowHeight(cands_size, footer_height);
  total_height += footer_height;

  // Resize
  table->setColumnWidth(1, max_width1);
  table->setColumnWidth(2, max_width2);
  const int width = kColumn0Width + max_width1 + max_width2 + kColumn3Width;
  table->resize(width, total_height);
}
}  // namespace

Point QtWindowManager::GetWindowPosition(
    const commands::RendererCommand &command, const Size &win_size) {
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

  if (IsUpdated(prev_command_, command)) {
    FillCandidates(candidates, candidates_);
    const Size win_size(candidates_->width(), candidates_->height());
    const Point win_pos = GetWindowPosition(command, win_size);
    candidates_->move(win_pos.x, win_pos.y);
  } else {
    // Reset the previous focused highlight
    const int prev_focused = GetFocusedRow(prev_command_.output().candidates());
    FillCandidateHighlight(candidates, prev_focused, candidates_);
  }

  // Set the focused highlight
  FillCandidateHighlight(candidates, GetFocusedRow(candidates), candidates_);

  // Footer index
  candidates_->item(candidates_->rowCount() - 1, 2)
      ->setText(QStr(GetIndexGuideString(candidates)));

  candidates_->show();
  prev_command_ = command;
  return GetRect(candidates_->geometry());
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
  QPoint point{x, y};
  const QScreen *screen = QGuiApplication::screenAt(point);
  if (screen == nullptr) {
    // (x, y) does not belong to any screen. Fall back to the primary screen.
    // TODO: Return the nearest monitor rect instead.
    // c.f. GetWorkingAreaFromPointImpl in win32_renderer_util.cc.
    return GetRect(QGuiApplication::primaryScreen()->geometry());
  }

  return GetRect(screen->geometry());
}

void QtWindowManager::UpdateInfolistWindow(
    const commands::RendererCommand &command,
    const Rect &candidate_window_rect) {
  if (!ShouldShowInfolistWindow(command)) {
    infolist_->hide();
    return;
  }

  infolist_->clear();

  const commands::InformationList &info =
      command.output().candidates().usages();
  const size_t size = info.information_size();

  infolist_->setColumnCount(1);
  infolist_->setColumnWidth(0, kInfolistWidth);
  infolist_->setRowCount(size * 2 + 1);  // +1 is for the caption title.
  int total_height = 12;                 // Heuristics margin.

  // Caption title
  const std::string &caption = style_.infolist_style().caption_string();
  QTableWidgetItem *infolist_title = new QTableWidgetItem(QStr(caption));
  infolist_title->setBackground(QBrush(
      QColorFromRGBAColor(style_.infolist_style().caption_background_color())));
  infolist_->setItem(0, 0, infolist_title);
  total_height += GetItemHeight(*infolist_title);

  for (int i = 0; i < size; ++i) {
    const int title_row = i * 2 + 1;
    const int desc_row = i * 2 + 2;
    const std::string title = info.information(i).title();
    const std::string desc = info.information(i).description();
    const auto qtitle = new QTableWidgetItem(QString::fromUtf8(title.c_str()));
    const auto qdesc = new QTableWidgetItem(QString::fromUtf8(desc.c_str()));

    const int title_height = GetItemHeight(*qtitle);
    const int desc_height =
        GetItemHeight(*qdesc) * (GetItemWidth(*qdesc) / kInfolistWidth + 2);
    infolist_->setRowHeight(title_row, title_height);
    infolist_->setRowHeight(desc_row, desc_height);
    total_height += (title_height + desc_height);

    infolist_->setItem(0, title_row, qtitle);
    infolist_->setItem(0, desc_row, qdesc);

    if (info.focused_index() == i) {
      const QBrush highlight = QBrush(QColor(kHighlightColor));
      qtitle->setBackground(highlight);
      qdesc->setBackground(highlight);
    }
  }
  const Size infolist_size(kInfolistWidth, total_height);
  const Rect monitor_rect = GetMonitorRect(candidate_window_rect.Right(),
                                           candidate_window_rect.Top());
  const Rect infolist_rect =
      WindowUtil::WindowUtil::GetWindowRectForInfolistWindow(
          infolist_size, candidate_window_rect, monitor_rect);

  infolist_->move(infolist_rect.Left(), infolist_rect.Top());
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

bool QtWindowManager::ExecCommand(const commands::RendererCommand &command) {
  switch (command.type()) {
    case commands::RendererCommand::NOOP:
      break;
    case commands::RendererCommand::SHUTDOWN:
      // TODO(nona): Implement shutdown command.
      DLOG(ERROR) << "Shutdown command is not implemented.";
      return false;
      break;
    case commands::RendererCommand::UPDATE:
      if (!command.visible()) {
        HideAllWindows();
      } else {
        UpdateLayout(command);
      }
      return true;
      break;
    default:
      LOG(WARNING) << "Unknown command: " << command.type();
      break;
  }
  return true;
}

bool QtWindowManager::SetSendCommandInterface(
    client::SendCommandInterface *send_command_interface) {
  send_command_interface_ = send_command_interface;
  return true;
}

void QtWindowManager::SetWindowPos(int x, int y) { candidates_->move(x, y); }

}  // namespace renderer
}  // namespace mozc

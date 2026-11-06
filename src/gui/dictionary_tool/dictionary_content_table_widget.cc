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

#include "gui/dictionary_tool/dictionary_content_table_widget.h"

#include <QHeaderView>
#include <QScrollBar>
#include <QtGui>
#include <algorithm>

#include "base/port.h"

namespace mozc::gui {

DictionaryContentTableWidget::DictionaryContentTableWidget(QWidget *parent)
    : QTableWidget(parent) {}

void DictionaryContentTableWidget::paintEvent(QPaintEvent *event) {
  QTableView::paintEvent(event);

  if constexpr (port::IsAppleBase()) {
    if (!isEnabled()) {
      return;
    }

    const int row_height =
        rowCount() > 0 ? rowHeight(0) : verticalHeader()->defaultSectionSize();
    if (row_height <= 0) [[unlikely]] {
      // If row_height is not available, do not paint the alternate color.
      return;
    }

    int row_y = 0;
    if (rowCount() > 0) {
      // rowViewportPosition() returns the Y-coordinate of the row directly on
      // the viewport, automatically accounting for the vertical scrollbar
      // offset.
      const int last_row_bottom =
          rowViewportPosition(rowCount() - 1) + rowHeight(rowCount() - 1);
      row_y = std::max(0, last_row_bottom);
    }

    bool is_odd = rowCount() % 2 == 1;
    QPainter painter(viewport());
    const QColor alternate_color =
        viewport()->palette().color(QPalette::Active, QPalette::AlternateBase);
    while (row_y < height()) {
      if (is_odd) {
        QRect draw_rect(0, row_y, width(), row_height);
        painter.fillRect(draw_rect, alternate_color);
      }
      row_y += row_height;
      is_odd = !is_odd;
    }
  }
}

void DictionaryContentTableWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  QTableView::mouseDoubleClickEvent(event);

  // When empty area is double-clicked, emit a signal
  if constexpr (port::IsAppleBase()) {
    if (itemAt(event->pos()) == nullptr) {
      emit emptyAreaClicked();
    }
  }
}

void DictionaryContentTableWidget::focusInEvent(QFocusEvent *event) {
  setStyleSheet(QString());
}

}  // namespace mozc::gui

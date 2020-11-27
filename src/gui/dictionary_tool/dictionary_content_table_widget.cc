// Copyright 2010-2020, Google Inc.
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

#include <QtGui/QtGui>

DictionaryContentTableWidget::DictionaryContentTableWidget(QWidget *parent)
    : QTableWidget(parent) {}

void DictionaryContentTableWidget::paintEvent(QPaintEvent *event) {
  QTableView::paintEvent(event);

#ifdef __APPLE__
  if (!isEnabled()) {
    return;
  }

  // TODO(taku): we don't want to use the fixed size, because
  // the row height would change according to the users' environment
  const int kDefaultHeight = 19;

  QRect rect;
  int alternate_index = 0;
  if (rowCount() == 0) {
    rect.setRect(0, 0, 1, kDefaultHeight);
    alternate_index = 1;
  } else {
    QTableWidgetItem *last_item = item(rowCount() - 1, 0);
    if (last_item == nullptr) {
      return;
    }
    rect = visualItemRect(last_item);
    alternate_index = rowCount();
  }

  int start_offset = rect.y() + rect.height();
  QPainter painter(viewport());
  while (start_offset < height()) {
    if (alternate_index % 2 == 1) {
      QRect draw_rect(0, start_offset, width(), rect.height());
      painter.fillRect(draw_rect, QPalette().color(QPalette::AlternateBase));
    }
    start_offset += rect.height();
    ++alternate_index;
  }
#endif  // __APPLE__
}

void DictionaryContentTableWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  QTableView::mouseDoubleClickEvent(event);

  // When empty area is double-clicked, emit a signal
#ifdef __APPLE__
  if (nullptr == itemAt(event->pos())) {
    emit emptyAreaClicked();
  }
#endif  // __APPLE__
}

void DictionaryContentTableWidget::focusInEvent(QFocusEvent *event) {
  setStyleSheet(QLatin1String(""));
}

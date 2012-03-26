// Copyright 2010-2012, Google Inc.
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

#include "gui/character_pad/character_palette_table_widget.h"

#include <QtGui/QtGui>
#include <QtCore/QTextCodec>
#include "gui/character_pad/selection_handler.h"
#include "gui/character_pad/unicode_util.h"

namespace mozc {
namespace gui {

CharacterPaletteTableWidget::CharacterPaletteTableWidget(QWidget *parent)
    : QTableWidget(parent),
      lookup_result_item_(NULL) {
  setMouseTracking(true);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setSelectionBehavior(QAbstractItemView::SelectItems);
}

void CharacterPaletteTableWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    return;
  }
  QTableWidgetItem *item = itemAt(event->pos());
  if (item == NULL) {
    return;
  }
  SelectionHandler::Select(item->text());
  emit itemSelected(item);
}

void CharacterPaletteTableWidget::mouseMoveEvent(QMouseEvent *event) {
  QTableWidgetItem *item = itemAt(event->pos());
  if (item == NULL) {
    return;
  }

  if (lookup_result_item_ == NULL || lookup_result_item_ == item) {
    setCurrentItem(item);
    QTableWidget::mousePressEvent(event);
    QToolTip::showText(event->globalPos(),
                       UnicodeUtil::GetToolTip(font(), item->text()),
                       this);
    setLookupResultItem(NULL);
  }
}
}  // namespace gui
}  // namespace mozc

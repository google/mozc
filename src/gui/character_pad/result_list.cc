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

#include "gui/character_pad/result_list.h"

#include <QtCore/QStringList>
#include <QtGui/QtGui>
#include "gui/character_pad/selection_handler.h"
#include "gui/character_pad/unicode_util.h"

namespace mozc {
namespace gui {

ResultList::ResultList(QWidget *parent)
    : QListWidget(parent) {
  setFlow(ResultList::LeftToRight);
  setWrapping(true);
  setUniformItemSizes(true);
  setMouseTracking(true);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setMouseTracking(true);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setSelectionBehavior(QAbstractItemView::SelectItems);
}

// redaraw items
void ResultList::update() {
  QStringList tmp;
  for (int i = 0; i < count(); ++i) {
    tmp << item(i)->text();
  }
  clear();
  for (int i = 0; i < tmp.size(); ++i) {
    addItem(tmp.at(i));
  }
  QListWidget::update();
}

void ResultList::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    return;
  }
  const QListWidgetItem *item = itemAt(event->pos());
  if (item == NULL) {
    return;
  }
  SelectionHandler::Select(item->text());
}

void ResultList::mouseMoveEvent(QMouseEvent *event) {
  QListWidgetItem *item = itemAt(event->pos());
  if (item == NULL) {
    return;
  }
  setCurrentItem(item);
  QListWidget::mousePressEvent(event);
  QToolTip::showText(event->globalPos(),
                     UnicodeUtil::GetToolTip(font(), item->text()),
                     this);
}

void ResultList::updateFont(const QFont &font) {
  QFont new_font = font;
  new_font.setPointSize(this->font().pointSize());
  setFont(new_font);
  adjustSize();
  update();
}

void ResultList::updateFontSize(int index) {
  int font_point = 20;
  switch (index) {
    case 0: font_point = 32; break;
    case 1: font_point = 28; break;
    case 2: font_point = 20; break;
    case 3: font_point = 18; break;
    case 4: font_point = 16; break;
  }

  QFont nfont = font();
  nfont.setPointSize(font_point);
  setFont(nfont);
  adjustSize();
  update();
}
}  // namespace gui
}  // namespace mozc

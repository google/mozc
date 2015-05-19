// Copyright 2010-2014, Google Inc.
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

#ifndef MOZC_GUI_CHARACTER_PAD_CHARACTER_PALETTE_TABLE_WIDGET_H_
#define MOZC_GUI_CHARACTER_PAD_CHARACTER_PALETTE_TABLE_WIDGET_H_

#include <QtGui/QTableWidget>

class QTextCodec;
class QTableWidgetItem;

namespace mozc {
namespace gui {

class CharacterPaletteTableWidget : public QTableWidget {
  Q_OBJECT;

 public:
  explicit CharacterPaletteTableWidget(QWidget *parent);

  void setLookupResultItem(QTableWidgetItem *item) {
    lookup_result_item_ = item;
  }

 signals:
  void itemSelected(const QTableWidgetItem *item);

 protected:
  void mouseReleaseEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);

 private:
  QTableWidgetItem *lookup_result_item_;
};
}  // namspace gui
}  // namespace mozc

#if defined UI_RADICAL_SEARCH_H || defined  UI_STROKE_COUNT_H \
 || defined UI_HAND_WRITING_H || defined UI_CHARACTER_PALETTE_H
using mozc::gui::CharacterPaletteTableWidget;
#endif

#endif  // MOZC_GUI_CHARACTER_PAD_CHARACTER_PALETTE_TABLE_WIDGET_H_

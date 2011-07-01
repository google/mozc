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

#ifndef MOZC_GUI_CHARACTER_PAD_HAND_WRITING_CANVAS_H_
#define MOZC_GUI_CHARACTER_PAD_HAND_WRITING_CANVAS_H_

#include "base/base.h"
#include "base/mmap.h"


#include <QtGui/QWidget>
#include <QtCore/QVector>
#include <QtCore/QPair>

class QMouseEvent;
class QListWidget;

namespace zinnia {
class Recognizer;
class Character;
};

namespace mozc {
namespace gui {

class HandWritingCanvas : public QWidget {
  Q_OBJECT;

 public:
  explicit HandWritingCanvas(QWidget *parent);
  virtual ~HandWritingCanvas();

  void setListWidget(QListWidget *list_widget) {
    list_widget_ = list_widget;
  }

  size_t strokes_size() const;

 public slots:
  void clear();
  void revert();

 protected:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);

 private:
  void recognize();
  QVector<QVector<QPair<float, float> > > strokes_;
  scoped_ptr<zinnia::Recognizer> recognizer_;
  scoped_ptr<zinnia::Character> character_;
  scoped_ptr<Mmap<char> > mmap_;
  QListWidget *list_widget_;
  bool is_drawing_;

 signals:
  void canvasUpdated();
};
}   // namespace gui
}   // namespace mozc

using mozc::gui::HandWritingCanvas;

#endif  // MOZC_GUI_CHARACTER_PAD_HAND_WRITING_CANVAS_H_

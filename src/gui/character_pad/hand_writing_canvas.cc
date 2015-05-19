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

#include "gui/character_pad/hand_writing_canvas.h"

#include <QtGui/QtGui>
#include "base/base.h"
#include "gui/character_pad/hand_writing.h"

namespace mozc {
namespace gui {

HandWritingCanvas::HandWritingCanvas(QWidget *parent)
    : QWidget(parent),
      list_widget_(NULL), is_drawing_(false),
      handwriting_status_(handwriting::HANDWRITING_NO_ERROR) {
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
  strokes_.reserve(128);
  QObject::connect(this, SIGNAL(startRecognition()),
                   &recognizer_thread_, SLOT(startRecognition()),
                   Qt::QueuedConnection);
  QObject::connect(&recognizer_thread_, SIGNAL(candidatesUpdated()),
                   this, SLOT(listUpdated()), Qt::QueuedConnection);
  qRegisterMetaType<mozc::handwriting::HandwritingStatus>(
      "mozc::handwriting::HandwritingStatus");
  QObject::connect(&recognizer_thread_,
                   SIGNAL(statusUpdated(mozc::handwriting::HandwritingStatus)),
                   this,
                   SLOT(statusUpdated(mozc::handwriting::HandwritingStatus)),
                   Qt::QueuedConnection);
  recognizer_thread_.Start();
}

HandWritingCanvas::~HandWritingCanvas() {
  recognizer_thread_.quit();
  recognizer_thread_.wait();
}

void HandWritingCanvas::setListWidget(QListWidget *list_widget)  {
  list_widget_ = list_widget;
  QObject::connect(list_widget_, SIGNAL(itemSelected(const QListWidgetItem*)),
                   &recognizer_thread_,
                   SLOT(itemSelected(const QListWidgetItem*)),
                   Qt::QueuedConnection);
}

void HandWritingCanvas::clear() {
  handwriting_status_ = handwriting::HANDWRITING_NO_ERROR;
  strokes_.clear();
  update();
  is_drawing_ = false;
}

void HandWritingCanvas::revert() {
  handwriting_status_ = handwriting::HANDWRITING_NO_ERROR;
  if (!strokes_.empty()) {
    strokes_.resize(strokes_.size() - 1);
    update();
    recognize();
  }
  is_drawing_ = false;
}

void HandWritingCanvas::restartRecognition() {
  // We need to call |recognize()| instead of |emit startRecognition()| here
  // so that the current stroke has a new timestamp.
  recognize();
}

void HandWritingCanvas::paintEvent(QPaintEvent *) {
  QPainter painter(this);

  // show grid information
  painter.setPen(QPen(Qt::gray, 1));

  const QRect border_rect(0, 0, width() - 1, height() - 1);
  painter.drawRect(border_rect);

  const int diff = static_cast<int>(height() * 0.05);
  const int margin = static_cast<int>(height() * 0.04);
  painter.drawLine(width() / 2 - diff, height() / 2,
                   width() / 2 + diff, height() / 2);
  painter.drawLine(width() / 2, height() / 2  - diff,
                   width() / 2, height() / 2 + diff);

  painter.drawLine(margin, margin, margin + diff, margin);
  painter.drawLine(margin, margin, margin, margin + diff);

  painter.drawLine(width() - margin - diff, margin,
                   width() - margin, margin);
  painter.drawLine(width() - margin, margin,
                   width() - margin, margin + diff);

  painter.drawLine(margin, height() - margin - diff,
                   margin, height() - margin);
  painter.drawLine(margin, height() - margin,
                   margin + diff, height() - margin);

  painter.drawLine(width() - margin - diff, height() - margin,
                   width() - margin, height() - margin);
  painter.drawLine(width() - margin, height() - margin - diff,
                   width() - margin, height() - margin);

  if (strokes_.empty()) {
    painter.drawText(margin + 10, margin + 10,
                     width() - margin - 20,  height() / 2,
                     Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                     QObject::tr("Draw a character here"));
  }

  // show pen strokes
  painter.setPen(QPen(Qt::black, 3));

  for (int i = 0; i < strokes_.size(); ++i) {
    for (int j = 1; j < strokes_[i].size(); ++j) {
      const int x1 = static_cast<int>(strokes_[i][j - 1].first * width());
      const int y1 = static_cast<int>(strokes_[i][j - 1].second * height());
      const int x2 = static_cast<int>(strokes_[i][j].first * width());
      const int y2 = static_cast<int>(strokes_[i][j].second * height());
      painter.drawLine(x1, y1, x2, y2);
    }
  }

  if (handwriting_status_ != handwriting::HANDWRITING_NO_ERROR) {
    painter.setPen(QPen(Qt::red, 2));
    QString warning_message;
    switch (handwriting_status_) {
      case handwriting::HANDWRITING_ERROR:
        warning_message = QObject::tr("error");
        break;
      case handwriting::HANDWRITING_NETWORK_ERROR:
        warning_message = QObject::tr("network error");
        break;
      case handwriting::HANDWRITING_UNKNOWN_ERROR:
      default:
        warning_message = QObject::tr("unknown error");
        break;
    }
    painter.drawText(0, 0, width() - margin, height() - margin,
                     Qt::AlignRight | Qt::AlignBottom | Qt::TextWordWrap,
                     warning_message);
  }

  emit canvasUpdated();
}

void HandWritingCanvas::recognize() {
  if (strokes_.empty()) {
    return;
  }

  recognizer_thread_.SetStrokes(strokes_);
  emit startRecognition();
}

void HandWritingCanvas::listUpdated() {
  vector<string> candidates;
  recognizer_thread_.GetCandidates(&candidates);

  list_widget_->clear();
  for (size_t i = 0; i < candidates.size(); ++i) {
    list_widget_->addItem(QString::fromUtf8(candidates[i].c_str()));
  }
}

void HandWritingCanvas::statusUpdated(handwriting::HandwritingStatus status) {
  handwriting_status_ = status;
  update();
}

void HandWritingCanvas::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    return;
  }

  strokes_.resize(strokes_.size() + 1);
  const float x = static_cast<float>(event->pos().x()) / width();
  const float y = static_cast<float>(event->pos().y()) / height();
  strokes_.back().push_back(make_pair(x, y));
  is_drawing_ = true;
  update();
}

void HandWritingCanvas::mouseMoveEvent(QMouseEvent *event) {
  if (!is_drawing_) {
    return;
  }

  const float x = static_cast<float>(event->pos().x()) / width();
  const float y = static_cast<float>(event->pos().y()) / height();
  strokes_.back().push_back(make_pair(x, y));
  update();
}

void HandWritingCanvas::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    return;
  }

  is_drawing_ = false;
  update();
  recognize();
}

size_t HandWritingCanvas::strokes_size() const {
  return strokes_.size();
}
}  // namespace gui
}  // namespace mozc

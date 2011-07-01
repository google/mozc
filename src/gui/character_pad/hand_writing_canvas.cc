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

#include "gui/character_pad/hand_writing_canvas.h"

#include <QtGui/QtGui>
#include "base/base.h"
#include "gui/character_pad/hand_writing.h"

#ifdef OS_MACOSX
#include "base/mac_util.h"
#endif

#ifdef USE_LIBZINNIA
// Use default zinnia installed in /usr/include
#include <zinnia.h>
#else
#include "third_party/zinnia/v0_04/zinnia.h"
#endif

namespace mozc {
namespace gui {

namespace {
string GetModelFileName() {
#ifdef OS_MACOSX
  // TODO(komatsu): Fix the file name to "handwriting-ja.model" like the
  // Windows implementation regardless which data file is actually
  // used.  See also gui.gyp:hand_writing_mac.
  const char kModelFile[] = "handwriting-light-ja.model";
  return Util::JoinPath(MacUtil::GetResourcesDirectory(), kModelFile);
#elif defined(USE_LIBZINNIA)
  // On Linux, use the model for tegaki-zinnia.
  const char kModelFile[] =
      "/usr/share/tegaki/models/zinnia/handwriting-ja.model";
  return kModelFile;
#else
  const char kModelFile[] = "handwriting-ja.model";
  return Util::JoinPath(Util::GetServerDirectory(), kModelFile);
#endif  // OS_MACOSX
}
}  // namespace

HandWritingCanvas::HandWritingCanvas(QWidget *parent)
    : QWidget(parent),
      recognizer_(zinnia::Recognizer::create()),
      character_(zinnia::Character::create()),
      mmap_(new Mmap<char>()),
      list_widget_(NULL), is_drawing_(false) {
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
  strokes_.reserve(128);

  const string model_file = GetModelFileName();
  if (!mmap_->Open(model_file.c_str())) {
    LOG(ERROR) << "Cannot open model file:" << model_file;
    QMessageBox::critical(this,
                          tr("Mozc"),
                          tr("Failed to load model file %1").arg(
                              QString(model_file.c_str())));
    return;
  }
  if (!recognizer_->open(mmap_->begin(), mmap_->GetFileSize())) {
    LOG(ERROR) << "Model file is broken:" << model_file;
    QMessageBox::critical(this,
                          tr("Mozc"),
                          tr("model file %1 is broken.").arg(
                              QString(model_file.c_str())));
    return;
  }
}

HandWritingCanvas::~HandWritingCanvas() {}

void HandWritingCanvas::clear() {
  strokes_.clear();
  update();
  is_drawing_ = false;
}

void HandWritingCanvas::revert() {
  if (!strokes_.empty()) {
    strokes_.resize(strokes_.size() - 1);
    update();
    recognize();
  }
  is_drawing_ = false;
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

  emit canvasUpdated();
}

void HandWritingCanvas::recognize() {
  character_->clear();
  character_->set_width(width());
  character_->set_height(height());
  for (int i = 0; i < strokes_.size(); ++i) {
    for (int j = 0; j < strokes_[i].size(); ++j) {
      character_->add(i,
                      static_cast<int>(width() * strokes_[i][j].first),
                      static_cast<int>(height() * strokes_[i][j].second));
    }
  }

  const int kMaxResultSize = 100;
  scoped_ptr<zinnia::Result> result(recognizer_->classify(*character_,
                                                          kMaxResultSize));
  if (result.get() == NULL) {
    return;
  }

  list_widget_->clear();
  for (size_t i = 0; i < result->size(); ++i) {
    list_widget_->addItem(QString::fromUtf8(result->value(i)));
  }
}

void HandWritingCanvas::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    return;
  }

  strokes_.resize(strokes_.size() + 1);
  const float x = static_cast<float>(event->pos().x()) / width();
  const float y = static_cast<float>(event->pos().y()) / height();
  strokes_.back().push_back(qMakePair(x, y));
  is_drawing_ = true;
  update();
}

void HandWritingCanvas::mouseMoveEvent(QMouseEvent *event) {
  if (!is_drawing_) {
    return;
  }

  const float x = static_cast<float>(event->pos().x()) / width();
  const float y = static_cast<float>(event->pos().y()) / height();
  strokes_.back().push_back(qMakePair(x, y));
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

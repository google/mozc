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

#include "gui/character_pad/hand_writing.h"

#include <QtGui/QtGui>
#include <QtGui/QMessageBox>

#ifdef OS_WINDOWS
#include <windows.h>
#include <windowsx.h>
#endif

#include "gui/base/win_util.h"

namespace mozc {
namespace gui {

HandWriting::HandWriting(QWidget *parent)
    : QMainWindow(parent) {
  setupUi(this);

  handWritingCanvas->setListWidget(resultListWidget);

  fontComboBox->setWritingSystem
      (static_cast<QFontDatabase::WritingSystem>
       (QFontDatabase::Any));
  fontComboBox->setEditable(false);
  fontComboBox->setCurrentFont(resultListWidget->font());

  QObject::connect(fontComboBox,
                   SIGNAL(currentFontChanged(const QFont &)),
                   this, SLOT(updateFont(const QFont &)));

  QObject::connect(sizeComboBox,
                   SIGNAL(currentIndexChanged(int)),
                   this, SLOT(updateFontSize(int)));

  QObject::connect(clearButton,
                   SIGNAL(clicked()),
                   this, SLOT(clear()));

  QObject::connect(revertButton,
                   SIGNAL(clicked()),
                   this, SLOT(revert()));

  QObject::connect(handWritingCanvas,
                   SIGNAL(canvasUpdated()),
                   this, SLOT(updateUIStatus()));

  // "4" means smallest.
  sizeComboBox->setCurrentIndex(4);
  fontComboBox->setCurrentFont(resultListWidget->font());

  updateUIStatus();
  repaint();
  update();
}

void HandWriting::updateFont(const QFont &font) {
  resultListWidget->updateFont(font);
}

void HandWriting::updateFontSize(int index) {
  resultListWidget->updateFontSize(index);
}

void HandWriting::resizeEvent(QResizeEvent *event) {
  resultListWidget->update();
}

void HandWriting::clear() {
  resultListWidget->clear();
  handWritingCanvas->clear();
  updateUIStatus();
}

void HandWriting::revert() {
  resultListWidget->clear();
  handWritingCanvas->revert();
  updateUIStatus();
}

void HandWriting::updateUIStatus() {
#ifdef OS_MACOSX
  // Due to a bug of Qt?, the appearance of these buttons
  // doesn't change on Mac. To fix this issue, always set
  // true on Mac.
  clearButton->setEnabled(true);
  revertButton->setEnabled(true);
#else
  const bool enabled = handWritingCanvas->strokes_size() > 0;
  clearButton->setEnabled(enabled);
  revertButton->setEnabled(enabled);
#endif
}

#ifdef OS_WINDOWS
bool HandWriting::winEvent(MSG *message, long *result) {
  if (message != NULL &&
      message->message == WM_LBUTTONDOWN &&
      WinUtil::IsCompositionEnabled()) {
    const QWidget *widget = qApp->widgetAt(
        mapToGlobal(QPoint(message->lParam & 0xFFFF,
                           (message->lParam >> 16) & 0xFFFF)));
    if (widget == centralwidget) {
      ::PostMessage(message->hwnd, WM_NCLBUTTONDOWN,
                    static_cast<WPARAM>(HTCAPTION), message->lParam);
      return true;
    }
  }

  return QWidget::winEvent(message, result);
}
#endif  // OS_WINDOWS

}  // namespace gui
}  // namespace mozc

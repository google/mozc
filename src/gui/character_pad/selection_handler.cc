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

#include "gui/character_pad/selection_handler.h"

#include <QtGui/QApplication>
#include <QtGui/QClipboard>
#include <QtGui/QMessageBox>

#include "base/singleton.h"

namespace mozc {
namespace gui {
namespace {

class CopyToClipboardCallback : public SelectionCallbackInterface {
 public:
  void Select(const QString &str) {
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard == NULL) {
      return;
    }

    clipboard->setText(str, QClipboard::Clipboard);
    clipboard->setText(str, QClipboard::Selection);
    QMessageBox::information(NULL,
                             QObject::tr("Note"),
                             str + QObject::tr(" is sent to clipboard"));
  }
};

SelectionCallbackInterface *g_selection_callback = NULL;

SelectionCallbackInterface *GetSelectionCallback() {
  if (g_selection_callback == NULL) {
    return Singleton<CopyToClipboardCallback>::get();
  } else {
    return g_selection_callback;
  }
}
}  // namespace

void SelectionHandler::Select(const QString &str) {
  GetSelectionCallback()->Select(str);
}

void SelectionHandler::SetSelectionCallback(
    SelectionCallbackInterface *callback) {
  g_selection_callback = callback;
}
}  // namespace gui
}  // namespace mozc

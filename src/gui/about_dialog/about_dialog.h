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

// A dialog widget for "About Mozc" info.

#ifndef MOZC_GUI_ABOUT_DIALOG_ABOUT_DIALOG_H_
#define MOZC_GUI_ABOUT_DIALOG_ABOUT_DIALOG_H_

#include <QtGui/QDialog>
#include "base/base.h"
#include "gui/about_dialog/ui_about_dialog.h"

#if defined(OS_WINDOWS) && defined(GOOGLE_JAPANESE_INPUT_BUILD) \
    && defined(_DEBUG)
#define USE_UPDATE_CHECKER
#endif  // OS_WINDOWS && GOOGLE_JAPANESE_INPUT_BUILD && _DEBUG

class QImage;

namespace mozc {
namespace gui {

class LinkCallbackInterface {
 public:
  virtual ~LinkCallbackInterface() {}
  virtual void linkActivated(const QString &str) = 0;
};

class AboutDialog : public QDialog,
                    private Ui::AboutDialog {
  Q_OBJECT;
 public:
  explicit AboutDialog(QWidget *parent = NULL);
  void SetLinkCallback(LinkCallbackInterface *callback);

  void paintEvent(QPaintEvent *event);

 public slots:
  void linkActivated(const QString &link);
  void updateButtonPushed();

 protected:
#ifdef USE_UPDATE_CHECKER
  bool winEvent(MSG *message, long *result);
#endif  // USE_UPDATE_CHECKER

 private:
  LinkCallbackInterface *callback_;
  scoped_ptr<QImage> product_image_;
};
}  // namespace mozc::gui
}  // namespace mozc

#endif  // MOZC_GUI_ABOUT_DIALOG_ABOUT_DIALOG_H_

// Copyright 2010, Google Inc.
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

#include "gui/about_dialog/about_dialog.h"

#include <QtGui/QtGui>

#include <string>

#include "base/process.h"
#include "base/run_level.h"
#include "base/util.h"
#include "base/version.h"

namespace mozc {
namespace gui {

namespace {
void defaultLinkActivated(const QString &str) {
  QByteArray utf8 = str.toUtf8();
  Process::OpenBrowser(string(utf8.data(), utf8.length()));
}

// set document file paths by adding <server_path>/documents/ to file name.
bool AddLocalPath(string *str) {
  const char *filenames[] = { "credits_en.html", "credits_ja.html" };
  for (size_t i = 0; i < arraysize(filenames); ++i) {
    if (str->find(filenames[i]) != string::npos) {
      string tmp;
      const string documents_dir = Util::JoinPath(Util::GetServerDirectory(),
                                                  "documents");
      const string file_path = Util::JoinPath(documents_dir, filenames[i]);
      Util::StringReplace(*str, filenames[i], file_path, false, &tmp);
      *str = tmp;
      return true;
    }
  }
  return false;
}

void SetLabelText(QLabel *label) {
  string label_text = label->text().toStdString();
  if (AddLocalPath(&label_text)) {
    label->setText(QString::fromStdString(label_text));
  }
}
}  // anonymous namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent),
      callback_(NULL) {
  setupUi(this);
  setWindowFlags(Qt::WindowSystemMenuHint);
  setWindowModality(Qt::NonModal);
  QPalette window_palette;
  window_palette.setColor(QPalette::Window, QColor(255, 255, 255));
  setPalette(window_palette);
  setAutoFillBackground(true);
  QString version_info("(");
  version_info += Version::GetMozcVersion().c_str();
  version_info += ")";
  version_label->setText(version_info);
  QPalette palette;
  palette.setColor(QPalette::Window, QColor(236, 233, 216));
  color_frame->setPalette(palette);
  color_frame->setAutoFillBackground(true);

  // change font size for product name
  QFont font = label->font();
#ifdef OS_WINDOWS
  font.setPointSize(22);
#endif  // OS_WINDOWS

#ifdef OS_MACOSX
  font.setPointSize(26);
#endif  // OS_MACOSX

  label->setFont(font);

  SetLabelText(label_terms);
  SetLabelText(label_credits);

  product_image_.reset(new QImage(":/product_logo.png"));
}

void AboutDialog::paintEvent(QPaintEvent *event) {
  // draw product logo
  QPainter painter(this);
  const QRect image_rect = product_image_->rect();
  // allow clipping on right / bottom borders
  const QRect draw_rect(max(5, width() - image_rect.width() - 15),
                        max(0, color_frame->y() - image_rect.height()),
                        image_rect.width(), image_rect.height());
  painter.drawImage(draw_rect, *product_image_.get());
}

void AboutDialog::SetLinkCallback(LinkCallbackInterface *callback) {
  callback_ = callback;
}

void AboutDialog::linkActivated(const QString &link) {
  // we don't activate the link if about dialog is running as root
  if (!RunLevel::IsValidClientRunLevel()) {
    return;
  }
  if (callback_ != NULL) {
    callback_->linkActivated(link);
  } else {
    defaultLinkActivated(link);
  }
}
}  // namespace mozc::gui
}  // namespace mozc

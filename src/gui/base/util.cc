// Copyright 2010-2020, Google Inc.
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

#include "gui/base/util.h"

#include <memory>

#include <QtCore/QObject>
#include <QtGui/QFont>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleFactory>

#include "absl/memory/memory.h"

namespace mozc {
namespace gui {

// static
std::unique_ptr<QApplication> Util::InitQt(int &argc, char *argv[]) {
  QApplication::setStyle(QStyleFactory::create("fusion"));
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

  // QApplication takes argc as a reference.
  auto app = absl::make_unique<QApplication>(argc, argv);
#ifdef __APPLE__
  app->setFont(QFont("Hiragino Sans"));
#endif  // __APPLE__

  return app;
}

// static
const QString Util::ProductName() {
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  const QString name = QObject::tr("Mozc");
#else  // GOOGLE_JAPANESE_INPUT_BUILD
  const QString name = QObject::tr("Mozc");
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
  return name;
}

// static
void Util::ReplaceLabel(QLabel *label) {
  label->setText(ReplaceString(label->text()));
}

// static
void Util::ReplaceTitle(QWidget *widget) {
  widget->setWindowTitle(Util::ReplaceString(widget->windowTitle()));
}

// static
QString Util::ReplaceString(const QString &str) {
  QString replaced(str);
  return replaced.replace("[ProductName]", Util::ProductName());
}

}  // namespace gui
}  // namespace mozc

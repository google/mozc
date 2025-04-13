// Copyright 2010-2021, Google Inc.
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

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"

// Show the build number on the title for debugging when the build
// configuration is official dev channel.
#if defined(CHANNEL_DEV) && defined(GOOGLE_JAPANESE_INPUT_BUILD)
#define MOZC_SHOW_BUILD_NUMBER_ON_TITLE
#endif  // CHANNEL_DEV && GOOGLE_JAPANESE_INPUT_BUILD

#include <QAbstractButton>
#include <QApplication>
#include <QFont>
#include <QGuiApplication>
#include <QObject>
#include <QStyleFactory>
#include <QtGui>
#include <memory>
#include <string>
#include <utility>

#ifdef MOZC_SHOW_BUILD_NUMBER_ON_TITLE
#include "gui/base/window_title_modifier.h"
#endif  // MOZC_SHOW_BUILD_NUMBER_ON_TITLE

namespace mozc {
namespace gui {

namespace {
void InstallEventFilter() {
#ifdef MOZC_SHOW_BUILD_NUMBER_ON_TITLE
  static WindowTitleModifier window_title_modifier;
  // Install WindowTitleModifier for official dev channel
  // append a special footer (Dev x.x.x) to the all Windows.
  qApp->installEventFilter(&window_title_modifier);
#endif  // MOZC_SHOW_BUILD_NUMBER_ON_TITLE
}

void InstallDefaultTranslator() {
  // qApplication must be loaded first
  CHECK(qApp);
  static absl::NoDestructor<QTranslator> translator;

  // Load "<translation_path>/qt_<lang>.qm" from a qrc file.
  bool loaded = translator->load(
      QLocale::system(), QLatin1String("qt"), QLatin1String("_"),
      QLibraryInfo::location(QLibraryInfo::TranslationsPath),
      QLatin1String(".qm"));
  if (loaded) {
    qApp->installTranslator(translator.get());
  } else {
    // Load ":/qt_<lang>.qm" from a qrc file.
    GuiUtil::InstallTranslator("qt");
  }

  // Load ":/tr_<lang>.qm" from a qrc file for the product name.
  GuiUtil::InstallTranslator("tr");
}
}  // namespace

// static
std::unique_ptr<QApplication> GuiUtil::InitQt(int &argc, char *argv[]) {
  QApplication::setStyle(QStyleFactory::create(QLatin1String("fusion")));
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif  // QT_VERSION

  // QApplication takes argc as a reference.
  auto app = std::make_unique<QApplication>(argc, argv);
#ifdef __APPLE__
  app->setFont(QFont("Hiragino Sans"));
#endif  // __APPLE__

  InstallEventFilter();
  InstallDefaultTranslator();

  return app;
}

// static
void GuiUtil::InstallTranslator(absl::string_view resource_name) {
  static absl::NoDestructor<
      absl::flat_hash_map<std::string, std::unique_ptr<QTranslator>>>
      translators;
  if (translators->contains(resource_name)) {
    return;
  }
  auto translator = std::make_unique<QTranslator>();

  // Load ":/<resource_name>_<lang>.qm" from a qrc file.
  if (translator->load(
          QLocale::system(),
          QLatin1String(resource_name.data(), resource_name.size()),
          QLatin1String("_"), QLatin1String(":/"), QLatin1String(".qm"))) {
    qApp->installTranslator(translator.get());
    translators->emplace(resource_name, std::move(translator));
  }
}

// static
QString GuiUtil::ProductName() {
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  const QString name = QObject::tr("Google Japanese Input");
#else   // GOOGLE_JAPANESE_INPUT_BUILD
  const QString name = QObject::tr("Mozc");
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
  return name;
}

// static
void GuiUtil::ReplaceWidgetLabels(QWidget *widget) {
  ReplaceTitle(widget);
  for (auto *label : widget->findChildren<QLabel *>()) {
    ReplaceLabel(label);
  }
  for (auto *button : widget->findChildren<QAbstractButton *>()) {
    button->setText(ReplaceString(button->text()));
  }
}

// static
void GuiUtil::ReplaceLabel(QLabel *label) {
  label->setText(ReplaceString(label->text()));
}

// static
void GuiUtil::ReplaceTitle(QWidget *widget) {
  widget->setWindowTitle(GuiUtil::ReplaceString(widget->windowTitle()));
}

// static
QString GuiUtil::ReplaceString(const QString &str) {
  QString replaced(str);
  return replaced.replace(QLatin1String("[ProductName]"),
                          GuiUtil::ProductName());
}

}  // namespace gui
}  // namespace mozc

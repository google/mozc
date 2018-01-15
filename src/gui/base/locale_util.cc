// Copyright 2010-2018, Google Inc.
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

#include "gui/base/locale_util.h"

#if defined(OS_ANDROID) || defined(OS_NACL)
#error "This platform is not supported."
#endif  // OS_ANDROID || OS_NACL

// Show the build number on the title for debugging when the build
// configuration is official dev channel.
#if defined(CHANNEL_DEV) && defined(GOOGLE_JAPANESE_INPUT_BUILD)
#define MOZC_SHOW_BUILD_NUMBER_ON_TITLE
#endif  // CHANNEL_DEV && GOOGLE_JAPANESE_INPUT_BUILD

#ifdef OS_WIN
#include <windows.h>
#include <CommCtrl.h>  // for CCSIZEOF_STRUCT
#endif

#include <QtGui/QGuiApplication>

#include <QtGui/QtGui>
#include <map>
#include <string>

#include "base/logging.h"
#include "base/singleton.h"
#include "base/util.h"

#ifdef MOZC_SHOW_BUILD_NUMBER_ON_TITLE
#include "gui/base/window_title_modifier.h"
#endif  // MOZC_SHOW_BUILD_NUMBER_ON_TITLE

namespace mozc {
namespace gui {
namespace {

// sicnce Qtranslator and QFont must be available until
// application exits, allocate the data with Singleton.
class TranslationDataImpl {
 public:
  void InstallTranslationMessageAndFont(const char *resource_name);

  TranslationDataImpl();
  ~TranslationDataImpl() {
    for (std::map<string, QTranslator *>::iterator it = translators_.begin();
         it != translators_.end(); ++it) {
      delete it->second;
    }
    translators_.clear();
  }

 private:
  std::map<string, QTranslator *> translators_;
  QTranslator default_translator_;
  QFont font_;
#ifdef MOZC_SHOW_BUILD_NUMBER_ON_TITLE
  WindowTitleModifier window_title_modifier_;
#endif  // MOZC_SHOW_BUILD_NUMBER_ON_TITLE
};

TranslationDataImpl::TranslationDataImpl() {
  // qApplication must be loaded first
  CHECK(qApp);

#ifdef MOZC_SHOW_BUILD_NUMBER_ON_TITLE
  // Install WindowTilteModifier for official dev channel
  // append a special footer (Dev x.x.x) to the all Windows.
  qApp->installEventFilter(&window_title_modifier_);
#endif  // MOZC_SHOW_BUILD_NUMBER_ON_TITLE

  // Load "<translation_path>/qt_<lang>.qm" from a qrc file.
  bool loaded = default_translator_.load(
      QLocale::system(), "qt", "_",
      QLibraryInfo::location(QLibraryInfo::TranslationsPath), ".qm");
  if (!loaded) {
    // Load ":/qt_<lang>.qm" from a qrc file.
    loaded = default_translator_.load(
        QLocale::system(), "qt", "_", ":/", ".qm");
  }

  if (loaded) {
    qApp->installTranslator(&default_translator_);
  }
}

void TranslationDataImpl::InstallTranslationMessageAndFont(
    const char *resource_name) {
  if (translators_.find(resource_name) != translators_.end()) {
    return;
  }
  QTranslator *translator = new QTranslator;
  CHECK(translator);
  translators_.insert(std::make_pair(resource_name, translator));

  // Load ":/<resource_name>_<lang>.qm" from a qrc file.
  if (translator->load(QLocale::system(), resource_name, "_", ":/", ".qm")) {
    qApp->installTranslator(translator);
  }
}

}  // namespace

void LocaleUtil::InstallTranslationMessageAndFont(
    const char *resource_name) {
  Singleton<TranslationDataImpl>::get()->InstallTranslationMessageAndFont
      (resource_name);
}
}  // namespace gui
}  // namespace mozc

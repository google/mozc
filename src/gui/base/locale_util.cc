// Copyright 2010-2016, Google Inc.
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

#ifdef MOZC_USE_QT5
#include <QtGui/QGuiApplication>
#else
#include <QtCore/QTextCodec>
#include <QtGui/QApplication>
#endif

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
    for (map<string, QTranslator *>::iterator it = translators_.begin();
         it != translators_.end(); ++it) {
      delete it->second;
    }
    translators_.clear();
  }

 private:
  map<string, QTranslator *> translators_;
  QTranslator default_translator_;
  QFont font_;
#ifdef MOZC_SHOW_BUILD_NUMBER_ON_TITLE
  WindowTitleModifier window_title_modifier_;
#endif  // MOZC_SHOW_BUILD_NUMBER_ON_TITLE
};

TranslationDataImpl::TranslationDataImpl() {
  // qApplication must be loaded first
  CHECK(qApp);

#ifdef OS_WIN
  // Get the font from MessageFont
  NONCLIENTMETRICSW ncm = { 0 };

  // We don't use |sizeof(NONCLIENTMETRICSW)| because it is fragile when the
  // code is copied-and-pasted without caring about WINVER.
  // http://blogs.msdn.com/b/oldnewthing/archive/2003/12/12/56061.aspx
  const size_t kSizeOfNonClientMetricsWForVistaOrLater =
      CCSIZEOF_STRUCT(NONCLIENTMETRICSW, iPaddedBorderWidth);
  ncm.cbSize = kSizeOfNonClientMetricsWForVistaOrLater;
  if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0)) {
    // Windows font scale is 0..100 while Qt's scale is 0..99.
    // If lfWeight is 0 (default weight), we don't set the Qt's font weight.
    if (ncm.lfMessageFont.lfWeight > 0) {
      font_.setWeight(static_cast<int>(
          99.0 * ncm.lfMessageFont.lfWeight / 1000.0));
    }
    font_.setItalic(static_cast<bool>(ncm.lfMessageFont.lfItalic));
    font_.setUnderline(static_cast<bool>(ncm.lfMessageFont.lfUnderline));
    font_.setStrikeOut(static_cast<bool>(ncm.lfMessageFont.lfStrikeOut));
    string face_name;
    Util::WideToUTF8(ncm.lfMessageFont.lfFaceName, &face_name);
    font_.setFamily(QString::fromUtf8(face_name.c_str()));
    HDC hdc = ::GetDC(NULL);
    if (hdc != NULL) {
      // Get point size from height:
      // http://msdn.microsoft.com/ja-jp/library/cc428368.aspx
      const int KPointToHeightFactor= 72;
      font_.setPointSize(abs(::MulDiv(ncm.lfMessageFont.lfHeight,
                                      KPointToHeightFactor,
                                      ::GetDeviceCaps(hdc, LOGPIXELSY))));
      ::ReleaseDC(NULL, hdc);
    }
    qApp->setFont(font_);
  }
#endif

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

#ifndef MOZC_USE_QT5
  // Set default encoding for multi-byte string to be UTF8
  QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
  QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
#endif
}

void TranslationDataImpl::InstallTranslationMessageAndFont(
    const char *resource_name) {
  if (translators_.find(resource_name) != translators_.end()) {
    return;
  }
  QTranslator *translator = new QTranslator;
  CHECK(translator);
  translators_.insert(make_pair(resource_name, translator));

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

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

#include "gui/base/locale_util.h"

#ifdef OS_WINDOWS
#include <windows.h>
#include <CommCtrl.h>  // for CCSIZEOF_STRUCT
#endif

#ifdef OS_MACOSX
// To fix http://b/2495747, undef qDebug if defined.
// TODO(yukawa): Investigate more appropriate solution.
#ifdef qDebug
#define qDebugBackup qDebug
#undef qDebug
#endif  // qDebug
#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>
#ifdef qDebugBackup
#define qDebug qDebugBackup
#undef qDebugBackup
#endif  // qDebugBackup
#endif

#include <QtCore/QTextCodec>
#include <QtGui/QApplication>
#include <QtGui/QtGui>
#include <map>
#include <string>

#include "base/base.h"
#include "base/singleton.h"
#include "base/scoped_cftyperef.h"
#include "base/util.h"
#include "gui/base/window_title_modifier.h"

namespace mozc {
namespace gui {
namespace {

QString GetUILocaleName() {
  // On Windows/Mac, QLocale::system().name() returns an invalid
  // value. For instance, QLocale::system().name() returns
  // system locale instead of user locale on Windows.
#if defined(OS_WINDOWS)
  // Check UI locale instead of system locale on Windows
  const LANGID kJapaneseLangId = MAKELANGID(LANG_JAPANESE,
                                            SUBLANG_JAPANESE_JAPAN);
  if (kJapaneseLangId == ::GetUserDefaultUILanguage()) {
    return QString("ja");
  }
  return QString("en");   // by default, use English locale
#elif defined(OS_MACOSX)
  // by default, use English locale
  QString result("en");

  scoped_cftyperef<CFArrayRef> aref(
      reinterpret_cast<CFArrayRef>(
          CFPreferencesCopyAppValue(
              CFSTR("AppleLanguages"),
              kCFPreferencesCurrentApplication)));

  if (aref.get() == NULL) {
    return result;
  }

  char locale[128];
  const int locale_size = sizeof(locale);
  if (aref.Verify(CFArrayGetTypeID()) && CFArrayGetCount(aref.get()) > 0) {
    CFStringRef sref = reinterpret_cast<CFStringRef>(
        CFArrayGetValueAtIndex(aref.get(), 0));
    if (sref != NULL && CFGetTypeID(sref) == CFStringGetTypeID()) {
      scoped_cftyperef<CFStringRef> locale_name_ref(
          CFLocaleCreateCanonicalLocaleIdentifierFromString(
              kCFAllocatorDefault, sref));
      if (locale_name_ref.get() != NULL &&
          locale_name_ref.Verify(CFStringGetTypeID()) &&
          CFStringGetCString(locale_name_ref.get(),
                             locale,
                             locale_size,
                             kCFStringEncodingASCII)) {
        result = QString::fromUtf8(locale);
      }
    }
  }

  return result;
#else  // OS_MACOSX
  // return system locale on Linux
  return QLocale::system().name();
#endif  // OS_LINUX
}

// sicnce Qtranslator and QFont must be available until
// application exits, allocate the data with Singleton.
class TranslationDataImpl {
 public:
  void InstallTranslationMessageAndFont(const char *resource_name);
  void InstallTranslationMessagesAndFont(const QStringList &resource_names);

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
  QString locale_name_;
  QFont font_;
#ifdef CHANNEL_DEV
  WindowTitleModifier window_title_modifier_;
#endif
};

TranslationDataImpl::TranslationDataImpl()
    : locale_name_(GetUILocaleName()) {
  // qApplication must be loaded first
  CHECK(qApp);

#ifdef OS_WINDOWS
  // Get the font from MessageFont
  NONCLIENTMETRICSW ncm = { 0 };

  // Note that SystemParametersInfoW fails on Windows XP when |ncm.cbSize| is
  // greater than 500 (bytes), which means that this function is no longer
  // compatible with XP if WINVER >= 0x0600 because a new field named
  // |iPaddedBorderWidth| was added at the last of NONCLIENTMETRICS structure
  // in Windows Vista.  It would be better to initialize |ncm.cbSize| to be
  // compatible with Windows XP unless you rely on |iPaddedBorderWidth|.
  // For background information, you can read:
  //   http://d.hatena.ne.jp/NyaRuRu/20080303/p1
  //   http://blogs.msdn.com/b/oldnewthing/archive/2003/12/12/56061.aspx
  const size_t kSizeOfNonClientMetricsWForXpOrPrior =
      CCSIZEOF_STRUCT(NONCLIENTMETRICSW, lfMessageFont);
  ncm.cbSize = kSizeOfNonClientMetricsWForXpOrPrior;
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

#ifdef CHANNEL_DEV
  // Install WindowTilteModifier for dev channel
  // append a special footer (Dev x.x.x) to the all Windows.
  qApp->installEventFilter(&window_title_modifier_);
#endif

#ifdef OS_LINUX
  // Use system default messages.
  // Even if the locale is not English nor Japanese, load translation
  // file to translate common messages like "OK" and "Cancel".
  default_translator_.load(
      QString("qt_") + QLocale::system().name(),
      QLibraryInfo::location(QLibraryInfo::TranslationsPath));
  qApp->installTranslator(&default_translator_);
#else
  // Load Qt's default translation for Japanese, which
  // is embedded into binary with Qt resource
  if (locale_name_.startsWith("ja") &&   // "ja" or "ja_JP"
      default_translator_.load(":/qt_ja_JP")) {
    qApp->installTranslator(&default_translator_);
  }
#endif

  // Set default encoding for multi-byte string to be UTF8
  QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
  QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
}

void TranslationDataImpl::InstallTranslationMessagesAndFont(
    const QStringList &resource_names) {
  for (int i = 0; i < resource_names.count(); ++i) {
    InstallTranslationMessageAndFont(
        resource_names[i].toStdString().c_str());
  }
}

void TranslationDataImpl::InstallTranslationMessageAndFont(
    const char *resource_name) {
  map<string, QTranslator *>::const_iterator it = translators_.find(
      resource_name);
  if (it != translators_.end()) {
    return;
  }
  const QString file_name = QString(":/%1_%2").
      arg(resource_name).arg(locale_name_);
  QTranslator *translator = new QTranslator;
  CHECK(translator);
  translators_.insert(make_pair(resource_name, translator));
  if (translator->load(file_name)) {
    qApp->installTranslator(translator);
  }
}
}  // namespace

void LocaleUtil::InstallTranslationMessagesAndFont(
    const QStringList &resource_names) {
  Singleton<TranslationDataImpl>::get()->InstallTranslationMessagesAndFont
      (resource_names);
}

void LocaleUtil::InstallTranslationMessageAndFont(
    const char *resource_name) {
  Singleton<TranslationDataImpl>::get()->InstallTranslationMessageAndFont
      (resource_name);
}
}  // namespace gui
}  // namespace mozc

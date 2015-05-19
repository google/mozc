// Copyright 2010-2013, Google Inc.
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

#include "gui/character_pad/unicode_util.h"

#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <algorithm>
#include <string>
#include "base/base.h"
#include "base/logging.h"
#include "base/util.h"
#include "gui/character_pad/data/cp932_map.h"
#include "gui/character_pad/data/unicode_data.h"
#include "gui/character_pad/data/unihan_data.h"

namespace mozc {
namespace {

template <class T> struct UnicodeDataCompare {
  bool operator()(const T &d1, const T &d2) const {
    return (d1.ucs4 < d2.ucs4);
  }
};

bool ExtractFirstUCS4Char(const QString &str, char32 *ucs4) {
  const QVector<uint> ucs4s = str.toUcs4();
  // Due to QTBUG-25536, QString::toUcs4() is not reliable on Qt 4.8.0/4.8.1.
  // https://bugreports.qt-project.org/browse/QTBUG-25536
  // Nevertheless, we should be able to get the first character.
  if (ucs4s.size() < 1) {
    return false;
  }
  DCHECK(ucs4);
  *ucs4 = static_cast<char32>(ucs4s[0]);
  return true;
}

// TODO(taku):  move it to base/util
uint16 SjisToEUC(uint16 code) {
  if (code < 0x80) {  // ascii
    return code;
  }

  if (code >= 0xa1 && code <= 0xdf) {  // halfwidth kana
    return (0x8e << 8) | code;
  }

  const uint16 lo = code & 0xff;
  const uint16 hi = (code >> 8) & 0xff;
  if (lo >= 0x9f) {
    return ((hi * 2 - (hi >= 0xe0 ? 0xe0 : 0x60)) << 8) | (lo + 2);
  }

  return ((hi * 2 - (hi >= 0xe0 ? 0xe1 : 0x61)) << 8) |
      (lo + (lo >= 0x7f ? 0x60 : 0x61));
}

uint16 LookupCP932Data(const QString &str) {
  char32 ucs4 = 0;
  if (!ExtractFirstUCS4Char(str, &ucs4)) {
    return 0;
  }

  CP932MapData key;
  key.ucs4 = ucs4;
  const CP932MapData *result =
      lower_bound(kCP932MapData,
                  kCP932MapData + kCP932MapDataSize,
                  key,
                  UnicodeDataCompare<CP932MapData>());
  if (result == kCP932MapData + kCP932MapDataSize ||
      result->ucs4 != key.ucs4) {
    return 0;
  }
  return result->sjis;
}

const UnihanData *LookupUnihanData(const QString &str) {
  char32 ucs4 = 0;
  if (!ExtractFirstUCS4Char(str, &ucs4)) {
    return NULL;
  }
  UnihanData key;
  key.ucs4 = ucs4;
  const UnihanData *result =
      lower_bound(kUnihanData,
                  kUnihanData + kUnihanDataSize,
                  key,
                  UnicodeDataCompare<UnihanData>());
  if (result == kUnihanData + kUnihanDataSize ||
      result->ucs4 != ucs4) {
    return NULL;
  }

  return result;
}

const QString LookupUnicodeData(const QString &str) {
  char32 ucs4 = 0;
  if (!ExtractFirstUCS4Char(str, &ucs4)) {
    return QString("");
  }
  UnicodeData key;
  key.ucs4 = ucs4;
  key.description = NULL;
  const UnicodeData *result =
      lower_bound(kUnicodeData,
                  kUnicodeData + kUnicodeDataSize,
                  key,
                  UnicodeDataCompare<UnicodeData>());
  if (result == kUnicodeData + kUnicodeDataSize ||
      result->ucs4 != ucs4) {
    return QString("");
  }

  return QString(result->description);
}

QString toCodeInUcs4(const QString &str) {
  char32 ucs4 = 0;
  if (!ExtractFirstUCS4Char(str, &ucs4)) {
    return "";
  }
  QString result;
  result.sprintf("U+%04X", ucs4);
  return result;
}

QString toHexUTF8(const QString &str) {
  if (str.isEmpty()) {
    return QString("--");
  }
  QByteArray array = str.toUtf8();
  QString result;
  for (int i = 0; i < array.size(); ++i) {
    QString tmp;
    tmp.sprintf("%02X ", static_cast<uint8>(array[i]));
    result += tmp;
  }
  return result;
}

QString Hexify(uint16 code) {
  if (code == 0) {
    return QString("--");
  }
  QString tmp;
  const uint16 high = (code >> 8) & 0xFF;
  const uint16 low = code & 0xFF;
  if (high == 0) {
    tmp.sprintf("%02X", low);
  } else {
    tmp.sprintf("%02X %02X", high, low);
  }
  return tmp;
}

QString toHexSJIS(const QString &str) {
  return Hexify(LookupCP932Data(str));
}

QString toHexEUC(const QString &str) {
  return Hexify(SjisToEUC(LookupCP932Data(str)));
}

QString toJapaneseReading(const char *str) {
  if (QLocale::system().language() == QLocale::Japanese) {
    string tmp = str;
    string output;
    Util::LowerString(&tmp);
    Util::RomanjiToHiragana(tmp, &output);
    return QString::fromUtf8(output.c_str());
  } else {
    return QString::fromUtf8(str);
  }
}
}  // namespace

// static
QString UnicodeUtil::GetToolTip(const QFont &font, const QString &text) {
  QString info = QString::fromLatin1
      ("<center><span style=\"font-size: 24pt; font-family: %1\">").arg
      (font.family());

  info += Qt::escape(text);
  info += "</span></center>";

  const QString desc = LookupUnicodeData(text);
  if (!desc.isEmpty()) {
    info += "<center><span>";
    info += Qt::escape(desc);
    info += "</span></center>";
  }

  info += "<table border=0>";

  const UnihanData *unihan = LookupUnihanData(text);
  if (unihan != NULL) {
    if (unihan->japanese_kun != NULL) {
      info += "<tr><td>" + QObject::tr("Kun Reading") + ":</td><td>";
      info += Qt::escape(toJapaneseReading(unihan->japanese_kun));
      info += "</td></tr>";
    }
    if (unihan->japanese_on != NULL) {
      info += "<tr><td>" + QObject::tr("On Reading") + ":</td><td>";
      info += Qt::escape(toJapaneseReading(unihan->japanese_on));
      info += "</td></tr>";
    }
    // Since radical/total_storkes defined in Unihan database are not
    // reliable, we currently don't want to display them.
    // if (unihan->radical != NULL) {
    //   info += "<tr><td>" + QObject::tr("Radical") + ":</td><td>";
    //   info += Qt::escape(QString::fromUtf8(unihan->radical));
    //   info += "</td></tr>";
    // }
    // if (unihan->total_strokes > 0) {
    //   QString tmp;
    //   tmp.sprintf("%d", unihan->total_strokes);
    //   info += "<tr><td>" + QObject::tr("Total Strokes") + ":</td><td>";
    //   info += Qt::escape(tmp);
    //   info += "</td></tr>";
    // }
    if (unihan->IRG_jsource != NULL) {
      info += "<tr><td>" + QObject::tr("Source") + ":</td><td>";
      info += Qt::escape(QString::fromUtf8(unihan->IRG_jsource));
      info += "</td></tr>";
    }
  }

  info += "<tr><td>" + QObject::tr("Unicode") + ":</td><td>";
  info += toCodeInUcs4(text) + "</td></tr>";
  info += "<tr><td>UTF-8: </td><td>";
  info += toHexUTF8(text) + "</td></tr>";
  info += "<tr><td>Shift-JIS: </td><td>";
  info += toHexSJIS(text) + "</td></tr>";
  info += "<tr><td>EUC-JP: </td><td>";
  info += toHexEUC(text) + "</td></tr>";
  info += "</table>";

  return info;
}
}  // namespace mozc

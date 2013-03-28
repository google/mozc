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

#ifndef MOZC_GUI_CHARACTER_PAD_CHARACTER_PALETTE_H_
#define MOZC_GUI_CHARACTER_PAD_CHARACTER_PALETTE_H_

#include <QtCore/QMap>
#include <QtGui/QMainWindow>
#include "base/base.h"
#include "gui/character_pad/ui_character_palette.h"

class QTextCodec;
class QClipboard;

namespace mozc {
namespace client {
class ClientInterface;
}  // namespace client

namespace gui {

class CharacterPalette :  public QMainWindow,
                          private Ui::CharacterPalette {
  Q_OBJECT;

 public:
  struct UnicodeRange {
    char32 first;
    char32 last;
  };

  struct UnicodeBlock {
    const char *name;
    UnicodeRange range;
  };

  struct LocalCharacterMap {
    uint32 from;
    uint32 ucs2;
  };

  struct CP932JumpTo {
    const char *name;
    uint32 from;
  };

  CharacterPalette(QWidget *parent = NULL);
  virtual ~CharacterPalette();

 public slots:
  void resizeEvent(QResizeEvent *event);
  void updateFontSize(int index);
  void updateFont(const QFont &font);
  void categorySelected(QTreeWidgetItem *item, int column);
  void itemSelected(const QTableWidgetItem *item);

 protected:
#ifdef OS_WIN
  bool winEvent(MSG *message, long *result);
#endif  // OS_WIN

  scoped_ptr<client::ClientInterface> client_;
  bool usage_stats_enabled_;

 private:
  void updateTableSize();

  // Unicode operation
  void showUnicodeTableByRange(const UnicodeRange &range);
  void showUnicodeTableByBlockName(const QString &block_name);

  // non-Unicode operation
  void showLocalTable(const LocalCharacterMap *local_map,
                      size_t local_map_size);

  // show Shift-JIS subcategories
  void showSJISBlockTable(const QString &name);

  QMap<QString, UnicodeRange> unicode_block_map_;
};
}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_PAD_CHARACTER_PALETTE_H_

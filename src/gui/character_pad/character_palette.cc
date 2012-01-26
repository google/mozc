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

#include "gui/character_pad/character_palette.h"

#include <QtGui/QtGui>
#include <QtGui/QMessageBox>
#include "base/util.h"
#include "gui/base/win_util.h"
#include "gui/character_pad/data/local_character_map.h"
#include "gui/character_pad/data/unicode_blocks.h"
#include "gui/character_pad/selection_handler.h"

namespace mozc {
namespace gui {

namespace {

const int kHexBase = 16;

const char kUNICODEName[]  = "Unicode";
const char kCP932Name[]    = "Shift JIS";
const char kJISX0201Name[] = "JISX 0201";
const char kJISX0208Name[] = "JISX 0208";
const char kJISX0212Name[] = "JISX 0212";
const char kJISX0213Name[] = "JISX 0213";

//   static const CP932JumpTo kCP932JumpTo[] = {
//     { "半角英数字",    0x0020 },
//     { "半角カタカナ",  0x00A1 },
//     { "全角記号",      0x8141 },
//     { "全角英数字",    0x8250 },
//     { "ひらがな",      0x829F },
//     { "かたかな",      0x8340 },
//     { "丸数字",        0x8740 },
//     { "ローマ数字",    0xFA40 },
//     { "単位",          0x875F },
//     { "その他の記号",  0x8780 },
//     { "ギリシャ文字",  0x839F },
//     { "キリル文字",    0x8440 },
//     { "罫線",          0x849F },
//     { "第一水準漢字",  0x889F },
//     { "第二水準漢字",  0x989F },
//     { NULL, 0 }
//   };

const CharacterPalette::CP932JumpTo kCP932JumpTo[] = {
  { "\xE5\x8D\x8A\xE8\xA7\x92\xE8\x8B\xB1\xE6\x95\xB0\xE5\xAD\x97",
    0x0020 },
  { "\xE5\x8D\x8A\xE8\xA7\x92\xE3\x82\xAB\xE3\x82\xBF\xE3\x82\xAB\xE3\x83\x8A",
    0x00A1 },
  { "\xE5\x85\xA8\xE8\xA7\x92\xE8\xA8\x98\xE5\x8F\xB7",
    0x8141 },
  { "\xE5\x85\xA8\xE8\xA7\x92\xE8\x8B\xB1\xE6\x95\xB0\xE5\xAD\x97",
    0x8250 },
  { "\xE3\x81\xB2\xE3\x82\x89\xE3\x81\x8C\xE3\x81\xAA",
    0x829F },
  { "\xE3\x81\x8B\xE3\x81\x9F\xE3\x81\x8B\xE3\x81\xAA",
    0x8340 },
  { "\xE4\xB8\xB8\xE6\x95\xB0\xE5\xAD\x97",
    0x8740 },
  { "\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\x9E\xE6\x95\xB0\xE5\xAD\x97",
    0xFA40 },
  { "\xE5\x8D\x98\xE4\xBD\x8D",
    0x875F },
  { "\xE3\x81\x9D\xE3\x81\xAE\xE4\xBB\x96\xE3\x81\xAE\xE8\xA8\x98\xE5\x8F\xB7",
    0x8780 },
  { "\xE3\x82\xAE\xE3\x83\xAA\xE3\x82\xB7\xE3\x83\xA3\xE6\x96\x87\xE5\xAD\x97",
    0x839F },
  { "\xE3\x82\xAD\xE3\x83\xAA\xE3\x83\xAB\xE6\x96\x87\xE5\xAD\x97",
    0x8440 },
  { "\xE7\xBD\xAB\xE7\xB7\x9A",
    0x849F },
  { "\xE7\xAC\xAC\xE4\xB8\x80\xE6\xB0\xB4\xE6\xBA\x96\xE6\xBC\xA2\xE5\xAD\x97",
    0x889F },
  { "\xE7\xAC\xAC\xE4\xBA\x8C\xE6\xB0\xB4\xE6\xBA\x96\xE6\xBC\xA2\xE5\xAD\x97",
    0x989F },
  { NULL, 0 }
};

// add child QTreeWidgetItem with the text "name" to parent.
// return child item.
QTreeWidgetItem *AddItem(QTreeWidgetItem *parent, const char *name) {
  QTreeWidgetItem *item = new QTreeWidgetItem(parent);
  item->setText(0, QString::fromUtf8(name));
  parent->addChild(item);
  return item;
}
}  // namespace

CharacterPalette::CharacterPalette(QWidget *parent)
    : QMainWindow(parent) {
  setupUi(this);

  fontComboBox->setWritingSystem
      (static_cast<QFontDatabase::WritingSystem>
       (QFontDatabase::Any));
  fontComboBox->setEditable(false);
  fontComboBox->setCurrentFont(tableWidget->font());

  QObject::connect(fontComboBox,
                   SIGNAL(currentFontChanged(const QFont &)),
                   this, SLOT(updateFont(const QFont &)));

  QObject::connect(sizeComboBox,
                   SIGNAL(currentIndexChanged(int)),
                   this, SLOT(updateFontSize(int)));

  sizeComboBox->setCurrentIndex(4);
  fontComboBox->setCurrentFont(tableWidget->font());

  // Category Tree
  QObject::connect(categoryTreeWidget,
                   SIGNAL(itemClicked(QTreeWidgetItem *, int)),
                   SLOT(categorySelected(QTreeWidgetItem *, int)));

  QTreeWidgetItem *unicode_item = new QTreeWidgetItem;
  unicode_item->setText(0, QString::fromUtf8(kUNICODEName));
  QTreeWidgetItem *sjis_item = new QTreeWidgetItem;
  sjis_item->setText(0, QString::fromUtf8(kCP932Name));
  QTreeWidgetItem *jisx0201_item = new QTreeWidgetItem;
  jisx0201_item->setText(0, QString::fromUtf8(kJISX0201Name));
  QTreeWidgetItem *jisx0208_item = new QTreeWidgetItem;
  jisx0208_item->setText(0, QString::fromUtf8(kJISX0208Name));
  QTreeWidgetItem *jisx0212_item = new QTreeWidgetItem;
  jisx0212_item->setText(0, QString::fromUtf8(kJISX0212Name));

  // Because almost all users use Shift-JIS table instead of
  // Unicode table, Shift-JIS table is selected and child
  // items of Shift-JIS table are expanded by default.
  // In order to let user know the existence of the Unicode table,
  // shows Unicode table at first, but don't expands the child items.
  categoryTreeWidget->addTopLevelItem(unicode_item);
  categoryTreeWidget->addTopLevelItem(sjis_item);
  categoryTreeWidget->addTopLevelItem(jisx0201_item);
  categoryTreeWidget->addTopLevelItem(jisx0208_item);
  categoryTreeWidget->addTopLevelItem(jisx0212_item);

  for (int i = 0; kCP932JumpTo[i].name != NULL; ++i) {
    AddItem(sjis_item, kCP932JumpTo[i].name);
  }

  // Make Unicode Block children
  for (int i = 0; kUnicodeBlockTable[i].name != NULL; ++i) {
    QTreeWidgetItem *item = new QTreeWidgetItem(unicode_item);
    item->setText(0, QObject::tr(kUnicodeBlockTable[i].name));
    unicode_item->addChild(item);
  }

  // Adjust the splitter.
  splitter->setSizes(QList<int>()
                     << static_cast<int>(width() * 0.25)
                     << static_cast<int>(width() * 0.75));

  // set default table
  showLocalTable(kCP932Map, kCP932MapSize);
  categoryTreeWidget->setCurrentItem(sjis_item);

  categoryTreeWidget->setItemExpanded(
      categoryTreeWidget->topLevelItem(0)->parent(),
      true);

  // Select "Shift-JIS" item as a default.
  categoryTreeWidget->setCurrentItem(sjis_item);
  sjis_item->setExpanded(true);

  tableWidget->setAutoScroll(false);

  repaint();
  update();
}

void CharacterPalette::updateFont(const QFont &font) {
  QFont new_font = font;
  new_font.setPointSize(tableWidget->font().pointSize());
  tableWidget->setFont(new_font);
  tableWidget->adjustSize();
  updateTableSize();
}

void CharacterPalette::updateFontSize(int index) {
  int font_point = 24;
  switch (index) {
    case 0: font_point = 32; break;
    case 1: font_point = 24; break;
    case 2: font_point = 16; break;
    case 3: font_point = 14; break;
    case 4: font_point = 12; break;
  }

  QFont font;
  font.setPointSize(font_point);
  tableWidget->setFont(font);

  updateTableSize();
  tableWidget->adjustSize();

  QTableWidgetItem *item = tableWidget->currentItem();
  if (item != NULL) {
    tableWidget->scrollToItem(item,
                              QAbstractItemView::PositionAtCenter);
  }
}

void CharacterPalette::resizeEvent(QResizeEvent *) {
  updateTableSize();
}

void CharacterPalette::updateTableSize() {
  // here we use "龍" to calc font size, as it looks almsot square
  // const char kHexBaseChar[]= "龍";
  const char kHexBaseChar[]= "\xE9\xBE\x8D";
  const QRect rect =
      QFontMetrics(tableWidget->font()).boundingRect(trUtf8(kHexBaseChar));

#ifdef OS_MACOSX
  const int width = static_cast<int>(rect.width() * 2.2);
  const int height = static_cast<int>(rect.height() * 2.0);
#else
  const int width = static_cast<int>(rect.width() * 1.6);
  const int height = static_cast<int>(rect.height() * 1.2);
#endif

  for (int j = 0; j < tableWidget->columnCount(); ++j) {
    tableWidget->setColumnWidth(j, width);
  }
  for (int j = 0; j < tableWidget->rowCount(); ++j) {
    tableWidget->setRowHeight(j, height);
  }
  tableWidget->setLookupResultItem(NULL);
}

void CharacterPalette::categorySelected(QTreeWidgetItem *item,
                                        int column) {
  const QString &text = item->text(column);
  const QTreeWidgetItem *parent = item->parent();

  item->setExpanded(!item->isExpanded());

  if (text == kUNICODEName) {     // TOP Unicode
    showUnicodeAllTable();
  } else if (parent != NULL && parent->text(0) == kUNICODEName) {
    showUnicodeBlockTable(text);
  } else if (parent != NULL && parent->text(0) == kCP932Name) {
    showSJISBlockTable(text);
  } else if (text == kJISX0201Name) {
    showLocalTable(kJISX0201Map, kJISX0201MapSize);
  } else if (text == kJISX0208Name) {
    showLocalTable(kJISX0208Map, kJISX0208MapSize);
  } else if (text == kJISX0212Name) {
    showLocalTable(kJISX0212Map, kJISX0212MapSize);
  } else if (text == kCP932Name) {
    showLocalTable(kCP932Map, kCP932MapSize);
  }
}

// Unicode operations
void CharacterPalette::showUnicodeTable(int start, int end) {
  tableWidget->hide();
  tableWidget->clear();

  QStringList column_header;
  for (int i = 0; i < kHexBase; ++i) {
    column_header << QString::number(i, kHexBase).toUpper();
  }

  QStringList row_header;
  for (int i = start; i < end; i += kHexBase) {
    QString str;
    str.sprintf("U+%3.3X0", i / kHexBase);
    row_header << str;
  }

  tableWidget->setColumnCount(kHexBase);
  tableWidget->setRowCount(row_header.size());

  tableWidget->setHorizontalHeaderLabels(column_header);
  tableWidget->setVerticalHeaderLabels(row_header);

  const int offset = start / kHexBase;
  for (int i = start; i < end; ++i) {
    // We do not use QString(QChar(i)) but Util::UCS4ToUTF8 because
    // QChar is only 16-bit.
    string utf8;
    Util::UCS4ToUTF8(i, &utf8);
    QTableWidgetItem *item = new QTableWidgetItem(
        QString::fromUtf8(utf8.data(), utf8.size()));
    item->setTextAlignment(Qt::AlignCenter);
    tableWidget->setItem(i / kHexBase - offset, i % kHexBase, item);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
  }

  tableWidget->scrollToItem(tableWidget->item(0, 0),
                            QAbstractItemView::PositionAtTop);
  tableWidget->setLookupResultItem(NULL);
  tableWidget->show();
}

void CharacterPalette::showUnicodeAllTable() {
  showUnicodeTable(0, 65536);  // all Unicode Range
}

void CharacterPalette::showSJISBlockTable(const QString &name) {
  const CP932JumpTo *block = NULL;
  for (int i = 0; kCP932JumpTo[i].name !=NULL; ++i) {
    if (name == QString::fromUtf8(kCP932JumpTo[i].name)) {
      block = &kCP932JumpTo[i];
      break;
    }
  }

  if (block == NULL) {
    return;
  }

  showLocalTable(kCP932Map, kCP932MapSize);

  tableWidget->hide();
  QTableWidgetItem *item = tableWidget->item(block->from / kHexBase -
                                             kCP932Map[0].from / kHexBase,
                                             block->from % kHexBase);

  if (item != NULL) {
    tableWidget->scrollToItem(item, QAbstractItemView::PositionAtTop);
    item->setSelected(true);
  }

  tableWidget->setLookupResultItem(NULL);
  tableWidget->show();
}

void CharacterPalette::showUnicodeBlockTable(const QString &name) {
  if (name == kUNICODEName) {
    showUnicodeAllTable();
    return;
  }

  const UnicodeBlock *block = NULL;
  for (int i = 0; kUnicodeBlockTable[i].name != NULL; ++i) {
    if (name == kUnicodeBlockTable[i].name ||
        name == QObject::tr(kUnicodeBlockTable[i].name)) {
      block = &kUnicodeBlockTable[i];
      break;
    }
  }

  if (block == NULL) {
    return;
  }

  showUnicodeTable(block->start, block->end);
}

// Local table
void CharacterPalette::showLocalTable(const LocalCharacterMap *local_map,
                                      size_t local_map_size) {
  tableWidget->hide();
  tableWidget->clear();

  QStringList column_header;
  for (int i = 0; i < kHexBase; ++i) {
    column_header << QString::number(i, kHexBase).toUpper();
  }

  // find range
  const int from_start = local_map[0].from;
  const int from_end   = local_map[local_map_size - 1].from + kHexBase;

  QStringList row_header;
  for (int i = from_start; i < from_end; i += kHexBase) {
    QString str;
    str.sprintf("0x%X0", i / kHexBase);
    row_header << str;
  }

  tableWidget->setColumnCount(kHexBase);
  tableWidget->setRowCount(row_header.size());

  tableWidget->setHorizontalHeaderLabels(column_header);
  tableWidget->setVerticalHeaderLabels(row_header);

  const int offset = from_start / kHexBase;
  for (size_t i = 0; i < local_map_size; ++i) {
    // We do not use QString(QChar(i)) but Util::UCS4ToUTF8 because
    // QChar is only 16-bit.
    string utf8;
    Util::UCS4ToUTF8(local_map[i].ucs2, &utf8);
    QTableWidgetItem *item = new QTableWidgetItem(
        QString::fromUtf8(utf8.data(), utf8.size()));
    item->setTextAlignment(Qt::AlignCenter);
    tableWidget->setItem(local_map[i].from / kHexBase - offset,
                         local_map[i].from % kHexBase, item);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
  }

  tableWidget->scrollToItem(tableWidget->item(0, 0),
                            QAbstractItemView::PositionAtCenter);

  tableWidget->setLookupResultItem(NULL);
  tableWidget->show();
}

#ifdef OS_WINDOWS
bool CharacterPalette::winEvent(MSG *message, long *result) {
  if (message != NULL &&
      message->message == WM_LBUTTONDOWN &&
      WinUtil::IsCompositionEnabled()) {
    const QWidget *widget = qApp->widgetAt(
        mapToGlobal(QPoint(message->lParam & 0xFFFF,
                           (message->lParam >> 16) & 0xFFFF)));
    if (widget == centralwidget) {
      ::PostMessage(message->hwnd, WM_NCLBUTTONDOWN,
                    static_cast<WPARAM>(HTCAPTION), message->lParam);
      return true;
    }
  }

  return QWidget::winEvent(message, result);
}
#endif  // OS_WINDOWS
}  // namespace gui
}  // namespace mozc

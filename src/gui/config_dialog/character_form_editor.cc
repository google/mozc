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

#include "gui/config_dialog/character_form_editor.h"

#include <QtGui/QtGui>
#include <QtWidgets/QHeaderView>
#include <memory>

#include "config/config_handler.h"
#include "gui/config_dialog/combobox_delegate.h"
#include "absl/memory/memory.h"

namespace mozc {
namespace gui {

namespace {
QString FormToString(config::Config::CharacterForm form) {
  switch (form) {
    case config::Config::FULL_WIDTH:
      return QObject::tr("Fullwidth");
      break;
    case config::Config::HALF_WIDTH:
      return QObject::tr("Halfwidth");
      break;
    case config::Config::LAST_FORM:
      return QObject::tr("Remember");
      break;
    default:
      return QObject::tr("Unknown");
      break;
  }
}

config::Config::CharacterForm StringToForm(const QString &str) {
  if (str == QObject::tr("Fullwidth")) {
    return config::Config::FULL_WIDTH;
  } else if (str == QObject::tr("Halfwidth")) {
    return config::Config::HALF_WIDTH;
  } else if (str == QObject::tr("Remember")) {
    return config::Config::LAST_FORM;
  }
  return config::Config::FULL_WIDTH;  // failsafe
}

QString GroupToString(const std::string &str) {
  // if (str == "ア") {
  if (str == "ア") {
    return QObject::tr("Katakana");
  } else if (str == "0") {
    return QObject::tr("Numbers");
  } else if (str == "A") {
    return QObject::tr("Alphabets");
  }
  return QString::fromUtf8(str.c_str());
}

const std::string StringToGroup(const QString &str) {
  if (str == QObject::tr("Katakana")) {
    // return "ア";
    return "ア";
  } else if (str == QObject::tr("Numbers")) {
    return "0";
  } else if (str == QObject::tr("Alphabets")) {
    return "A";
  }
  return str.toUtf8().data();
}
}  // namespace

CharacterFormEditor::CharacterFormEditor(QWidget *parent)
    : QTableWidget(parent), delegate_(new ComboBoxDelegate) {
  QStringList item_list;
  item_list << tr("Fullwidth") << tr("Halfwidth") << tr("Remember");
  delegate_->SetItemList(item_list);
  setEditTriggers(QAbstractItemView::AllEditTriggers);
  setItemDelegate(delegate_.get());
  setToolTip(tr("Character form editor"));
  setColumnCount(3);
  setAlternatingRowColors(true);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setSelectionBehavior(QAbstractItemView::SelectItems);
  verticalHeader()->hide();
#ifdef __APPLE__
  // grid is basically hidden in mac ui.
  // Please take a look at iTunes.
  setShowGrid(false);
#endif
}

CharacterFormEditor::~CharacterFormEditor() {}

void CharacterFormEditor::Load(const config::Config &config) {
  clear();
  QStringList header;
  header << tr("Group") << tr("Composition") << tr("Conversion");
  setHorizontalHeaderLabels(header);

  std::unique_ptr<config::Config> default_config;
  const config::Config *target_config = &config;

  // make sure that table isn't empty.
  if (config.character_form_rules_size() == 0) {
    default_config = absl::make_unique<config::Config>();
    config::ConfigHandler::GetDefaultConfig(default_config.get());
    target_config = default_config.get();
  }

  setRowCount(0);
  setRowCount(target_config->character_form_rules_size());

  for (size_t row = 0; row < target_config->character_form_rules_size();
       ++row) {
    const config::Config::CharacterFormRule &rule =
        target_config->character_form_rules(row);
    const QString group = GroupToString(rule.group());
    const QString preedit = FormToString(rule.preedit_character_form());
    const QString conversion = FormToString(rule.conversion_character_form());

    QTableWidgetItem *item_group = new QTableWidgetItem(group);
    item_group->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    QTableWidgetItem *item_preedit = new QTableWidgetItem(preedit);
    item_preedit->setTextAlignment(Qt::AlignCenter);
    QTableWidgetItem *item_conversion = new QTableWidgetItem(conversion);
    item_conversion->setTextAlignment(Qt::AlignCenter);

    // Preedit Katakan is always FULLWIDTH
    // This item should not be editable
    if (group == QObject::tr("Katakana")) {
      item_preedit->setFlags(nullptr);  // disable flag
    }

    setItem(row, 0, item_group);
    setItem(row, 1, item_preedit);
    setItem(row, 2, item_conversion);
    const int height = rowHeight(row);
    setRowHeight(row, static_cast<int>(height * 0.7));
  }

  setColumnWidth(0, static_cast<int>(width() * 0.3));
  setColumnWidth(1, static_cast<int>(width() * 0.3));
  setColumnWidth(2, static_cast<int>(width() * 0.3));
}

void CharacterFormEditor::Save(config::Config *config) {
  if (rowCount() == 0) {
    return;
  }

  config->clear_character_form_rules();
  for (int row = 0; row < rowCount(); ++row) {
    if (item(row, 0)->text().isEmpty()) {
      continue;
    }
    const std::string group = StringToGroup(item(row, 0)->text());
    config::Config::CharacterForm preedit_form =
        StringToForm(item(row, 1)->text());
    config::Config::CharacterForm conversion_form =
        StringToForm(item(row, 2)->text());
    config::Config::CharacterFormRule *rule =
        config->add_character_form_rules();
    rule->set_group(group);
    rule->set_preedit_character_form(preedit_form);
    rule->set_conversion_character_form(conversion_form);
  }
}
}  // namespace gui
}  // namespace mozc

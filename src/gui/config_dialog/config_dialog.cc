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

// Qt component of configure dialog for Mozc
#include "gui/config_dialog/config_dialog.h"

#ifdef OS_WINDOWS
#include <QtGui/QWindowsStyle>
#include <windows.h>
#endif

#include <algorithm>
#include <stdlib.h>
#include <QtGui/QMessageBox>
#include "base/const.h"
#include "base/util.h"
#include "base/run_level.h"
#include "base/stats_config_util.h"
#include "gui/config_dialog/keymap_editor.h"
#include "gui/config_dialog/roman_table_editor.h"
#include "ipc/ipc.h"
#include "session/commands.pb.h"
#include "session/config_handler.h"
#include "session/config.pb.h"
#include "client/session.h"

namespace mozc {

namespace {
static const char kServerName[] = "session";
}  // namespace

namespace gui {

ConfigDialog::ConfigDialog()
    : client_(new client::Session),
      initial_preedit_method_(0) {
  setupUi(this);
  setWindowFlags(Qt::WindowSystemMenuHint);
  setWindowModality(Qt::NonModal);

#ifdef NO_LOGGING
  // disable logging options
  loggingLabel->setVisible(false);
  loggingLine->setVisible(false);
  verboseLevelLabel->setVisible(false);
  verboseLevelComboBox->setVisible(false);

#if defined(OS_MACOSX) || defined(OS_LINUX)
  // The last "misc" tab has no valid configs on Mac and Linux
  const int kMiscTabIndex = 5;
  configDialogTabWidget->removeTab(kMiscTabIndex);
#endif  // OS_MACOSX || OS_LINUX
#endif  // NO_LOGGING

  client_->set_restricted(true);   // start with restricted mode

  suggestionsSizeSpinBox->setRange(1, 9);

  // punctuationsSettingComboBox->addItem(QString::fromUtf8("、。"));
  // punctuationsSettingComboBox->addItem(QString::fromUtf8("，．"));
  // punctuationsSettingComboBox->addItem(QString::fromUtf8("、．"));
  // punctuationsSettingComboBox->addItem(QString::fromUtf8("，。"));
  punctuationsSettingComboBox->addItem
      (QString::fromUtf8("\xE3\x80\x81\xE3\x80\x82"));
  punctuationsSettingComboBox->addItem
      (QString::fromUtf8("\xEF\xBC\x8C\xEF\xBC\x8E"));
  punctuationsSettingComboBox->addItem
      (QString::fromUtf8("\xE3\x80\x81\xEF\xBC\x8E"));
  punctuationsSettingComboBox->addItem
      (QString::fromUtf8("\xEF\xBC\x8C\xE3\x80\x82"));

  // symbolsSettingComboBox->addItem(QString::fromUtf8("「」・"));
  // symbolsSettingComboBox->addItem(QString::fromUtf8("[]／"));
  // symbolsSettingComboBox->addItem(QString::fromUtf8("「」／"));
  // symbolsSettingComboBox->addItem(QString::fromUtf8("[]・"));
  symbolsSettingComboBox->addItem(
      QString::fromUtf8("\xE3\x80\x8C\xE3\x80\x8D\xE3\x83\xBB"));
  symbolsSettingComboBox->addItem(
      QString::fromUtf8("[]\xEF\xBC\x8F"));
  symbolsSettingComboBox->addItem(
      QString::fromUtf8("\xE3\x80\x8C\xE3\x80\x8D\xEF\xBC\x8F"));
  symbolsSettingComboBox->addItem(
      QString::fromUtf8("[]\xE3\x83\xBB"));

  keymapSettingComboBox->addItem(tr("Custom keymap"));
  keymapSettingComboBox->addItem(tr("ATOK"));
  keymapSettingComboBox->addItem(tr("MS-IME"));
  keymapSettingComboBox->addItem(tr("Kotoeri"));

  inputModeComboBox->addItem(tr("Romaji"));
  inputModeComboBox->addItem(tr("Kana"));

  spaceCharacterFormComboBox->addItem(tr("Follow input mode"));
  spaceCharacterFormComboBox->addItem(tr("Fullwidth"));
  spaceCharacterFormComboBox->addItem(tr("Halfwidth"));

  selectionShortcutModeComboBox->addItem(tr("No shortcut"));
  selectionShortcutModeComboBox->addItem(tr("1 -- 9"));
  selectionShortcutModeComboBox->addItem(tr("A -- L"));

  historyLearningLevelComboBox->addItem(tr("Yes"));
  historyLearningLevelComboBox->addItem(tr("Yes (don't record new data)"));
  historyLearningLevelComboBox->addItem(tr("No"));

  shiftKeyModeSwitchComboBox->addItem(tr("Off"));
  shiftKeyModeSwitchComboBox->addItem(tr("Alphanumeric"));
  shiftKeyModeSwitchComboBox->addItem(tr("Katakana"));

  numpadCharacterFormComboBox->addItem(tr("Follow input mode"));
  numpadCharacterFormComboBox->addItem(tr("Fullwidth"));
  numpadCharacterFormComboBox->addItem(tr("Halfwidth"));
  numpadCharacterFormComboBox->addItem(tr("Direct input"));

  verboseLevelComboBox->addItem(tr("0"));
  verboseLevelComboBox->addItem(tr("1"));
  verboseLevelComboBox->addItem(tr("2"));


  // signal/slot
  QObject::connect(configDialogButtonBox,
                   SIGNAL(clicked(QAbstractButton *)),
                   this,
                   SLOT(clicked(QAbstractButton *)));
  QObject::connect(clearUserHistoryButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(ClearUserHistory()));
  QObject::connect(clearUserPredictionButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(ClearUserPrediction()));
  QObject::connect(clearUnusedUserPredictionButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(ClearUnusedUserPrediction()));
  QObject::connect(editUserDictionaryButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(EditUserDictionary()));
  QObject::connect(editKeymapButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(EditKeymap()));
  QObject::connect(resetToDefaultsButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(ResetToDefaults()));
  QObject::connect(editRomanTableButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(EditRomanTable()));
  QObject::connect(inputModeComboBox,
                   SIGNAL(currentIndexChanged(int)),
                   this,
                   SLOT(SelectInputModeSetting(int)));

  QObject::connect(keymapSettingComboBox,
                   SIGNAL(currentIndexChanged(int)),
                   this,
                   SLOT(SelectKeymapSetting(int)));
  QObject::connect(launchAdministrationDialogButton,
                   SIGNAL(clicked()),
                   this,
                   SLOT(LaunchAdministrationDialog()));
  QObject::connect(launchAdministrationDialogButtonForUsageStats,
                   SIGNAL(clicked()),
                   this,
                   SLOT(LaunchAdministrationDialog()));

  // When clicking these messages, CheckBoxs corresponding
  // to them should be toggled.
  // We cannot use connect/slot as QLabel doesn't define
  // clicked slot by default.
  usageStatsMessage->installEventFilter(this);
  incognitoModeMessage->installEventFilter(this);
  checkDefaultMessage->installEventFilter(this);

#ifndef OS_WINDOWS
  checkDefaultCheckBox->setVisible(false);
  checkDefaultLine->setVisible(false);
  checkDefaultLabel->setVisible(false);
  checkDefaultMessage->setVisible(false);
#endif   // !OS_WINDOWS

#ifdef OS_WINDOWS
  launchAdministrationDialogButton->setEnabled(true);
  // if the current application is not elevated by UAC,
  // add a shield icon
  if (mozc::Util::IsVistaOrLater()) {
    if (!mozc::RunLevel::IsElevatedByUAC()) {
      QWindowsStyle style;
      QIcon vista_icon(style.standardIcon(QStyle::SP_VistaShield));
      launchAdministrationDialogButton->setIcon(vista_icon);
      launchAdministrationDialogButtonForUsageStats->setIcon(vista_icon);
    }
  } else {
    dictionaryPreloadingAndUACLabel->setText(
        tr("Dictionary preloading"));
  }

  usageStatsCheckBox->setDisabled(true);
  usageStatsCheckBox->setVisible(false);
  usageStatsMessage->setDisabled(true);
  usageStatsMessage->setVisible(false);
#else
  launchAdministrationDialogButton->setEnabled(false);
  launchAdministrationDialogButton->setVisible(false);
  launchAdministrationDialogButtonForUsageStats->setEnabled(false);
  launchAdministrationDialogButtonForUsageStats->setVisible(false);
  administrationLine->setVisible(false);
  administrationLabel->setVisible(false);
  dictionaryPreloadingAndUACLabel->setVisible(false);
#endif  // OS_WINDOWS

#ifdef OS_LINUX
  // On Linux, disable all fields for UsageStats
  usageStatsLabel->setEnabled(false);
  usageStatsLabel->setVisible(false);
  usageStatsLine->setEnabled(false);
  usageStatsLine->setVisible(false);
  usageStatsMessage->setEnabled(false);
  usageStatsMessage->setVisible(false);
  usageStatsCheckBox->setEnabled(false);
  usageStatsCheckBox->setVisible(false);
#endif // OS_LINUX

  config::Config config;
  if (!GetConfig(&config)) {
    QMessageBox::critical(this,
                          tr("Mozc settings"),
                          tr("Failed to get current config values"));
  }
  ConvertFromProto(config);

  initial_preedit_method_ = static_cast<int>(config.preedit_method());

  // If the keymap is a custome keymap (= 0), the buttion is activated.
  editKeymapButton->setEnabled(keymapSettingComboBox->currentIndex() == 0);

#ifdef CHANNEL_DEV
  usageStatsCheckBox->setEnabled(false);
#endif  // CHANNEL_DEV
}

ConfigDialog::~ConfigDialog() {
}

bool ConfigDialog::SetConfig(const config::Config &config) {
#ifndef OS_MACOSX
  if (!client_->CheckVersionOrRestartServer()) {
    LOG(ERROR) << "CheckVersionOrRestartServer failed";
    return false;
  }
#endif  // OS_MACOSX

  if (!client_->SetConfig(config)) {
    LOG(ERROR) << "SetConfig failed";
    return false;
  }

  return true;
}

bool ConfigDialog::GetConfig(config::Config *config) {
#ifndef OS_MACOSX
  if (!client_->CheckVersionOrRestartServer()) {
    LOG(ERROR) << "CheckVersionOrRestartServer failed";
    return false;
  }
#endif  // OS_MACOSX

  if (!client_->GetConfig(config)) {
    LOG(ERROR) << "GetConfig failed";
    return false;
  }

  return true;
}

bool ConfigDialog::Update() {
  config::Config config;
  ConvertToProto(&config);

  if (config.session_keymap() == config::Config::CUSTOM &&
      config.custom_keymap_table().empty()) {
    QMessageBox::warning(
        this,
        tr("Mozc settings"),
        tr("The current custom keymap table is empty. "
           "When custom keymap is selected, "
           "you must customize it."));
    return false;
  }


#if defined(OS_WINDOWS) || defined(OS_LINUX)
  if (initial_preedit_method_ !=
      static_cast<int>(config.preedit_method())) {
    QMessageBox::information(this,
                             tr("Mozc settings"),
                             tr("Romaji/Kana setting is enabled from"
                                " new applications."));
  }
#endif

  if (!SetConfig(config)) {
    QMessageBox::critical(this,
                          tr("Mozc settings"),
                          tr("Failed to update config"));
  }

  return true;
}

void ConfigDialog::SetSendStatsCheckBox() {
  // On windows, usage_stats flag is managed by
  // administration_dialog. http://b/issue?id=2889759
#ifndef OS_WINDOWS
  const bool val = StatsConfigUtil::IsEnabled();
  usageStatsCheckBox->setChecked(val);
#endif  // OS_WINDOWS
}

void ConfigDialog::GetSendStatsCheckBox() const {
  // On windows, usage_stats flag is managed by
  // administration_dialog. http://b/issue?id=2889759
#ifndef OS_WINDOWS
  const bool val = usageStatsCheckBox->isChecked();
  StatsConfigUtil::SetEnabled(val);
#endif  // OS_WINDOWS
}

#define SET_COMBOBOX(combobox, enumname, field) \
do { \
  (combobox)->setCurrentIndex(static_cast<int>(config.field()));  \
} while (0)

#define SET_CHECKBOX(checkbox, field) \
do { (checkbox)->setChecked(config.field()); } while (0)

#define GET_COMBOBOX(combobox, enumname, field) \
do {  \
  config->set_##field(static_cast<config::Config_##enumname> \
                   ((combobox)->currentIndex())); \
} while (0)

#define GET_CHECKBOX(checkbox, field) \
do { config->set_##field((checkbox)->isChecked());  } while (0)

// TODO(taku)
// Actually ConvertFromProto and ConvertToProto are almost the same.
// The difference only SET_ and GET_. We would like to unify the twos.
void ConfigDialog::ConvertFromProto(const config::Config &config) {
  // tab 1
  SET_COMBOBOX(inputModeComboBox, PreeditMethod, preedit_method);
  SET_COMBOBOX(punctuationsSettingComboBox, PunctuationMethod,
               punctuation_method);
  SET_COMBOBOX(symbolsSettingComboBox, SymbolMethod, symbol_method);
  SET_COMBOBOX(spaceCharacterFormComboBox, FundamentalCharacterForm,
               space_character_form);
  SET_COMBOBOX(selectionShortcutModeComboBox, SelectionShortcut,
               selection_shortcut);
  SET_COMBOBOX(numpadCharacterFormComboBox, NumpadCharacterForm,
               numpad_character_form);
  SET_COMBOBOX(keymapSettingComboBox, SessionKeymap, session_keymap);

  custom_keymap_table_ = config.custom_keymap_table();
  custom_roman_table_ = config.custom_roman_table();

  // tab2
  SET_COMBOBOX(historyLearningLevelComboBox, HistoryLearningLevel,
               history_learning_level);
  SET_CHECKBOX(singleKanjiDictionaryCheckBox, use_single_kanji_conversion);
  SET_CHECKBOX(symbolDictionaryCheckBox, use_symbol_conversion);
  SET_CHECKBOX(dateConversionCheckBox, use_date_conversion);
  SET_CHECKBOX(numberConversionCheckBox, use_number_conversion);

  // tab3
  SET_CHECKBOX(useAutoImeTurnOff, use_auto_ime_turn_off);
  SET_COMBOBOX(shiftKeyModeSwitchComboBox,
               ShiftKeyModeSwitch,
               shift_key_mode_switch);

  // tab4
  SET_CHECKBOX(historySuggestCheckBox, use_history_suggest);
  SET_CHECKBOX(dictionarySuggestCheckBox, use_dictionary_suggest);

  suggestionsSizeSpinBox->setValue
      (max(1, min(9, static_cast<int>(config.suggestions_size()))));

  // tab5
  SetSendStatsCheckBox();
  SET_CHECKBOX(incognitoModeCheckBox, incognito_mode);

  // tab6
  SET_COMBOBOX(verboseLevelComboBox, int, verbose_level);
  SET_CHECKBOX(checkDefaultCheckBox, check_default);

  characterFormEditor->Load(config);
}

void ConfigDialog::ConvertToProto(config::Config *config) const {
  // tab 1
  GET_COMBOBOX(inputModeComboBox, PreeditMethod,
               preedit_method);
  GET_COMBOBOX(punctuationsSettingComboBox, PunctuationMethod,
               punctuation_method);
  GET_COMBOBOX(symbolsSettingComboBox, SymbolMethod, symbol_method);
  GET_COMBOBOX(spaceCharacterFormComboBox, FundamentalCharacterForm,
               space_character_form);
  GET_COMBOBOX(selectionShortcutModeComboBox, SelectionShortcut,
               selection_shortcut);
  GET_COMBOBOX(numpadCharacterFormComboBox, NumpadCharacterForm,
               numpad_character_form);
  GET_COMBOBOX(keymapSettingComboBox, SessionKeymap, session_keymap);

  config->set_custom_keymap_table(custom_keymap_table_);

  config->clear_custom_roman_table();
  if (!custom_roman_table_.empty()) {
    config->set_custom_roman_table(custom_roman_table_);
  }

  // tab2
  GET_COMBOBOX(historyLearningLevelComboBox, HistoryLearningLevel,
               history_learning_level);
  GET_CHECKBOX(singleKanjiDictionaryCheckBox, use_single_kanji_conversion);
  GET_CHECKBOX(symbolDictionaryCheckBox, use_symbol_conversion);
  GET_CHECKBOX(dateConversionCheckBox, use_date_conversion);
  GET_CHECKBOX(numberConversionCheckBox, use_number_conversion);

  // tab3
  GET_CHECKBOX(useAutoImeTurnOff, use_auto_ime_turn_off);
  GET_COMBOBOX(shiftKeyModeSwitchComboBox,
               ShiftKeyModeSwitch,
               shift_key_mode_switch);

  // tab4
  GET_CHECKBOX(historySuggestCheckBox, use_history_suggest);
  GET_CHECKBOX(dictionarySuggestCheckBox, use_dictionary_suggest);

  config->set_suggestions_size
      (static_cast<uint32>(suggestionsSizeSpinBox->value()));

  // tab5
  GetSendStatsCheckBox();
  GET_CHECKBOX(incognitoModeCheckBox, incognito_mode);

  // tab6
  config->set_verbose_level(verboseLevelComboBox->currentIndex());
  GET_CHECKBOX(checkDefaultCheckBox, check_default);

  characterFormEditor->Save(config);
}

#undef SET_COMBOBOX
#undef SET_CHECKBOX
#undef GET_COMBOBOX
#undef GET_CHECKBOX

void ConfigDialog::clicked(QAbstractButton *button) {
  switch (configDialogButtonBox->buttonRole(button)) {
    case QDialogButtonBox::AcceptRole:
      if (Update()) {
        QWidget::close();
      }
      break;
    case QDialogButtonBox::ApplyRole:
      Update();
      break;
    case QDialogButtonBox::RejectRole:
      QWidget::close();
      break;
    default:
      break;
  }
}

void ConfigDialog::ClearUserHistory() {
  if (QMessageBox::Ok !=
      QMessageBox::question(
          this,
          tr("Mozc settings"),
          tr("Do you want to clear personalization data?"),
          QMessageBox::Ok | QMessageBox::Cancel,
          QMessageBox::Cancel)) {
    return;
  }

#ifndef OS_MACOSX
  client_->CheckVersionOrRestartServer();
#endif  // OS_MACOSX

  if (!client_->ClearUserHistory()) {
    QMessageBox::critical(
        this,
        tr("Mozc settings"),
        tr("Mozc Converter is not running. "
           "Settings were not saved."));
  }
}

void ConfigDialog::ClearUserPrediction() {
  if (QMessageBox::Ok !=
      QMessageBox::question(
          this,
          tr("Mozc settings"),
          tr("Do you want to clear all history data?"),
          QMessageBox::Ok | QMessageBox::Cancel,
          QMessageBox::Cancel)) {
    return;
  }

#ifndef OS_MACOSX
  client_->CheckVersionOrRestartServer();
#endif  // OS_MACOSX

  if (!client_->ClearUserPrediction()) {
    QMessageBox::critical(
        this,
        tr("Mozc settings"),
        tr("Mozc Converter is not running. "
           "Settings were not saved"));
  }
}

void ConfigDialog::ClearUnusedUserPrediction() {
  if (QMessageBox::Ok !=
      QMessageBox::question(
          this,
          tr("Mozc settings"),
          tr("Do you want to clear unused history data?"),
          QMessageBox::Ok | QMessageBox::Cancel,
          QMessageBox::Cancel)) {
    return;
  }

#ifndef OS_MACOSX
  client_->CheckVersionOrRestartServer();
#endif  // OS_MACOSX

  if (!client_->ClearUnusedUserPrediction()) {
    QMessageBox::critical(
        this,
        tr("Mozc settings"),
        tr("Mozc Converter is not running. "
           "operation was not executed"));
  }
}

void ConfigDialog::EditUserDictionary() {
  client_->LaunchTool("dictionary_tool", "");
}

void ConfigDialog::EditKeymap() {
  string output;
  if (gui::KeyMapEditorDialog::Show(this,
                                    custom_keymap_table_,
                                    &output)) {
    custom_keymap_table_ = output;
  }
}

void ConfigDialog::EditRomanTable() {
  string output;
  if (gui::RomanTableEditorDialog::Show(this,
                                        custom_roman_table_,
                                        &output)) {
    custom_roman_table_ = output;
  }
}

void ConfigDialog::SelectKeymapSetting(int index) {
  // enable "EDIT" button if custom is selected.
  editKeymapButton->setEnabled((index == 0));
}

void ConfigDialog::SelectInputModeSetting(int index) {
  // enable "EDIT" button if roman mode is selected
  editRomanTableButton->setEnabled((index == 0));
}

void ConfigDialog::ResetToDefaults() {
  if (QMessageBox::Ok ==
      QMessageBox::question(
          this,
          tr("Mozc settings"),
          tr("When you reset Mozc settings, any changes you've made "
             "will be reverted to the default settings. "
             "Do you want to reset settings? "
             "The following items are not reset with this operation.\n"
             " - Personalization data\n"
             " - Input history\n"
             " - Usage statistics and crash reports\n"
             " - Administrator settings"),
          QMessageBox::Ok | QMessageBox::Cancel,
          QMessageBox::Cancel)) {
    // TODO(taku): remove the dependency to config::ConfigHandler
    // nice to have GET_DEFAULT_CONFIG command
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    ConvertFromProto(config);
  }
}

void ConfigDialog::LaunchAdministrationDialog() {
#ifdef OS_WINDOWS
  client_->LaunchTool("administration_dialog", "");
#endif
}

// Catch MouseButtonRelease event to toggle the CheckBoxes
bool ConfigDialog::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::MouseButtonRelease) {
    if (obj == usageStatsMessage) {
#ifndef CHANNEL_DEV
      usageStatsCheckBox->toggle();
#endif
    } else if (obj == incognitoModeMessage) {
      incognitoModeCheckBox->toggle();
    } else if (obj == checkDefaultMessage) {
      checkDefaultCheckBox->toggle();
    }
  }
  return QObject::eventFilter(obj, event);
}
}  // namespace gui
}  // namespace mozc

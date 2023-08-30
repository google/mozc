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

// Qt component of configure dialog for Mozc

#ifndef MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_H_
#define MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_H_

#include <QObject>
#include <QTimer>
#include <map>
#include <memory>
#include <string>

#include "protocol/config.pb.h"
#include "gui/config_dialog/ui_config_dialog.h"

namespace mozc {

namespace client {
class ClientInterface;
}

namespace config {
class Config;
}

namespace gui {
class ConfigDialog : public QDialog, private Ui::ConfigDialog {
  Q_OBJECT;

 public:
  ConfigDialog();
  ~ConfigDialog() override;

  // Methods defined in the 'slots' section (Qt's extension) will be processed
  // by Qt's moc tool (moc.exe on Windows). Unfortunately, preprocessor macros
  // defined for C/C++ are not automatically passed into the moc tool.
  // For example, you need to call the moc tool with '-D' option as
  // 'moc -DENABLE_FOOBER ...' to make the moc tool aware of the ENABLE_FOOBER
  // macro. http://developer.qt.nokia.com/doc/qt-4.8/moc.html
  // So basically we must not use any #ifdef macro in slot declarations.
  // Otherwise, methods enclosed by "ifdef ENABLE_FOOBER" will be simply ignored
  // by the moc tool and |QObject::connect| against these methods results in
  // failure. See b/5935351 about how we found this issue.
 protected slots:
  virtual void clicked(QAbstractButton *button);
  virtual void ClearUserHistory();
  virtual void ClearUserPrediction();
  virtual void ClearUnusedUserPrediction();
  virtual void EditUserDictionary();
  virtual void EditKeymap();
  virtual void EditRomanTable();
  virtual void ResetToDefaults();
  virtual void SelectInputModeSetting(int index);
  virtual void SelectAutoConversionSetting(int state);
  virtual void SelectSuggestionSetting(int state);
  virtual void LaunchAdministrationDialog();
  virtual void EnableApplyButton();

 protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

 private:
  bool GetConfig(config::Config *config);
  bool SetConfig(const config::Config &config);
  // Set/GetSendStatsChechBox read/write registry or file directly
  // instead of config protobuf.
  void SetSendStatsCheckBox();
  void GetSendStatsCheckBox() const;
  void ConvertToProto(config::Config *config) const;
  void ConvertFromProto(const config::Config &config);
  bool Update();
  void Reload();

  std::unique_ptr<client::ClientInterface> client_;
  std::string custom_keymap_table_;
  std::string custom_roman_table_;
  // base_config_ keeps the original config imported from the file including
  // unconfigurable options with the GUI (e.g. composing_timeout_threshold_msec)
  config::Config base_config_;
  int initial_preedit_method_;
  bool initial_use_keyboard_to_change_preedit_method_;
  bool initial_use_mode_indicator_;
  std::map<QString, config::Config::SessionKeymap>
      keymapname_sessionkeymap_map_;
};
}  // namespace gui
}  // namespace mozc
#endif  // MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_H_

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

#ifndef MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_H_
#define MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_H_

#include <QtCore/QObject>
#include <string>
#include "base/base.h"
#include "gui/config_dialog/ui_config_dialog.h"

namespace mozc {

namespace client {
class Session;
}

namespace config {
class Config;
}

namespace gui {
class ConfigDialog : public QDialog,
                     private Ui::ConfigDialog {
  Q_OBJECT;

 public:
  ConfigDialog();
  virtual ~ConfigDialog();

 protected slots:
  virtual void clicked(QAbstractButton *button);
  virtual void ClearUserHistory();
  virtual void ClearUserPrediction();
  virtual void ClearUnusedUserPrediction();
  virtual void EditUserDictionary();
  virtual void EditKeymap();
  virtual void EditRomanTable();
  virtual void ResetToDefaults();
  virtual void SelectKeymapSetting(int index);
  virtual void SelectInputModeSetting(int index);
  virtual void LaunchAdministrationDialog();

 protected:
  bool eventFilter(QObject *obj, QEvent *event);

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
  scoped_ptr<client::Session> client_;
  string custom_keymap_table_;
  string custom_roman_table_;
  int initial_preedit_method_;
};
}  // namespace gui
}  // namespace mozc
#endif  // MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_H_

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

#ifndef MOZC_GUI_POST_INSTALL_DIALOG_POST_INSTALL_DIALOG_H_
#define MOZC_GUI_POST_INSTALL_DIALOG_POST_INSTALL_DIALOG_H_

#include <memory>

#include <QWidget>

#include "gui/base/setup_util.h"
#include "gui/post_install_dialog/ui_post_install_dialog.h"

namespace mozc {
namespace gui {

// Shows additional information to the user after installation.
// This dialog also set this IME as the default IME if it is closed
// with the check box marekd.
class PostInstallDialog : public QDialog, private Ui::PostInstallDialog {
  Q_OBJECT;

 public:
  PostInstallDialog();

 protected slots:
  virtual void OnOk();
  virtual void OnsetAsDefaultCheckBoxToggled(int state);
  void reject() override;

 private:
  // - Sets this IME as the default IME if the check box on the
  //   dialog is marked.
  // - Imports MS-IME's user dictionary to Mozc' dictionary if
  //   the checkbox on the dialog is marked.
  void ApplySettings();

  std::unique_ptr<SetupUtil> setuputil_;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_POST_INSTALL_DIALOG_POST_INSTALL_DIALOG_H_

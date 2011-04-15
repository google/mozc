// Copyright 2010-2011, Google Inc.
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

// The main function of configure dialog for Mozc.

#include <QtCore/QStringList>
#include <QtCore/QTextCodec>
#include <QtGui/QApplication>
#include <QtGui/QDialog>
#include <QtGui/QtGui>
#include "base/base.h"
#include "base/util.h"
#include "gui/base/locale_util.h"
#include "gui/base/singleton_window_helper.h"
#include "gui/config_dialog/config_dialog.h"

int RunConfigDialog(int argc, char *argv[]) {
  Q_INIT_RESOURCE(qrc_config_dialog);
  QApplication app(argc, argv);

  string name = "config_dialog.";
  name += mozc::Util::GetDesktopNameAsString();
  mozc::gui::SingletonWindowHelper window_helper(name);
  if (window_helper.FindPreviousWindow()) {
    // already running
    window_helper.ActivatePreviousWindow();
    return -1;
  }

  QStringList resource_names;
  resource_names << "config_dialog" << "keymap";
  mozc::gui::LocaleUtil::InstallTranslationMessagesAndFont(resource_names);
  mozc::gui::ConfigDialog mozc_config;

  mozc_config.show();
  mozc_config.raise();
  return app.exec();
}

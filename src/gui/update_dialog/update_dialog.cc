// Copyright 2010-2014, Google Inc.
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

#include "gui/update_dialog/update_dialog.h"

#ifdef OS_WIN
# include <windows.h>
#endif  // OS_WIN

#include <QtCore/QTimer>
#include <QtGui/QtGui>
#include <string>
#include <vector>
#include <stdlib.h>

#include "base/number_util.h"
#include "base/update_checker.h"

namespace mozc {
namespace gui {

namespace {
#ifdef USE_UPDATE_CHECKER
const UINT kUpdateMonitorMessage = WM_USER + 1;
#endif  // USE_UPDATE_CHECKER
}  // anonymous namespace

UpdateDialog::UpdateDialog() : new_version_found_(false) {
  setupUi(this);
  setWindowFlags(Qt::WindowSystemMenuHint);
  setWindowModality(Qt::NonModal);

#ifdef USE_UPDATE_CHECKER
  UpdateInvoker::CallbackInfo info;
  info.mesage_receiver_window = winId();
  info.mesage_id = kUpdateMonitorMessage;
  UpdateInvoker::BeginUpdate(info);
#endif  // USE_UPDATE_CHECKER
}

UpdateDialog::~UpdateDialog() {}

#ifdef USE_UPDATE_CHECKER
bool UpdateDialog::winEvent(MSG *message, long *result) {

  if(message == NULL) {
    return QWidget::winEvent(message, result);
  }

  if (message->message == kUpdateMonitorMessage) {
    QString label_text;
    switch (message->wParam) {
      case UpdateInvoker::ON_SHOW:
        label_text = tr("Starting.");
        break;
      case UpdateInvoker::ON_CHECKING_FOR_UPDATE:
        label_text = tr("Start checking update.");
        break;
      case UpdateInvoker::ON_UPDATE_AVAILABLE:
        new_version_found_ = true;
        label_text = tr("New Version found.");
        break;
      case UpdateInvoker::ON_WAITING_TO_DOWNLOAD:
        label_text = tr("Waiting for download.");
        break;
      case UpdateInvoker::ON_DOWNLOADING:
        label_text = tr("Downloading.");
        label_text += " ";
        label_text +=
            NumberUtil::SimpleItoa(static_cast<int>(message->lParam)).c_str();
        label_text += "%";
        break;
      case UpdateInvoker::ON_WAITING_TO_INSTALL:
        label_text = tr("Waiting for install.");
        break;
      case UpdateInvoker::ON_INSTALLING:
        label_text = tr("Installing.");
        break;
      case UpdateInvoker::ON_PAUSE:
        label_text = tr("Pausing.");
        break;
      case UpdateInvoker::ON_COMPLETE:
        if (message->lParam == UpdateInvoker::JOB_FAILED) {
          if (new_version_found_) {
            label_text = tr("Installation failed.");
          } else {
            label_text = tr("Job finished without any update.");
          }
        } else if (message->lParam == UpdateInvoker::JOB_SUCCEEDED) {
          if (new_version_found_) {
            label_text = tr("New version installed.");
          } else {
            label_text = tr("You are using the latest version.");
          }
        } else {
          label_text = tr("Unexpected error occured.");
        }
        break;
      default:
        label_text = tr("Unexpected error occured.");
        break;
    }
    message_label->setText(label_text);
    *result = 0;
    return true;
  }

  return QWidget::winEvent(message, result);
}
#endif  // USE_UPDATE_CHECKER
}  // namespace gui
}  // namespace mozc

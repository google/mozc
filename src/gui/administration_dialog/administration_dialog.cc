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

#include "gui/administration_dialog/administration_dialog.h"

#include <QtGui/QMessageBox>
#include "base/base.h"
#include "base/run_level.h"
#include "base/system_util.h"
#include "config/stats_config_util.h"
#include "server/cache_service_manager.h"

namespace mozc {
namespace gui {

using mozc::config::StatsConfigUtil;

AdministrationDialog::AdministrationDialog() {
  setupUi(this);
  setWindowFlags(Qt::WindowSystemMenuHint |
                 Qt::MSWindowsFixedSizeDialogHint |
                 Qt::WindowStaysOnTopHint);
  setWindowModality(Qt::NonModal);

  // signal/slot
  connect(AdministrationDialogbuttonBox,
          SIGNAL(clicked(QAbstractButton *)),
          this,
          SLOT(clicked(QAbstractButton *)));

  // When clicking these messages, CheckBoxs corresponding
  // to them should be toggled.
  // We cannot use connect/slot as QLabel doesn't define
  // clicked slot by default.
  usageStatsMessage->installEventFilter(this);

#ifdef OS_WIN

#ifdef CHANNEL_DEV
  usageStatsCheckBox->setEnabled(false);
#endif

  usageStatsCheckBox->setChecked(StatsConfigUtil::IsEnabled());

  if (SystemUtil::IsVistaOrLater()) {
    ElevatedProcessDisabledcheckBox->setChecked
        (RunLevel::GetElevatedProcessDisabled());
  } else {
    ElevatedProcessDisabledcheckBox->setVisible(false);
  }

  CacheServiceEnabledcheckBox->setChecked(
      CacheServiceManager::IsEnabled() || CacheServiceManager::IsRunning());
#endif
}

AdministrationDialog::~AdministrationDialog() {}

bool AdministrationDialog::CanStartService() {
#ifdef OS_WIN
  if (!CacheServiceEnabledcheckBox->isChecked()) {
    return true;
  }

  if (!CacheServiceManager::HasEnoughMemory()) {
    QMessageBox::critical(
        this,
        tr("Mozc administration settings"),
        tr("This computer does not have enough memory to load "
           "dictionary into physical memory."));
    return false;
  }
#endif  // OS_WIN

  return true;
}

void AdministrationDialog::clicked(QAbstractButton *button) {
#ifdef OS_WIN
  switch (AdministrationDialogbuttonBox->buttonRole(button)) {
    case QDialogButtonBox::ApplyRole:
    case QDialogButtonBox::AcceptRole:
      if (!StatsConfigUtil::SetEnabled(usageStatsCheckBox->isChecked())) {
        QMessageBox::critical(
            this,
            tr("Mozc administration settings"),
            tr("Failed to change the configuration of "
               "usage statistics and crash report. "
               "Administrator privilege is required to change the "
               "configuration."));
      }

      if (CanStartService()) {
        bool result = false;
        if (CacheServiceEnabledcheckBox->isChecked()) {
          result = CacheServiceManager::EnableAutostart();
        } else {
          result = CacheServiceManager::DisableService();
        }
        if (!result) {
          QMessageBox::critical(
              this,
              tr("Mozc administration settings"),
              tr("Failed to change the configuration of on-memory dictionary. "
                 "Administrator privilege is required to change the "
                 "configuration."));
        }
      }

      if (ElevatedProcessDisabledcheckBox->isVisible() &&
          RunLevel::GetElevatedProcessDisabled()
          != ElevatedProcessDisabledcheckBox->isChecked()) {
        if (!RunLevel::SetElevatedProcessDisabled
            (ElevatedProcessDisabledcheckBox->isChecked())) {
          QMessageBox::critical(
              this,
              tr("Mozc administration settings"),
              tr("Failed to save the UAC policy setting. "
                 "Administrator privilege is required to "
                 "change UAC settings."));
        }
      }
      QWidget::close();
      break;
    case QDialogButtonBox::RejectRole:
      QWidget::close();
      break;
    default:
      break;
  }
#endif  // OS_WIN
}

// Catch MouseButtonRelease event to toggle the CheckBoxes
bool AdministrationDialog::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::MouseButtonRelease) {
    if (obj == usageStatsMessage) {
#ifndef CHANNEL_DEV
      usageStatsCheckBox->toggle();
#endif
    }
  }
  return QObject::eventFilter(obj, event);
}
}  // namespace gui
}  // namespace mozc

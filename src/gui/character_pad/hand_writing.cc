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

#include "gui/character_pad/hand_writing.h"

#ifdef ENABLE_CLOUD_HANDWRITING
#include <QtGui/QApplication>
#endif  // ENABLE_CLOUD_HANDWRITING
#include <QtGui/QtGui>
#include <QtGui/QMessageBox>

#ifdef OS_WINDOWS
#include <windows.h>
#include <windowsx.h>
#endif

#ifdef ENABLE_CLOUD_HANDWRITING
#include "client/client.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#endif  // ENABLE_CLOUD_HANDWRITING

#include "handwriting/handwriting_manager.h"
#include "gui/base/win_util.h"

namespace mozc {

namespace {
enum HandwritingSourceId {
  kZinniaHandwriting = 0,
#ifdef ENABLE_CLOUD_HANDWRITING
  kCloudHandwriting = 1,
#endif  // ENABLE_CLOUD_HANDWRITING
};

#ifdef ENABLE_CLOUD_HANDWRITING
bool SetConfig(client::ClientInterface *client,
               const config::Config &config) {
  if (!client->CheckVersionOrRestartServer()) {
    LOG(ERROR) << "CheckVersionOrRestartServer failed";
    return false;
  }

  if (!client->SetConfig(config)) {
    LOG(ERROR) << "SetConfig failed";
    return false;
  }

  return true;
}

bool GetConfig(client::ClientInterface *client,
               config::Config *config) {
  if (!client->CheckVersionOrRestartServer()) {
    LOG(ERROR) << "CheckVersionOrRestartServer failed";
    return false;
  }

  if (!client->GetConfig(config)) {
    LOG(ERROR) << "GetConfig failed";
    return false;
  }

  return true;
}

bool IsCloudHandwritingAllowed(client::ClientInterface *client) {
  config::Config config;
  if (!GetConfig(client, &config)) {
    return false;
  }

  // Note that |allow_cloud_handwriting| has default value 'false'.
  return config.allow_cloud_handwriting();
}
#endif  // ENABLE_CLOUD_HANDWRITING

}  // namespace

namespace gui {

HandWriting::HandWriting(QWidget *parent)
    : QMainWindow(parent) {
#ifdef ENABLE_CLOUD_HANDWRITING
    client_.reset(client::ClientFactory::NewClient());
#endif  // ENABLE_CLOUD_HANDWRITING

  setupUi(this);

  handWritingCanvas->setListWidget(resultListWidget);

  fontComboBox->setWritingSystem
      (static_cast<QFontDatabase::WritingSystem>
       (QFontDatabase::Any));
  fontComboBox->setEditable(false);
  fontComboBox->setCurrentFont(resultListWidget->font());

  QObject::connect(fontComboBox,
                   SIGNAL(currentFontChanged(const QFont &)),
                   this, SLOT(updateFont(const QFont &)));

  QObject::connect(sizeComboBox,
                   SIGNAL(currentIndexChanged(int)),
                   this, SLOT(updateFontSize(int)));

#ifdef ENABLE_CLOUD_HANDWRITING
  QObject::connect(handwritingSourceComboBox,
                   SIGNAL(currentIndexChanged(int)),
                   this, SLOT(tryToUpdateHandwritingSource(int)));
#else
  // When cloud handwriting is configured to be disabled, hide the combo box.
  handwritingSourceComboBox->setVisible(false);
#endif  // ENABLE_CLOUD_HANDWRITING

  QObject::connect(clearButton,
                   SIGNAL(clicked()),
                   this, SLOT(clear()));

  QObject::connect(revertButton,
                   SIGNAL(clicked()),
                   this, SLOT(revert()));

  QObject::connect(handWritingCanvas,
                   SIGNAL(canvasUpdated()),
                   this, SLOT(updateUIStatus()));

  // "4" means smallest.
  sizeComboBox->setCurrentIndex(4);
  fontComboBox->setCurrentFont(resultListWidget->font());

  int default_handwriting_method = kZinniaHandwriting;
#ifdef ENABLE_CLOUD_HANDWRITING
  if (IsCloudHandwritingAllowed(client_.get())) {
    // If cloud handwriting is enabled, use it by default.
    // TODO(team): Consider the case where network access is not available.
    default_handwriting_method = kCloudHandwriting;
  }
#endif  // ENABLE_CLOUD_HANDWRITING
  handwritingSourceComboBox->setCurrentIndex(default_handwriting_method);

  updateUIStatus();
  repaint();
  update();
}

HandWriting::~HandWriting() {}

void HandWriting::updateFont(const QFont &font) {
  resultListWidget->updateFont(font);
}

void HandWriting::updateFontSize(int index) {
  resultListWidget->updateFontSize(index);
}

void HandWriting::tryToUpdateHandwritingSource(int index) {
  switch (index) {
    case kZinniaHandwriting:
      updateHandwritingSource(kZinniaHandwriting);
      break;
#ifdef ENABLE_CLOUD_HANDWRITING
    case kCloudHandwriting:
      if (TryToEnableCloudHandwriting()) {
        updateHandwritingSource(kCloudHandwriting);
      } else {
        // When user refused to use cloud handwriting, change the
        // combobox to Zinnia.
        handwritingSourceComboBox->setCurrentIndex(kZinniaHandwriting);
        updateHandwritingSource(kZinniaHandwriting);
      }
      break;
#endif
    default:
      DLOG(INFO) << "Unknown index = " << index;
      break;
  }
}

void HandWriting::updateHandwritingSource(int index) {
  mozc::handwriting::HandwritingManager::ClearHandwritingModules();
  switch (index) {
    case kZinniaHandwriting:
      mozc::handwriting::HandwritingManager::AddHandwritingModule(
          &zinnia_handwriting_);
      break;
#ifdef ENABLE_CLOUD_HANDWRITING
    case kCloudHandwriting:
      mozc::handwriting::HandwritingManager::AddHandwritingModule(
          &cloud_handwriting_);
      break;
#endif  // ENABLE_CLOUD_HANDWRITING
    default:
      DLOG(INFO) << "Unknown index = " << index;
      break;
  }
  resultListWidget->clear();
  handWritingCanvas->restartRecognition();
}

void HandWriting::resizeEvent(QResizeEvent *event) {
  resultListWidget->update();
}

void HandWriting::clear() {
  resultListWidget->clear();
  handWritingCanvas->clear();
  updateUIStatus();
}

void HandWriting::revert() {
  resultListWidget->clear();
  handWritingCanvas->revert();
  updateUIStatus();
}

void HandWriting::updateUIStatus() {
#ifdef OS_MACOSX
  // Due to a bug of Qt?, the appearance of these buttons
  // doesn't change on Mac. To fix this issue, always set
  // true on Mac.
  clearButton->setEnabled(true);
  revertButton->setEnabled(true);
#else
  const bool enabled = handWritingCanvas->strokes_size() > 0;
  clearButton->setEnabled(enabled);
  revertButton->setEnabled(enabled);
#endif
}

#ifdef OS_WINDOWS
bool HandWriting::winEvent(MSG *message, long *result) {
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

#ifdef ENABLE_CLOUD_HANDWRITING
bool HandWriting::TryToEnableCloudHandwriting() {
  if (IsCloudHandwritingAllowed(client_.get())) {
    // Already allowed. Do nothing.
    return true;
  }

  // Currently custom style sheet is used only on Windows.
#ifdef OS_WINDOWS
  // When a custom style sheet is applied, temporarily disable it to
  // show a message box with default theme. See b/5949615.
  // Mysteriously, a message box launched from dictionary tool does
  // not have this issue even when a custom style sheet is applied.
  // This implies that we might be able to fix this issue in a more
  // appropriate way.
  // TODO(yukawa): Investigate why this does not happen on the
  //     dictionary tool and remove this workaround code if possible.
  //     See b/5974593.
  const QString custom_style_sheet = qApp->styleSheet();
  if (!custom_style_sheet.isEmpty()) {
    qApp->setStyleSheet("");
  }
#endif  // OS_WINDOWS

  // When cloud handwriting is not allowed, ask the user to enable it.
  const QMessageBox::StandardButton result =
      QMessageBox::question(
          this,
          tr("Cloud handwriting recognition"),
          // TODO(yukawa, peria): Update the warning message and have
          //     native check. b/5943541.
          tr("This feature improve the accuracy of handwriting recognition "
             "by using a Google web service. To do so, your handwriting "
             "strokes will be securely sent to Google. Do you want to use "
             "Cloud handwriting?"),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  // Currently custom style sheet is used only on Windows.
#ifdef OS_WINDOWS
  // Restore the custom style sheet if necessary.
  if (!custom_style_sheet.isEmpty()) {
    qApp->setStyleSheet(custom_style_sheet);
  }
#endif  // OS_WINDOWS

  if (result == QMessageBox::No) {
    // User refused.
    return false;
  }

  // The user allowed to enable the cloud handwriting. Store this info
  // for later use.
  config::Config config;
  if (GetConfig(client_.get(), &config)) {
    config.set_allow_cloud_handwriting(true);
    SetConfig(client_.get(), config);
  }

  return true;
}
#endif  // ENABLE_CLOUD_HANDWRITING

}  // namespace gui
}  // namespace mozc

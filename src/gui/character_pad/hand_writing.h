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

#ifndef MOZC_GUI_CHARACTER_PAD_HAND_WRITING_H_
#define MOZC_GUI_CHARACTER_PAD_HAND_WRITING_H_

#include <QtGui/QMainWindow>
// Do not depend on CloudHandwriting class to keep dependencies
// minimum.
// TODO(yukawa): Remove this #ifdef when CloudHandwriting class
//     never depends on unnecessary libraries when
//     ENABLE_CLOUD_HANDWRITING macro is not defined.
#ifdef ENABLE_CLOUD_HANDWRITING
#include "handwriting/cloud_handwriting.h"
#endif  // ENABLE_CLOUD_HANDWRITING
#include "handwriting/zinnia_handwriting.h"
#include "gui/character_pad/ui_hand_writing.h"

namespace mozc {
namespace client {
class ClientInterface;
}

namespace gui {
class HandWriting : public QMainWindow,
                    private Ui::HandWriting {
  Q_OBJECT;

 public:
  explicit HandWriting(QWidget *parent = NULL);
  virtual ~HandWriting();

 public slots:
  void updateFontSize(int index);
  void updateFont(const QFont &font);
  void clear();
  void revert();
  void updateUIStatus();
  void tryToUpdateHandwritingSource(int index);
  void itemSelected(const QListWidgetItem *item);

 protected:
  void resizeEvent(QResizeEvent *event);

#ifdef OS_WINDOWS
  bool winEvent(MSG *message, long *result);
#endif  // OS_WINDOWS

  void updateHandwritingSource(int index);

  scoped_ptr<client::ClientInterface> client_;
  bool usage_stats_enabled_;
// Do not depend on CloudHandwriting class to keep dependencies
// minimum.
// TODO(yukawa): Remove this #ifdef when CloudHandwriting class
//     never depends on unnecessary libraries when
//     ENABLE_CLOUD_HANDWRITING macro is not defined.
#ifdef ENABLE_CLOUD_HANDWRITING
  // Returns true if the user allowed to enable cloud handwriting feature.
  // This function asks to the user to enable cloud handwriting if
  // necessary and updates the current config as
  // |config.set_allow_cloud_handwriting(true)| when it is allowed.
  bool TryToEnableCloudHandwriting();
  mozc::handwriting::CloudHandwriting cloud_handwriting_;
#endif  // ENABLE_CLOUD_HANDWRITING
  mozc::handwriting::ZinniaHandwriting zinnia_handwriting_;
};
}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_PAD_HAND_WRITING_WIDGET_H_

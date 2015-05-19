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

// HandWritingThread is an independent thread for handwriting
// recognizer not to block the UI during the recognition computations.
// We intentionally use QThread and not use Mozc's base/thread because
// this is a part of eventloop/handling of Qt GUI.
// All slot/signal handling in Qt >4 over threads are actually queued
// to eventloop on the target thread, so it's safe.

#ifndef MOZC_GUI_CHARACTER_PAD_HAND_WRITING_THREAD_H_
#define MOZC_GUI_CHARACTER_PAD_HAND_WRITING_THREAD_H_

#include "handwriting/handwriting_manager.h"

#include <QtCore/QMutex>
#include <QtCore/QThread>

class QListWidgetItem;

namespace mozc {
namespace gui {

class HandWritingThread : public QThread {
  Q_OBJECT;

 public:
  void Start();

  // Set the strokes to be used in the recognition.  This will make
  // lock on strokes_ so thread-safe.
  void SetStrokes(const handwriting::Strokes &strokes);
  // Copy internal candidates_ into the candidates.  This will make
  // lock on candidates_ so thead-safe.
  void GetCandidates(vector<string> *candidates);

 public slots:
  // This slot invokes the recognition and emit candidatesUpdated().
  void startRecognition();
  void itemSelected(const QListWidgetItem *item);

 signals:
  void candidatesUpdated();
  void statusUpdated(mozc::handwriting::HandwritingStatus status);

 private:
  handwriting::Strokes strokes_;
  vector<string> candidates_;

  uint64 strokes_sec_;
  uint32 strokes_usec_;
  uint64 last_requested_sec_;
  uint32 last_requested_usec_;

  QMutex strokes_mutex_;
  QMutex candidates_mutex_;

  bool usage_stats_enabled_;
};
}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_PAD_HAND_WRITING_THREAD_H_

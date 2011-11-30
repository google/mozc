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

#include "gui/character_pad/hand_writing_thread.h"

// MutexLocker locks in the constructor and unlocks in the destructor.
#include <QtCore/QMutexLocker>

namespace {
// Thread-safe copying of strokes.
void CopyStrokes(const mozc::handwriting::Strokes &source,
                 mozc::handwriting::Strokes *target,
                 QMutex *mutex) {
  DCHECK(target);
  DCHECK(mutex);
  QMutexLocker l(mutex);
  target->clear();
  target->resize(source.size());
  for (size_t i = 0; i < source.size(); ++i) {
    const mozc::handwriting::Stroke &stroke = source[i];
    (*target)[i].resize(stroke.size());
    for (size_t j = 0; j < stroke.size(); ++j) {
      (*target)[i][j] = stroke[j];
    }
  }
}

// Thread-safe copying of candidates.
void CopyCandidates(
    const vector<string> &source, vector<string> *target, QMutex *mutex) {
  DCHECK(target);
  DCHECK(mutex);
  QMutexLocker l(mutex);
  target->clear();
  target->resize(source.size());
  for (size_t i = 0; i < source.size(); ++i) {
    (*target)[i] = source[i];
  }
}
}  // namespace

namespace mozc {
namespace gui {

void HandWritingThread::Start() {
  start();
  moveToThread(this);
}

void HandWritingThread::SetStrokes(const handwriting::Strokes &strokes) {
  CopyStrokes(strokes, &strokes_, &strokes_mutex_);
}

void HandWritingThread::GetCandidates(vector<string> *candidates) {
  CopyCandidates(candidates_, candidates, &candidates_mutex_);
}

void HandWritingThread::startRecognition() {
  handwriting::Strokes strokes;
  CopyStrokes(strokes_, &strokes, &strokes_mutex_);
  if (strokes.empty()) {
    return;
  }

  vector<string> candidates;
  handwriting::HandwritingManager::Recognize(strokes, &candidates);
  CopyCandidates(candidates, &candidates_, &candidates_mutex_);
  emit candidatesUpdated();
}

}  // namespace gui
}  // namespace mozc

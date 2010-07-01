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

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtGui/QApplication>
#include <QtGui/QWidget>

#include "base/base.h"
#include "base/singleton.h"
#include "base/version.h"
#include "gui/base/window_title_modifier.h"

namespace mozc {
namespace gui {
bool WindowTitleModifier::eventFilter(QObject *obj,
                                      QEvent *event) {
  QWidget *w = qApp->activeWindow();
  if (w != NULL && obj != NULL && w == obj &&
      QEvent::WindowActivate == event->type() &&
      w->windowTitle().indexOf(prefix_) == -1) {
    w->setWindowTitle(w->windowTitle() +
                      prefix_ +
                      Version::GetMozcVersion().c_str() +
                      suffix_);
  }

  return QObject::eventFilter(obj, event);
}

WindowTitleModifier::WindowTitleModifier()
    : prefix_(" (Dev "), suffix_(")") {}

WindowTitleModifier::~WindowTitleModifier() {}

}  // namespace gui
}  // namespace mozc

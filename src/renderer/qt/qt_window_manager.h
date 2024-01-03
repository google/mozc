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

#ifndef MOZC_RENDERER_QT_QT_WINDOW_MANAGER_H_
#define MOZC_RENDERER_QT_QT_WINDOW_MANAGER_H_

#include <QtWidgets>
#include <functional>
#include <memory>
#include <string>

#include "base/coordinates.h"
#include "base/port.h"
#include "client/client_interface.h"
#include "protocol/renderer_command.pb.h"
#include "protocol/renderer_style.pb.h"

namespace mozc {
namespace renderer {

class QtWindowManager {
 public:
  QtWindowManager();
  QtWindowManager(const QtWindowManager &) = delete;
  QtWindowManager &operator=(const QtWindowManager &) = delete;
  ~QtWindowManager() = default;

  void Initialize();

  void HideAllWindows();
  void ShowAllWindows();
  void UpdateLayout(const commands::RendererCommand &command);
  bool Activate();
  bool IsAvailable() const;
  bool ExecCommand(const commands::RendererCommand &command);
  bool SetSendCommandInterface(
      client::SendCommandInterface *send_command_interface);
  void SetWindowPos(int x, int y);

 protected:
  // If this function returns true, we should show/reload candidate window.
  bool ShouldShowCandidateWindow(const commands::RendererCommand &command);

  // Judges whether infolist should be shown or not.
  bool ShouldShowInfolistWindow(const commands::RendererCommand &command);

  // Updates candidate window size and location based on given command, and
  // returns actual rectangle.
  Rect UpdateCandidateWindow(const commands::RendererCommand &command);

  // Updates infolist window size and location based on given command and
  // candidate window rectangle.
  void UpdateInfolistWindow(const commands::RendererCommand &command,
                            const Rect &candidate_window_rect);

  // Returns monitor rectangle for the specified point.
  Rect GetMonitorRect(int x, int y);

  Point GetWindowPosition(const commands::RendererCommand &command,
                          const Size &win_size);

  void OnClicked(int row, int column);

 private:
  QTableWidget *candidates_ = nullptr;
  QTableWidget *infolist_ = nullptr;

  RendererStyle style_;
  commands::RendererCommand prev_command_;
  client::SendCommandInterface *send_command_interface_ = nullptr;
};

}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_QT_QT_WINDOW_MANAGER_H_

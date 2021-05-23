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

#include <memory>
#include <string>

#include <QtWidgets/QtWidgets>

#include "base/coordinates.h"
#include "base/port.h"
#include "client/client_interface.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/qt/qt_window_manager_interface.h"

namespace mozc {
namespace renderer {

class QtWindowManager : public QtWindowManagerInterface {
 public:
  // WindowManager takes arguments' ownership
  explicit QtWindowManager() = default;
  ~QtWindowManager() override = default;

  int StartRendererLoop(int argc, char **argv) override;

  void Initialize() override;
  void HideAllWindows() override;
  void ShowAllWindows() override;
  void UpdateLayout(const commands::RendererCommand &command) override;
  bool Activate() override;
  bool IsAvailable() const override;
  bool SetSendCommandInterface(
      client::SendCommandInterface *send_command_interface) override;
  void SetWindowPos(int x, int y) override;

 protected:
  // If this function returns true, we should show/reload candidate window.
  bool ShouldShowCandidateWindow(
      const commands::RendererCommand &command);

  // Judges whether infolist should be shown or not.
  bool ShouldShowInfolistWindow(
      const commands::RendererCommand &command);

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

 private:
  QWidget *window_;
  QTableWidget *candidates_;

  commands::RendererCommand prev_command_;

  DISALLOW_COPY_AND_ASSIGN(QtWindowManager);
};

}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_QT_QT_WINDOW_MANAGER_H_

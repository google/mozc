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

#ifndef MOZC_RENDERER_WIN32_WINDOW_MANAGER_H_
#define MOZC_RENDERER_WIN32_WINDOW_MANAGER_H_

#include <windows.h>

#include "base/port.h"
#include "base/scoped_ptr.h"

namespace mozc {

namespace client {
class SendCommandInterface;
}  // namespace client

namespace commands {
class RendererCommand;
}  // namespace commands

namespace renderer {
namespace win32 {

class CandidateWindow;
class CompositionWindowList;
class IndicatorWindow;
class InfolistWindow;
class LayoutManager;
class WorkingAreaInterface;

// This class is the controller of the candidate windows.
class WindowManager {
 public:
  WindowManager();
  ~WindowManager();
  void Initialize();
  void AsyncHideAllWindows();
  void AsyncQuitAllWindows();
  void DestroyAllWindows();
  void HideAllWindows();
  void UpdateLayoutIMM32(const commands::RendererCommand &command);
  void UpdateLayoutTSF(const commands::RendererCommand &command);
  bool IsAvailable() const;
  void SetSendCommandInterface(
      client::SendCommandInterface *send_command_interface);
  void PreTranslateMessage(const MSG &message);

 private:
  scoped_ptr<CandidateWindow> main_window_;
  scoped_ptr<CandidateWindow> cascading_window_;
  scoped_ptr<CompositionWindowList> composition_window_list_;
  scoped_ptr<IndicatorWindow> indicator_window_;
  scoped_ptr<InfolistWindow> infolist_window_;
  scoped_ptr<LayoutManager> layout_manager_;
  scoped_ptr<WorkingAreaInterface> working_area_;
  client::SendCommandInterface *send_command_interface_;
  POINT last_position_;
  int candidates_finger_print_;
  DWORD thread_id_;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_WINDOW_MANAGER_H_

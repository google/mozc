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

#ifndef MOZC_RENDERER_UNIX_WINDOW_MANAGER_H_
#define MOZC_RENDERER_UNIX_WINDOW_MANAGER_H_
#include <gtk/gtk.h>
#include <string>

#include "base/base.h"
#include "base/mutex.h"
#include "client/client_interface.h"
#include "renderer/renderer_command.pb.h"
#include "renderer/unix/gtk_window_interface.h"
#include "renderer/unix/gtk_wrapper_interface.h"
#include "renderer/unix/window_manager_interface.h"
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace renderer {
namespace gtk {

class WindowManager : public WindowManagerInterface {
 public:
  // WindowManager takes arguments' ownership
  explicit WindowManager(GtkWindowInterface *main_window,
                         GtkWindowInterface *infolist_window,
                         GtkWrapperInterface *gtk);
  virtual ~WindowManager();

  virtual void Initialize();
  virtual void HideAllWindows();
  virtual void ShowAllWindows();
  virtual void UpdateLayout(const commands::RendererCommand &command);
  virtual bool Activate();
  virtual bool IsAvailable() const;
  virtual bool SetSendCommandInterface(
      client::SendCommandInterface *send_command_interface);
  virtual void SetWindowPos(int x, int y);

 private:
  // If this function returns true, we should show/reload candidate window.
  static bool ShouldShowCandidateWindow(
      const commands::RendererCommand &command);

  // Judges whether infolist should be shown or not.
  static bool ShouldShowInfolistWindow(
      const commands::RendererCommand &command);

  // Updates candidate window size and location based on given command, and
  // returns actual rectangle.
  Rect UpdateCandidateWindow(const commands::RendererCommand &command);

  // Updates infolist window size and location based on given command and
  // candidate window rectangle.
  void UpdateInfolistWindow(const commands::RendererCommand &command,
                            const Rect &candidate_window_rect);

  Rect GetDesktopRect();

  FRIEND_TEST(WindowManagerTest, SetSendCommandInterfaceTest);
  FRIEND_TEST(WindowManagerTest, ShouldShowCandidateWindowTest);
  FRIEND_TEST(WindowManagerTest, ShouldShowInfolistWindowTest);
  FRIEND_TEST(WindowManagerTest, UpdateCandidateWindowTest);
  FRIEND_TEST(WindowManagerTest, UpdateInfolistWindowTest);
  FRIEND_TEST(WindowManagerTest, GetDesktopRectTest);

  scoped_ptr<GtkWindowInterface> candidate_window_;
  scoped_ptr<GtkWindowInterface> infolist_window_;
  scoped_ptr<GtkWrapperInterface> gtk_;
  client::SendCommandInterface *send_command_interface_;
  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_UNIX_WINDOW_MANAGER_H_

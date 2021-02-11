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

#ifdef OS_WIN
#include <windows.h>
#elif defined(ENABLE_GTK_RENDERER)
#include <gtk/gtk.h>
#endif  // OS_WIN, ENABLE_GTK_RENDERER

#include "base/crash_report_handler.h"
#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/run_level.h"
#include "base/system_util.h"
#include "base/util.h"
#include "config/stats_config_util.h"

#ifdef OS_WIN
#include "base/win_util.h"
#include "base/winmain.h"
#include "renderer/win32/win32_server.h"
#elif defined(__APPLE__)
#include "renderer/mac/CandidateController.h"
#include "renderer/mac/mac_server.h"
#include "renderer/mac/mac_server_send_command.h"
#elif defined(ENABLE_GTK_RENDERER)
#include "renderer/renderer_client.h"
#include "renderer/table_layout.h"
#include "renderer/unix/cairo_factory.h"
#include "renderer/unix/candidate_window.h"
#include "renderer/unix/draw_tool.h"
#include "renderer/unix/font_spec.h"
#include "renderer/unix/gtk_wrapper.h"
#include "renderer/unix/infolist_window.h"
#include "renderer/unix/text_renderer.h"
#include "renderer/unix/unix_renderer.h"
#include "renderer/unix/unix_server.h"
#include "renderer/unix/window_manager.h"
#endif  // OS_WIN, __APPLE__, ENABLE_GTK_RENDERER

MOZC_DECLARE_FLAG(bool, restricted);

int main(int argc, char *argv[]) {
  const mozc::RunLevel::RunLevelType run_level =
      mozc::RunLevel::GetRunLevel(mozc::RunLevel::RENDERER);

  if (run_level >= mozc::RunLevel::DENY) {
    return -1;
  }

#ifdef OS_WIN
  mozc::ScopedCOMInitializer com_initializer;
#elif defined(ENABLE_GTK_RENDERER)
  gtk_set_locale();
#if !GLIB_CHECK_VERSION(2, 31, 0)
  // There are not g_thread_init function in glib>=2.31.0.
  // http://developer.gnome.org/glib/2.31/glib-Deprecated-Thread-APIs.html#g-thread-init
  g_thread_init(nullptr);
#endif  // GLIB>=2.31.0
  gdk_threads_init();
  gtk_init(&argc, &argv);
#endif  // OS_WIN, ENABLE_GTK_RENDERER

  mozc::SystemUtil::DisableIME();

  // restricted mode
  if (run_level == mozc::RunLevel::RESTRICTED) {
    mozc::SetFlag(&FLAGS_restricted, true);
  }

  if (mozc::config::StatsConfigUtil::IsEnabled()) {
    mozc::CrashReportHandler::Initialize(false);
  }
  mozc::InitMozc(argv[0], &argc, &argv);

  int result_code = 0;

  {
#ifdef OS_WIN
    mozc::renderer::win32::Win32Server server;
    server.SetRendererInterface(&server);
    result_code = server.StartServer();
#elif defined(__APPLE__)
    mozc::renderer::mac::MacServer::Init();
    mozc::renderer::mac::MacServer server(argc, (const char **)argv);
    mozc::renderer::mac::CandidateController renderer;
    mozc::renderer::mac::MacServerSendCommand send_command;
    server.SetRendererInterface(&renderer);
    renderer.SetSendCommandInterface(&send_command);
    result_code = server.StartServer();
#elif defined(ENABLE_GTK_RENDERER)
    mozc::renderer::gtk::UnixRenderer renderer(
        new mozc::renderer::gtk::WindowManager(
            new mozc::renderer::gtk::CandidateWindow(
                new mozc::renderer::TableLayout(),
                new mozc::renderer::gtk::TextRenderer(
                    new mozc::renderer::gtk::FontSpec(
                        new mozc::renderer::gtk::GtkWrapper())),
                new mozc::renderer::gtk::DrawTool(),
                new mozc::renderer::gtk::GtkWrapper(),
                new mozc::renderer::gtk::CairoFactory()),
            new mozc::renderer::gtk::InfolistWindow(
                new mozc::renderer::gtk::TextRenderer(
                    new mozc::renderer::gtk::FontSpec(
                        new mozc::renderer::gtk::GtkWrapper())),
                new mozc::renderer::gtk::DrawTool(),
                new mozc::renderer::gtk::GtkWrapper(),
                new mozc::renderer::gtk::CairoFactory()),
            new mozc::renderer::gtk::GtkWrapper()));
    mozc::renderer::gtk::UnixServer server(
        new mozc::renderer::gtk::GtkWrapper());
    server.OpenPipe();
    renderer.Initialize();
    server.SetRendererInterface(&renderer);
    result_code = server.StartServer();
#endif  // OS_WIN, __APPLE__, ENABLE_GTK_RENDERER
  }

  return result_code;
}

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

#include "renderer/qt/qt_server.h"

#if defined(__linux__) && !defined(__ANDROID__)
#include <stdlib.h>
#endif  // __linux__ && !__ANDROID__

#include <QApplication>
#include <QMetaType>
#include <algorithm>
#include <cstdint>
#include <string>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "base/system_util.h"
#include "base/vlog.h"
#include "ipc/named_event.h"
#include "protocol/config.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/qt/qt_ipc_thread.h"

#ifndef NDEBUG
#include "config/config_handler.h"
#endif  // NDEBUG

// By default, mozc_renderer quits when user-input continues to be
// idle for 10min.
ABSL_FLAG(int32_t, timeout, 10 * 60, "timeout of candidate server (sec)");
ABSL_FLAG(bool, restricted, false,
          "launch candidates server with restricted mode");

Q_DECLARE_METATYPE(std::string);

namespace mozc {
namespace renderer {

namespace {
constexpr char kServiceName[] = "renderer";

std::string GetServiceName() {
  std::string name = kServiceName;
  const std::string desktop_name = SystemUtil::GetDesktopNameAsString();
  if (!desktop_name.empty()) {
    name += ".";
    name += desktop_name;
  }
  return name;
}
}  // namespace

QtServer::QtServer() : timeout_(0) {
  if (absl::GetFlag(FLAGS_restricted)) {
    absl::SetFlag(&FLAGS_timeout,
                  // set 60 sec with restricted mode
                  std::min(absl::GetFlag(FLAGS_timeout), 60));
  }

  timeout_ = 1000 * std::clamp(absl::GetFlag(FLAGS_timeout), 3, 24 * 60 * 60);
  MOZC_VLOG(2) << "timeout is set to be : " << timeout_;

#ifndef NDEBUG
  mozc::internal::SetConfigVLogLevel(
      config::ConfigHandler::GetSharedConfig()->verbose_level());
#endif  // NDEBUG
}

QtServer::~QtServer() = default;

void QtServer::AsyncExecCommand(const std::string &command) {
  emit EmitUpdated(command);
}

void QtServer::Update(std::string command) {
  commands::RendererCommand protocol;
  if (!protocol.ParseFromString(command)) {
    LOG(WARNING) << "Parse From String Failed";
    return;
  }
  ExecCommandInternal(protocol);
}

int QtServer::StartServer(int argc, char **argv) {
#if defined(__linux__) && !defined(__ANDROID__)
  // |QWidget::move()| never works with wayland platform backend. Always use
  // 'xcb' platform backend.  https://github.com/google/mozc/issues/794
  ::setenv("QT_QPA_PLATFORM", "xcb", 1);
#endif  // __linux__ && !__ANDROID__

  qRegisterMetaType<std::string>("std::string");
  QApplication app(argc, argv);

  // send "ready" event to the client
  const std::string name = GetServiceName();
  NamedEventNotifier notifier(name.c_str());
  notifier.Notify();

  renderer_.Initialize();
  connect(&ipc_thread_, &QtIpcThread::EmitUpdated, this, &QtServer::Update);
  ipc_thread_.start();
  return app.exec();
}

bool QtServer::ExecCommandInternal(const commands::RendererCommand &command) {
  MOZC_VLOG(2) << command;

  return renderer_.ExecCommand(command);
}

uint32_t QtServer::timeout() const { return timeout_; }

}  // namespace renderer
}  // namespace mozc

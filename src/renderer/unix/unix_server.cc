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

#include "renderer/unix/unix_server.h"

#include <fcntl.h>
#include <unistd.h>

#include <memory>

#include "base/logging.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/unix/window_manager.h"

using std::unique_ptr;

namespace mozc {
namespace renderer {
namespace gtk {
namespace {

gboolean mozc_prepare(GSource *source, int *timeout) {
  *timeout = -1;
  return FALSE;
}

gboolean mozc_check(GSource *source) {
  UnixServer::MozcWatchSource *watch =
      reinterpret_cast<UnixServer::MozcWatchSource *>(source);
  return watch->poll_fd.revents != 0;
}

gboolean mozc_dispatch(GSource *source, GSourceFunc callback,
                       gpointer user_data) {
  UnixServer::MozcWatchSource *watch =
      reinterpret_cast<UnixServer::MozcWatchSource *>(source);
  char buf[8];
  // Discards read data.
  while (read(watch->poll_fd.fd, buf, 8) > 0) {
  }
  watch->unix_server->Render();
  return TRUE;
}
}  // namespace

UnixServer::UnixServer(GtkWrapperInterface *gtk) : gtk_(gtk) {}

UnixServer::~UnixServer() {}

void UnixServer::AsyncHide() {
  // obsolete callback
}

void UnixServer::AsyncQuit() { gtk_->GtkMainQuit(); }

bool UnixServer::Render() {
  std::string message;
  {
    scoped_lock l(&mutex_);
    message.assign(message_);
  }

  commands::RendererCommand command;
  if (command.ParseFromString(message)) {
    ExecCommandInternal(command);
  } else {
    LOG(WARNING) << "Parse From String Failed";
  }
  return true;
}

bool UnixServer::AsyncExecCommand(std::string *proto_message) {
  {
    // Take the ownership of |proto_message|.
    unique_ptr<std::string> proto_message_owner(proto_message);
    scoped_lock l(&mutex_);
    if (message_ == *proto_message_owner.get()) {
      // This is exactly the same to the previous message. Theoretically it is
      // safe to do nothing here.
      return true;
    }
    // Since mozc rendering protocol is state-less, we can always ignore the
    // previous content of |message_|.
    message_.swap(*proto_message_owner.get());
  }

  const char dummy_data = 0;
  if (write(pipefd_[1], &dummy_data, sizeof(dummy_data)) < 0) {
    LOG(ERROR) << "Pipe write failed";
  }
  return true;
}

int UnixServer::StartMessageLoop() {
  GSourceFuncs src_funcs = {mozc_prepare, mozc_check, mozc_dispatch, nullptr};

  MozcWatchSource *watch = reinterpret_cast<MozcWatchSource *>(
      gtk_->GSourceNew(&src_funcs, sizeof(MozcWatchSource)));
  gtk_->GSourceSetCanRecurse(&watch->source, TRUE);
  gtk_->GSourceAttach(&watch->source, nullptr);
  gtk_->GSourceSetCallback(&watch->source, nullptr, (gpointer)this, nullptr);

  watch->poll_fd.fd = pipefd_[0];
  watch->poll_fd.events = G_IO_IN | G_IO_HUP;
  watch->poll_fd.revents = 0;
  watch->unix_server = this;

  gtk_->GSourceAddPoll(reinterpret_cast<GSource *>(watch), &watch->poll_fd);

  gtk_->GdkThreadsEnter();
  gtk_->GtkMain();
  gtk_->GdkThreadsLeave();

  return 0;
}

void UnixServer::OpenPipe() {
  if (pipe(pipefd_) == 1) {
    LOG(FATAL) << "popen failed";
    return;
  }

  // TODO(nona): Close unused fd.
  fcntl(pipefd_[0], F_SETFL, O_NONBLOCK);
  fcntl(pipefd_[1], F_SETFL, O_NONBLOCK);
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

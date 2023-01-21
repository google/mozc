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

#include "renderer/unix/unix_renderer.h"

#include "base/util.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/unix/gtk_wrapper_mock.h"
#include "renderer/unix/window_manager_mock.h"
#include "testing/gmock.h"
#include "testing/googletest.h"
#include "testing/gunit.h"

using testing::_;
using testing::Return;
using testing::StrictMock;

namespace mozc {
namespace renderer {
namespace gtk {

TEST(UnixRendererTest, ExecCommand) {
  {
    SCOPED_TRACE("NOOP should not do anything.");

    WindowManagerMock *wm_mock = new WindowManagerMock();
    commands::RendererCommand command;

    command.set_type(commands::RendererCommand::NOOP);

    UnixRenderer renderer(wm_mock);
    EXPECT_TRUE(renderer.ExecCommand(command));
  }
  {
    SCOPED_TRACE("SHUTDOWN should quit main loop via AsyncQuit.");

    WindowManagerMock *wm_mock = new WindowManagerMock();
    commands::RendererCommand command;

    command.set_type(commands::RendererCommand::SHUTDOWN);

    UnixRenderer renderer(wm_mock);
    EXPECT_FALSE(renderer.ExecCommand(command));
  }
  {
    SCOPED_TRACE("UPDATE with visible = true update layout");

    WindowManagerMock *wm_mock = new WindowManagerMock();
    commands::RendererCommand command;

    command.set_type(commands::RendererCommand::UPDATE);
    command.set_visible(true);

    EXPECT_CALL(*wm_mock, UpdateLayout(_));

    UnixRenderer renderer(wm_mock);
    EXPECT_TRUE(renderer.ExecCommand(command));
  }
  {
    SCOPED_TRACE("UPDATE with visible = false call HideAllWindows");

    WindowManagerMock *wm_mock = new WindowManagerMock();
    commands::RendererCommand command;

    command.set_type(commands::RendererCommand::UPDATE);
    command.set_visible(false);

    EXPECT_CALL(*wm_mock, HideAllWindows());

    UnixRenderer renderer(wm_mock);
    EXPECT_TRUE(renderer.ExecCommand(command));
  }
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

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

#include "renderer/unix/unix_server.h"

#include "base/system_util.h"
#include "renderer/unix/gtk_wrapper_mock.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

using testing::Return;
using testing::StrictMock;
using testing::_;

namespace mozc {
namespace renderer {
namespace gtk {

class UnixServerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }
};

TEST_F(UnixServerTest, StartMessageLoopTest) {
  GtkWrapperMock *gtk_mock = new StrictMock<GtkWrapperMock>();

  UnixServer server(gtk_mock);

  UnixServer::MozcWatchSource watch;

  EXPECT_CALL(*gtk_mock, GSourceNew(_, _))
      .WillOnce(Return(reinterpret_cast<GSource*>(&watch)));
  EXPECT_CALL(*gtk_mock, GSourceSetCanRecurse(&watch.source, TRUE));
  EXPECT_CALL(*gtk_mock, GSourceAttach(&watch.source, NULL));
  EXPECT_CALL(*gtk_mock,
              GSourceSetCallback(&watch.source, NULL, (gpointer)&server, NULL));
  EXPECT_CALL(*gtk_mock, GSourceAddPoll(
      reinterpret_cast<GSource*>(&watch), &watch.poll_fd));
  EXPECT_CALL(*gtk_mock, GdkThreadsEnter());
  EXPECT_CALL(*gtk_mock, GtkMain());
  EXPECT_CALL(*gtk_mock, GdkThreadsLeave());

  server.StartMessageLoop();
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc

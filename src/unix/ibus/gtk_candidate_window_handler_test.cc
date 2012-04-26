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

#include "unix/ibus/gtk_candidate_window_handler.h"

#include <unistd.h>  // for getpid()

#include "renderer/renderer_command.pb.h"
#include "renderer/renderer_mock.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

using ::testing::AllOf;
using ::testing::Property;
using ::testing::Return;
using mozc::commands::Candidates;
using mozc::commands::Command;
using mozc::commands::Output;
using mozc::commands::RendererCommand;
using mozc::renderer::RendererInterface;
using mozc::renderer::RendererMock;
typedef mozc::commands::Candidates_Candidate Candidate;
typedef mozc::commands::RendererCommand_ApplicationInfo ApplicationInfo;

namespace mozc {
namespace ibus {

class TestableGtkCandidateWindowHandler : public GtkCandidateWindowHandler {
 public:
  explicit TestableGtkCandidateWindowHandler(RendererInterface *renderer)
      : GtkCandidateWindowHandler(renderer) {}
  virtual ~TestableGtkCandidateWindowHandler() {}

  // Change access rights.
  using GtkCandidateWindowHandler::SendUpdateCommand;
  using GtkCandidateWindowHandler::renderer_;
  using GtkCandidateWindowHandler::last_update_output_;
};

namespace {

MATCHER(IsIBusMozcRendererRequest, "") {
  const ApplicationInfo &info = arg.application_info();
  if (!info.has_process_id()) {
    *result_listener << "ApplicationInfo::process_id does not exist";
    return false;
  }
  if (::getpid() != info.process_id()) {
    *result_listener << "ProcessId does not match\n"
                     << "  expected: " << ::getpid() << "\n"
                     << "  actual:   " << info.process_id();
    return false;
  }
  if (!info.has_input_framework()) {
    *result_listener << "ApplicationInfo::input_framework does not exist";
    return false;
  }
  if (ApplicationInfo::IBus != info.input_framework()) {
    *result_listener << "InputFramework does not match\n"
                     << "  expected: " << ApplicationInfo::IBus << "\n"
                     << "  actual:   " << info.input_framework();
    return false;
  }
  return true;
}

MATCHER_P(VisibilityEq, visibility, "") {
  if (!arg.has_visible()) {
    *result_listener << "RendererCommand::visible does not exist";
    return false;
  }
  if (RendererCommand::UPDATE != arg.type()) {
    *result_listener << "RendererCommand::type does not match\n"
                     << "  expected: " << RendererCommand::UPDATE << "\n"
                     << "  actual:   " << arg.type();
    return false;
  }
  if (visibility != arg.visible()) {
    *result_listener << "The visibility does not match\n"
                     << "  expected: " << visibility << "\n"
                     << "  actual:   " << arg.visible();
    return false;
  }
  return true;
}

MATCHER_P(OutputEq, expected, "") {
  if (expected.Utf8DebugString() != arg.Utf8DebugString()) {
    *result_listener
        << "The output does not match\n"
        << "  expected: \n" << expected.Utf8DebugString() << "\n"
        << "  actual:   \n" << arg.Utf8DebugString();
    return false;
  }

  return true;
}

#define EXPECT_CALL_EXEC_COMMAND(mock, ...)                              \
    EXPECT_CALL((mock), ExecCommand(                                     \
        AllOf(IsIBusMozcRendererRequest(), __VA_ARGS__)))

}  // namespace

TEST(GtkCandidateWindowHandlerTest, SendUpdateCommandTest) {
  {
    SCOPED_TRACE("visibility check. false case");
    Output output;
    RendererMock *renderer_mock = new RendererMock();
    TestableGtkCandidateWindowHandler gtk_candidate_window_handler(
        renderer_mock);
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock, VisibilityEq(false));
    gtk_candidate_window_handler.SendUpdateCommand(output, false);
  }
  {
    SCOPED_TRACE("visibility check. true case");
    Output output;
    RendererMock *renderer_mock = new RendererMock();
    TestableGtkCandidateWindowHandler gtk_candidate_window_handler(
        renderer_mock);
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock, VisibilityEq(true));
    gtk_candidate_window_handler.SendUpdateCommand(output, true);
  }
  {
    SCOPED_TRACE("return value check. false case.");
    Output output;
    RendererMock *renderer_mock = new RendererMock();
    TestableGtkCandidateWindowHandler gtk_candidate_window_handler(
        renderer_mock);
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock, VisibilityEq(true))
        .WillOnce(Return(false));
    EXPECT_FALSE(gtk_candidate_window_handler.SendUpdateCommand(output, true));
  }
  {
    SCOPED_TRACE("return value check. true case.");
    Output output;
    RendererMock *renderer_mock = new RendererMock();
    TestableGtkCandidateWindowHandler gtk_candidate_window_handler(
        renderer_mock);
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock, VisibilityEq(true))
        .WillOnce(Return(true));
    EXPECT_TRUE(gtk_candidate_window_handler.SendUpdateCommand(output, true));
  }
}

TEST(GtkCandidateWindowHandlerTest, UpdateTest) {
  const int sample_idx1 = 0;
  const int sample_idx2 = 1;
  const char *sample_candidate1 = "SAMPLE_CANDIDATE1";
  const char *sample_candidate2 = "SAMPLE_CANDIDATE2";
  {
    SCOPED_TRACE("If there are no candidates, visibility expects false");
    Output output;
    RendererMock *renderer_mock = new RendererMock();
    TestableGtkCandidateWindowHandler gtk_candidate_window_handler(
        renderer_mock);
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock, VisibilityEq(false));
    gtk_candidate_window_handler.Update(NULL, output);
  }
  {
    SCOPED_TRACE("If there is at least one candidate, "
                 "visibility expects true");
    Output output;
    Candidates *candidates = output.mutable_candidates();
    Candidate *candidate = candidates->add_candidate();
    candidate->set_index(sample_idx1);
    candidate->set_value(sample_candidate1);
    RendererMock *renderer_mock = new RendererMock();
    TestableGtkCandidateWindowHandler gtk_candidate_window_handler(
        renderer_mock);
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock, VisibilityEq(true));
    gtk_candidate_window_handler.Update(NULL, output);
  }
  {
    SCOPED_TRACE("Update last updated output protobuf object.");
    Output output1;
    Output output2;
    Candidates *candidates1 = output1.mutable_candidates();
    Candidates *candidates2 = output2.mutable_candidates();
    Candidate *candidate1 = candidates1->add_candidate();
    Candidate *candidate2 = candidates2->add_candidate();
    candidate1->set_index(sample_idx1);
    candidate1->set_index(sample_idx2);
    candidate2->set_value(sample_candidate1);
    candidate2->set_value(sample_candidate2);

    RendererMock *renderer_mock = new RendererMock();
    TestableGtkCandidateWindowHandler gtk_candidate_window_handler(
        renderer_mock);
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock,
                             Property(&RendererCommand::output,
                                      OutputEq(output1)))
        .WillOnce(Return(true));
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock,
                             Property(&RendererCommand::output,
                                      OutputEq(output2)))
        .WillOnce(Return(true));
    gtk_candidate_window_handler.Update(NULL, output1);
    EXPECT_THAT(*(gtk_candidate_window_handler.last_update_output_.get()),
                OutputEq(output1));
    gtk_candidate_window_handler.Update(NULL, output2);
    EXPECT_THAT(*(gtk_candidate_window_handler.last_update_output_.get()),
                OutputEq(output2));
  }
}

TEST(GtkCandidateWindowHandlerTest, HideTest) {
  RendererMock *renderer_mock = new RendererMock();
  TestableGtkCandidateWindowHandler gtk_candidate_window_handler(
      renderer_mock);
  EXPECT_CALL_EXEC_COMMAND(*renderer_mock, VisibilityEq(false));
  gtk_candidate_window_handler.Hide(NULL);
}

TEST(GtkCandidateWindowHandlerTest, ShowTest) {
  RendererMock *renderer_mock = new RendererMock();
  TestableGtkCandidateWindowHandler gtk_candidate_window_handler(
      renderer_mock);
  EXPECT_CALL_EXEC_COMMAND(*renderer_mock, VisibilityEq(true));
  gtk_candidate_window_handler.Show(NULL);
}

}  // namespace ibus
}  // namespace mozc

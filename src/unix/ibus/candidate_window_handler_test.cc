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

#include "unix/ibus/candidate_window_handler.h"

#include <unistd.h>  // for getpid()
#include <memory>
#include <string>
#include <utility>

#include "base/coordinates.h"
#include "base/protobuf/text_format.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/renderer_interface.h"
#include "renderer/renderer_mock.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "unix/ibus/ibus_wrapper.h"

using mozc::commands::CandidateWindow;
using mozc::commands::Command;
using mozc::commands::Output;
using mozc::commands::RendererCommand;
using mozc::renderer::RendererInterface;
using mozc::renderer::RendererMock;
using ::testing::AllOf;
using ::testing::Property;
using ::testing::Return;
typedef mozc::commands::CandidateWindow_Candidate Candidate;
typedef mozc::commands::RendererCommand_ApplicationInfo ApplicationInfo;

namespace mozc {
namespace ibus {

class TestableCandidateWindowHandler : public CandidateWindowHandler {
 public:
  explicit TestableCandidateWindowHandler(
      std::unique_ptr<RendererInterface> renderer)
      : CandidateWindowHandler(std::move(renderer)) {}
  virtual ~TestableCandidateWindowHandler() {}

  // Change access rights.
  using CandidateWindowHandler::last_update_output_;
  using CandidateWindowHandler::renderer_;
  using CandidateWindowHandler::SendUpdateCommand;
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

MATCHER_P(PreeditRectangleEq, rect, "") {
  if (!arg.has_preedit_rectangle()) {
    *result_listener << "RendererCommand::preedit_rectangle does not exist";
    return false;
  }
  const auto &actual_rect = arg.preedit_rectangle();
  if (rect.Left() != actual_rect.left()) {
    *result_listener << "left field does not match\n"
                     << "  expected: " << rect.Left() << "\n"
                     << "  actual:   " << actual_rect.left();
    return false;
  }
  if (rect.Top() != actual_rect.top()) {
    *result_listener << "top field does not match\n"
                     << "  expected: " << rect.Top() << "\n"
                     << "  actual:   " << actual_rect.top();
    return false;
  }
  if (rect.Right() != actual_rect.right()) {
    *result_listener << "right field does not match\n"
                     << "  expected: " << rect.Right() << "\n"
                     << "  actual:   " << actual_rect.right();
    return false;
  }
  if (rect.Bottom() != actual_rect.bottom()) {
    *result_listener << "bottom field does not match\n"
                     << "  expected: " << rect.Bottom() << "\n"
                     << "  actual:   " << actual_rect.bottom();
    return false;
  }
  return true;
}

MATCHER_P(OutputEq, expected, "") {
  std::string arg_str;
  std::string expected_str;
  protobuf::TextFormat::PrintToString(expected, &expected_str);
  protobuf::TextFormat::PrintToString(arg, &arg_str);
  if (arg_str != expected_str) {
    *result_listener << "The output does not match\n"
                     << "  expected: \n"
                     << expected_str << "\n"
                     << "  actual:   \n"
                     << arg_str;
    return false;
  }

  return true;
}

#define EXPECT_CALL_EXEC_COMMAND(mock, ...) \
  EXPECT_CALL((mock),                       \
              ExecCommand(AllOf(IsIBusMozcRendererRequest(), __VA_ARGS__)))

}  // namespace

TEST(CandidateWindowHandlerTest, SendUpdateCommandTest) {
  const Rect kExpectedCursorArea(10, 20, 200, 100);

  IBusEngine ibus_engine = {};
  ibus_engine.cursor_area.x = kExpectedCursorArea.Left();
  ibus_engine.cursor_area.y = kExpectedCursorArea.Top();
  ibus_engine.cursor_area.width = kExpectedCursorArea.Width();
  ibus_engine.cursor_area.height = kExpectedCursorArea.Height();
  IbusEngineWrapper engine(&ibus_engine);

  {
    SCOPED_TRACE("visibility check. false case");
    Output output;
    auto renderer_mock = std::make_unique<RendererMock>();
    RendererMock *renderer_mock_ptr = renderer_mock.get();
    TestableCandidateWindowHandler candidate_window_handler(
        std::move(renderer_mock));
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock_ptr, VisibilityEq(false),
                             PreeditRectangleEq(kExpectedCursorArea));
    candidate_window_handler.SendUpdateCommand(&engine, output, false);
  }
  {
    SCOPED_TRACE("visibility check. true case");
    Output output;
    auto renderer_mock = std::make_unique<RendererMock>();
    RendererMock *renderer_mock_ptr = renderer_mock.get();
    TestableCandidateWindowHandler candidate_window_handler(
        std::move(renderer_mock));
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock_ptr, VisibilityEq(true),
                             PreeditRectangleEq(kExpectedCursorArea));
    candidate_window_handler.SendUpdateCommand(&engine, output, true);
  }
  {
    SCOPED_TRACE("return value check. false case.");
    Output output;
    auto renderer_mock = std::make_unique<RendererMock>();
    RendererMock *renderer_mock_ptr = renderer_mock.get();
    TestableCandidateWindowHandler candidate_window_handler(
        std::move(renderer_mock));
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock_ptr, VisibilityEq(true),
                             PreeditRectangleEq(kExpectedCursorArea))
        .WillOnce(Return(false));
    EXPECT_FALSE(
        candidate_window_handler.SendUpdateCommand(&engine, output, true));
  }
  {
    SCOPED_TRACE("return value check. true case.");
    Output output;
    auto renderer_mock = std::make_unique<RendererMock>();
    RendererMock *renderer_mock_ptr = renderer_mock.get();
    TestableCandidateWindowHandler candidate_window_handler(
        std::move(renderer_mock));
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock_ptr, VisibilityEq(true),
                             PreeditRectangleEq(kExpectedCursorArea))
        .WillOnce(Return(true));
    EXPECT_TRUE(
        candidate_window_handler.SendUpdateCommand(&engine, output, true));
  }
}

TEST(CandidateWindowHandlerTest, UpdateTest) {
  const Rect kExpectedCursorArea(10, 20, 200, 100);

  IBusEngine ibus_engine = {};
  ibus_engine.cursor_area.x = kExpectedCursorArea.Left();
  ibus_engine.cursor_area.y = kExpectedCursorArea.Top();
  ibus_engine.cursor_area.width = kExpectedCursorArea.Width();
  ibus_engine.cursor_area.height = kExpectedCursorArea.Height();
  IbusEngineWrapper engine(&ibus_engine);

  const int sample_idx1 = 0;
  const int sample_idx2 = 1;
  const char *sample_candidate1 = "SAMPLE_CANDIDATE1";
  const char *sample_candidate2 = "SAMPLE_CANDIDATE2";
  {
    SCOPED_TRACE("If there are no candidates, visibility expects false");
    Output output;
    auto renderer_mock = std::make_unique<RendererMock>();
    RendererMock *renderer_mock_ptr = renderer_mock.get();
    TestableCandidateWindowHandler candidate_window_handler(
        std::move(renderer_mock));
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock_ptr, VisibilityEq(false),
                             PreeditRectangleEq(kExpectedCursorArea));
    candidate_window_handler.Update(&engine, output);
  }
  {
    SCOPED_TRACE(
        "If there is at least one candidate, "
        "visibility expects true");
    Output output;
    CandidateWindow *candidate_window = output.mutable_candidate_window();
    Candidate *candidate = candidate_window->add_candidate();
    candidate->set_index(sample_idx1);
    candidate->set_value(sample_candidate1);
    auto renderer_mock = std::make_unique<RendererMock>();
    RendererMock *renderer_mock_ptr = renderer_mock.get();
    TestableCandidateWindowHandler candidate_window_handler(
        std::move(renderer_mock));
    EXPECT_CALL_EXEC_COMMAND(*renderer_mock_ptr, VisibilityEq(true),
                             PreeditRectangleEq(kExpectedCursorArea));
    candidate_window_handler.Update(&engine, output);
  }
  {
    SCOPED_TRACE("Update last updated output protobuf object.");
    Output output1;
    Output output2;
    CandidateWindow *candidate_window1 = output1.mutable_candidate_window();
    CandidateWindow *candidate_window2 = output2.mutable_candidate_window();
    Candidate *candidate1 = candidate_window1->add_candidate();
    Candidate *candidate2 = candidate_window2->add_candidate();
    candidate1->set_index(sample_idx1);
    candidate1->set_index(sample_idx2);
    candidate2->set_value(sample_candidate1);
    candidate2->set_value(sample_candidate2);

    auto renderer_mock = std::make_unique<RendererMock>();
    RendererMock *renderer_mock_ptr = renderer_mock.get();
    TestableCandidateWindowHandler candidate_window_handler(
        std::move(renderer_mock));
    EXPECT_CALL_EXEC_COMMAND(
        *renderer_mock_ptr,
        Property(&RendererCommand::output, OutputEq(output1)))
        .WillOnce(Return(true));
    EXPECT_CALL_EXEC_COMMAND(
        *renderer_mock_ptr,
        Property(&RendererCommand::output, OutputEq(output2)))
        .WillOnce(Return(true));
    candidate_window_handler.Update(&engine, output1);
    EXPECT_THAT(*(candidate_window_handler.last_update_output_.get()),
                OutputEq(output1));
    candidate_window_handler.Update(&engine, output2);
    EXPECT_THAT(*(candidate_window_handler.last_update_output_.get()),
                OutputEq(output2));
  }
}

TEST(CandidateWindowHandlerTest, HideTest) {
  const Rect kExpectedCursorArea(10, 20, 200, 100);

  IBusEngine ibus_engine = {};
  ibus_engine.cursor_area.x = kExpectedCursorArea.Left();
  ibus_engine.cursor_area.y = kExpectedCursorArea.Top();
  ibus_engine.cursor_area.width = kExpectedCursorArea.Width();
  ibus_engine.cursor_area.height = kExpectedCursorArea.Height();
  IbusEngineWrapper engine(&ibus_engine);

  auto renderer_mock = std::make_unique<RendererMock>();
  RendererMock *renderer_mock_ptr = renderer_mock.get();
  TestableCandidateWindowHandler candidate_window_handler(
      std::move(renderer_mock));
  EXPECT_CALL_EXEC_COMMAND(*renderer_mock_ptr, VisibilityEq(false),
                           PreeditRectangleEq(kExpectedCursorArea));
  candidate_window_handler.Hide(&engine);
}

TEST(CandidateWindowHandlerTest, ShowTest) {
  const Rect kExpectedCursorArea(10, 20, 200, 100);

  IBusEngine ibus_engine = {};
  ibus_engine.cursor_area.x = kExpectedCursorArea.Left();
  ibus_engine.cursor_area.y = kExpectedCursorArea.Top();
  ibus_engine.cursor_area.width = kExpectedCursorArea.Width();
  ibus_engine.cursor_area.height = kExpectedCursorArea.Height();
  IbusEngineWrapper engine(&ibus_engine);

  auto renderer_mock = std::make_unique<RendererMock>();
  RendererMock *renderer_mock_ptr = renderer_mock.get();
  TestableCandidateWindowHandler candidate_window_handler(
      std::move(renderer_mock));
  EXPECT_CALL_EXEC_COMMAND(*renderer_mock_ptr, VisibilityEq(true),
                           PreeditRectangleEq(kExpectedCursorArea));
  candidate_window_handler.Show(&engine);
}

}  // namespace ibus
}  // namespace mozc

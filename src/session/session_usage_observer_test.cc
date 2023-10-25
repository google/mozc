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

#include "session/session_usage_observer.h"

#include <cstdint>
#include <memory>

#include "base/clock.h"
#include "base/clock_mock.h"
#include "base/logging.h"
#include "config/stats_config_util.h"
#include "config/stats_config_util_mock.h"
#include "protocol/commands.pb.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats.pb.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/time/time.h"

namespace mozc {
namespace session {

using ::mozc::usage_stats::Stats;
using ::mozc::usage_stats::UsageStats;

class SessionUsageObserverTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    UsageStats::ClearAllStatsForTest();

    Clock::SetClockForUnitTest(nullptr);

    stats_config_util_mock_ = std::make_unique<config::StatsConfigUtilMock>();
    config::StatsConfigUtil::SetHandler(stats_config_util_mock_.get());
  }

  void TearDown() override {
    Clock::SetClockForUnitTest(nullptr);
    config::StatsConfigUtil::SetHandler(nullptr);

    UsageStats::ClearAllStatsForTest();
  }

  void SetDoubleValueStats(uint32_t num, double total, double square_total,
                           Stats::DoubleValueStats *double_stats) {
    DCHECK(double_stats);
    double_stats->set_num(num);
    double_stats->set_total(total);
    double_stats->set_square_total(square_total);
  }

  void SetEventStats(uint32_t source_id, uint32_t sx_num, double sx_total,
                     double sx_square_total, uint32_t sy_num, double sy_total,
                     double sy_square_total, uint32_t dx_num, double dx_total,
                     double dx_square_total, uint32_t dy_num, double dy_total,
                     double dy_square_total, uint32_t tl_num, double tl_total,
                     double tl_square_total,
                     Stats::TouchEventStats *event_stats) {
    event_stats->set_source_id(source_id);
    SetDoubleValueStats(sx_num, sx_total, sx_square_total,
                        event_stats->mutable_start_x_stats());
    SetDoubleValueStats(sy_num, sy_total, sy_square_total,
                        event_stats->mutable_start_y_stats());
    SetDoubleValueStats(dx_num, dx_total, dx_square_total,
                        event_stats->mutable_direction_x_stats());
    SetDoubleValueStats(dy_num, dy_total, dy_square_total,
                        event_stats->mutable_direction_y_stats());
    SetDoubleValueStats(tl_num, tl_total, tl_square_total,
                        event_stats->mutable_time_length_stats());
  }

  std::unique_ptr<config::StatsConfigUtilMock> stats_config_util_mock_;
};

TEST_F(SessionUsageObserverTest, DoNotSaveWhenDeleted) {
  stats_config_util_mock_->SetEnabled(false);

  std::unique_ptr<SessionUsageObserver> observer(new SessionUsageObserver);

  // Add command
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::NONE);
  command.mutable_input()->set_id(0);
  command.mutable_output()->set_consumed(true);
  for (int i = 0; i < 5; ++i) {
    observer->EvalCommandHandler(command);
    EXPECT_STATS_NOT_EXIST("SessionAllEvent");
  }

  observer.reset();
  EXPECT_STATS_NOT_EXIST("SessionAllEvent");
}

TEST_F(SessionUsageObserverTest, ClientSideStatsInfolist) {
  std::unique_ptr<SessionUsageObserver> observer(new SessionUsageObserver);

  // create session
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
    command.mutable_input()->set_id(1);
    command.mutable_output()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  ClockMock clock(absl::UnixEpoch());
  Clock::SetClockForUnitTest(&clock);

  // prepare command
  commands::Command orig_show_command, orig_hide_command;
  orig_show_command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  orig_show_command.mutable_input()->set_id(1);
  orig_show_command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::USAGE_STATS_EVENT);
  orig_show_command.mutable_input()->mutable_command()->set_usage_stats_event(
      commands::SessionCommand::INFOLIST_WINDOW_SHOW);
  orig_show_command.mutable_output()->set_consumed(false);
  EXPECT_TRUE(orig_show_command.output().has_consumed());
  EXPECT_FALSE(orig_show_command.output().consumed());
  EXPECT_TRUE(orig_show_command.input().has_id());
  orig_hide_command = orig_show_command;
  orig_hide_command.mutable_input()->mutable_command()->set_usage_stats_event(
      commands::SessionCommand::INFOLIST_WINDOW_HIDE);

  {  // show infolist, wait 1100 msec and hide infolist.
    commands::Command show_command, hide_command;
    show_command = orig_show_command;
    hide_command = orig_hide_command;

    observer->EvalCommandHandler(show_command);
    EXPECT_STATS_NOT_EXIST("InfolistWindowDurationMSec");
    clock.Advance(absl::Milliseconds(1100));
    observer->EvalCommandHandler(hide_command);
    EXPECT_TIMING_STATS("InfolistWindowDurationMSec", 1100, 1, 1100, 1100);
  }

  {  // show infolist, wait 1200 msec and hide infolist.
    commands::Command show_command, hide_command;
    show_command = orig_show_command;
    hide_command = orig_hide_command;

    observer->EvalCommandHandler(show_command);
    clock.Advance(absl::Milliseconds(1200));
    observer->EvalCommandHandler(hide_command);
    EXPECT_TIMING_STATS("InfolistWindowDurationMSec", 2300, 2, 1100, 1200);
  }
}

TEST_F(SessionUsageObserverTest, ClientSideStatsSoftwareKeyboardLayout) {
  SessionUsageObserver observer;

  // create session
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
  command.mutable_input()->set_id(1);
  command.mutable_output()->set_id(1);
  observer.EvalCommandHandler(command);

  EXPECT_STATS_NOT_EXIST("SoftwareKeyboardLayoutLandscape");
  EXPECT_STATS_NOT_EXIST("SoftwareKeyboardLayoutPortrait");
  EXPECT_STATS_NOT_EXIST("SoftwareKeyboardLayoutEnglishLandscape");
  EXPECT_STATS_NOT_EXIST("SoftwareKeyboardLayoutEnglishPortrait");

  command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  commands::SessionCommand *session_command =
      command.mutable_input()->mutable_command();
  session_command->set_type(commands::SessionCommand::USAGE_STATS_EVENT);
  session_command->set_usage_stats_event(
      commands::SessionCommand::SOFTWARE_KEYBOARD_LAYOUT_LANDSCAPE);
  session_command->set_usage_stats_event_int_value(1);
  observer.EvalCommandHandler(command);
  EXPECT_INTEGER_STATS("SoftwareKeyboardLayoutLandscape", 1);
  EXPECT_STATS_NOT_EXIST("SoftwareKeyboardLayoutPortrait");

  session_command->set_usage_stats_event_int_value(2);
  observer.EvalCommandHandler(command);
  EXPECT_INTEGER_STATS("SoftwareKeyboardLayoutLandscape", 2);
  EXPECT_STATS_NOT_EXIST("SoftwareKeyboardLayoutPortrait");

  session_command->set_usage_stats_event(
      commands::SessionCommand::SOFTWARE_KEYBOARD_LAYOUT_PORTRAIT);
  session_command->set_usage_stats_event_int_value(3);
  observer.EvalCommandHandler(command);
  EXPECT_INTEGER_STATS("SoftwareKeyboardLayoutLandscape", 2);
  EXPECT_INTEGER_STATS("SoftwareKeyboardLayoutPortrait", 3);
}

TEST_F(SessionUsageObserverTest, SubmittedCandidateRow) {
  SessionUsageObserver observer;

  // create session
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
  command.mutable_input()->set_id(1);
  command.mutable_output()->set_id(1);
  observer.EvalCommandHandler(command);

  EXPECT_STATS_NOT_EXIST("SubmittedCandidateRow0");
  EXPECT_STATS_NOT_EXIST("SubmittedCandidateRow1");
  EXPECT_STATS_NOT_EXIST("SubmittedCandidateRow2");
  EXPECT_STATS_NOT_EXIST("SubmittedCandidateRow3");
  EXPECT_STATS_NOT_EXIST("SubmittedCandidateRow4");
  EXPECT_STATS_NOT_EXIST("SubmittedCandidateRow5");
  EXPECT_STATS_NOT_EXIST("SubmittedCandidateRow6");
  EXPECT_STATS_NOT_EXIST("SubmittedCandidateRow7");
  EXPECT_STATS_NOT_EXIST("SubmittedCandidateRow8");
  EXPECT_STATS_NOT_EXIST("SubmittedCandidateRow9");
  EXPECT_STATS_NOT_EXIST("SubmittedCandidateRowGE10");

  command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  commands::SessionCommand *session_command =
      command.mutable_input()->mutable_command();
  session_command->set_type(commands::SessionCommand::USAGE_STATS_EVENT);
  session_command->set_usage_stats_event(
      commands::SessionCommand::SUBMITTED_CANDIDATE_ROW_0);
  observer.EvalCommandHandler(command);
  EXPECT_COUNT_STATS("SubmittedCandidateRow0", 1);

  observer.EvalCommandHandler(command);
  EXPECT_COUNT_STATS("SubmittedCandidateRow0", 2);
}

TEST_F(SessionUsageObserverTest, LogTouchEvent) {
  std::unique_ptr<SessionUsageObserver> observer(new SessionUsageObserver);

  // create session
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
    command.mutable_input()->set_id(1);
    command.mutable_output()->set_id(1);
    observer->EvalCommandHandler(command);
  }
  // set keyboard
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SET_REQUEST);
    command.mutable_input()->set_id(1);
    command.mutable_input()->mutable_request()->set_keyboard_name("KB1");
    observer->EvalCommandHandler(command);
  }
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SEND_KEY);
    command.mutable_input()->set_id(1);
    commands::Input::TouchEvent *touch_event =
        command.mutable_input()->add_touch_events();
    touch_event->set_source_id(10);
    commands::Input::TouchPosition *pos1 = touch_event->add_stroke();
    pos1->set_action(commands::Input::TOUCH_DOWN);
    pos1->set_x(1);
    pos1->set_y(2);
    pos1->set_timestamp(0);
    commands::Input::TouchPosition *pos2 = touch_event->add_stroke();
    pos2->set_action(commands::Input::TOUCH_UP);
    pos2->set_x(2);
    pos2->set_y(1);
    pos2->set_timestamp(1500);
    observer->EvalCommandHandler(command);
  }
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SEND_KEY);
    command.mutable_input()->set_id(1);
    commands::Input::TouchEvent *touch_event =
        command.mutable_input()->add_touch_events();
    touch_event->set_source_id(10);
    commands::Input::TouchPosition *pos1 = touch_event->add_stroke();
    pos1->set_action(commands::Input::TOUCH_DOWN);
    pos1->set_x(2);
    pos1->set_y(2);
    pos1->set_timestamp(0);
    commands::Input::TouchPosition *pos2 = touch_event->add_stroke();
    pos2->set_action(commands::Input::TOUCH_MOVE);
    pos2->set_x(2);
    pos2->set_y(2);
    pos2->set_timestamp(1000);
    commands::Input::TouchPosition *pos3 = touch_event->add_stroke();
    pos3->set_action(commands::Input::TOUCH_MOVE);
    pos3->set_x(1);
    pos3->set_y(1);
    pos3->set_timestamp(2000);
    observer->EvalCommandHandler(command);
  }
  // change keyboard
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SET_REQUEST);
    command.mutable_input()->set_id(1);
    command.mutable_input()->mutable_request()->set_keyboard_name("KB2");
    commands::Input::TouchEvent *touch_event =
        command.mutable_input()->add_touch_events();
    touch_event->set_source_id(100);
    commands::Input::TouchPosition *pos1 = touch_event->add_stroke();
    pos1->set_action(commands::Input::TOUCH_DOWN);
    pos1->set_x(1);
    pos1->set_y(1);
    pos1->set_timestamp(0);
    commands::Input::TouchPosition *pos2 = touch_event->add_stroke();
    pos2->set_action(commands::Input::TOUCH_UP);
    pos2->set_x(1);
    pos2->set_y(1);
    pos2->set_timestamp(1000);
    observer->EvalCommandHandler(command);
  }
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SEND_KEY);
    command.mutable_input()->set_id(1);
    commands::Input::TouchEvent *touch_event =
        command.mutable_input()->add_touch_events();
    touch_event->set_source_id(10);
    commands::Input::TouchPosition *pos1 = touch_event->add_stroke();
    pos1->set_action(commands::Input::TOUCH_DOWN);
    pos1->set_x(1);
    pos1->set_y(2);
    pos1->set_timestamp(0);
    commands::Input::TouchPosition *pos2 = touch_event->add_stroke();
    pos2->set_action(commands::Input::TOUCH_UP);
    pos2->set_x(2);
    pos2->set_y(1);
    pos2->set_timestamp(1500);
    observer->EvalCommandHandler(command);
  }
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SEND_KEY);
    command.mutable_input()->set_id(1);
    commands::Input::TouchEvent *touch_event =
        command.mutable_input()->add_touch_events();
    touch_event->set_source_id(20);
    commands::Input::TouchPosition *pos1 = touch_event->add_stroke();
    pos1->set_action(commands::Input::TOUCH_DOWN);
    pos1->set_x(2);
    pos1->set_y(2);
    pos1->set_timestamp(0);
    commands::Input::TouchPosition *pos2 = touch_event->add_stroke();
    pos2->set_action(commands::Input::TOUCH_UP);
    pos2->set_x(1);
    pos2->set_y(1);
    pos2->set_timestamp(2000);

    // add preedit
    command.mutable_output()->mutable_preedit();
    command.mutable_output()->set_consumed(true);
    observer->EvalCommandHandler(command);
  }
  {
    // send BACKSPACE
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SEND_KEY);
    command.mutable_input()->set_id(1);
    command.mutable_input()->mutable_key()->set_special_key(
        commands::KeyEvent::BACKSPACE);
    observer->EvalCommandHandler(command);
  }
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SEND_KEY);
    command.mutable_input()->set_id(1);
    commands::Input::TouchEvent *touch_event =
        command.mutable_input()->add_touch_events();
    touch_event->set_source_id(30);
    commands::Input::TouchPosition *pos1 = touch_event->add_stroke();
    pos1->set_action(commands::Input::TOUCH_DOWN);
    pos1->set_x(2);
    pos1->set_y(2);
    pos1->set_timestamp(0);
    commands::Input::TouchPosition *pos2 = touch_event->add_stroke();
    pos2->set_action(commands::Input::TOUCH_MOVE);
    pos2->set_x(1);
    pos2->set_y(1);
    pos2->set_timestamp(1000);
    commands::Input::TouchPosition *pos3 = touch_event->add_stroke();
    pos3->set_action(commands::Input::TOUCH_UP);
    pos3->set_x(1);
    pos3->set_y(3);
    pos3->set_timestamp(2000);
    observer->EvalCommandHandler(command);
  }
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::NONE);
    command.mutable_input()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  EXPECT_STATS_NOT_EXIST("VirtualKeyboardStats");
  EXPECT_STATS_NOT_EXIST("VirtualKeyboardMissStats");

  // Does not store usage stats anymore.
  Stats stats;
  EXPECT_FALSE(
      UsageStats::GetVirtualKeyboardForTest("VirtualKeyboardStats", &stats));
  EXPECT_FALSE(UsageStats::GetVirtualKeyboardForTest("VirtualKeyboardMissStats",
                                                     &stats));
}

TEST_F(SessionUsageObserverTest, LogTouchEventPasswordField) {
  std::unique_ptr<SessionUsageObserver> observer(new SessionUsageObserver);

  // create session
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
    command.mutable_input()->set_id(1);
    command.mutable_output()->set_id(1);
    observer->EvalCommandHandler(command);
  }
  // set keyboard
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SET_REQUEST);
    command.mutable_input()->set_id(1);
    command.mutable_input()->mutable_request()->set_keyboard_name("KB1");
    observer->EvalCommandHandler(command);
  }
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SEND_KEY);
    command.mutable_input()->set_id(1);
    commands::Input::TouchEvent *touch_event =
        command.mutable_input()->add_touch_events();
    touch_event->set_source_id(10);
    commands::Input::TouchPosition *pos1 = touch_event->add_stroke();
    pos1->set_action(commands::Input::TOUCH_DOWN);
    pos1->set_x(1);
    pos1->set_y(1);
    pos1->set_timestamp(0);
    commands::Input::TouchPosition *pos2 = touch_event->add_stroke();
    pos2->set_action(commands::Input::TOUCH_UP);
    pos2->set_x(1);
    pos2->set_y(1);
    pos2->set_timestamp(1000);
    observer->EvalCommandHandler(command);
  }
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SEND_KEY);
    command.mutable_input()->set_id(1);
    commands::Input::TouchEvent *touch_event =
        command.mutable_input()->add_touch_events();
    touch_event->set_source_id(20);
    commands::Input::TouchPosition *pos1 = touch_event->add_stroke();
    pos1->set_action(commands::Input::TOUCH_DOWN);
    pos1->set_x(1);
    pos1->set_y(1);
    pos1->set_timestamp(0);
    commands::Input::TouchPosition *pos2 = touch_event->add_stroke();
    pos2->set_action(commands::Input::TOUCH_MOVE);
    pos2->set_x(1);
    pos2->set_y(1);
    pos2->set_timestamp(1000);
    observer->EvalCommandHandler(command);
  }
  {
    // Changes INPUT_FIELD_TYPE to PASSWORD
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
    command.mutable_input()->set_id(1);
    command.mutable_input()->mutable_command()->set_type(
        commands::SessionCommand::SWITCH_INPUT_FIELD_TYPE);
    command.mutable_input()->mutable_context()->set_input_field_type(
        commands::Context::PASSWORD);
    observer->EvalCommandHandler(command);
  }
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SEND_KEY);
    command.mutable_input()->set_id(1);
    commands::Input::TouchEvent *touch_event =
        command.mutable_input()->add_touch_events();
    touch_event->set_source_id(30);
    commands::Input::TouchPosition *pos1 = touch_event->add_stroke();
    pos1->set_action(commands::Input::TOUCH_DOWN);
    pos1->set_x(1);
    pos1->set_y(1);
    pos1->set_timestamp(0);
    commands::Input::TouchPosition *pos2 = touch_event->add_stroke();
    pos2->set_action(commands::Input::TOUCH_UP);
    pos2->set_x(1);
    pos2->set_y(1);
    pos2->set_timestamp(1000);
    observer->EvalCommandHandler(command);
  }
  {
    // Changes INPUT_FIELD_TYPE to NORMAL
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
    command.mutable_input()->set_id(1);
    command.mutable_input()->mutable_command()->set_type(
        commands::SessionCommand::SWITCH_INPUT_FIELD_TYPE);
    command.mutable_input()->mutable_context()->set_input_field_type(
        commands::Context::NORMAL);
    observer->EvalCommandHandler(command);
  }
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SEND_KEY);
    command.mutable_input()->set_id(1);
    commands::Input::TouchEvent *touch_event =
        command.mutable_input()->add_touch_events();
    touch_event->set_source_id(40);
    commands::Input::TouchPosition *pos1 = touch_event->add_stroke();
    pos1->set_action(commands::Input::TOUCH_DOWN);
    pos1->set_x(1);
    pos1->set_y(1);
    pos1->set_timestamp(0);
    commands::Input::TouchPosition *pos2 = touch_event->add_stroke();
    pos2->set_action(commands::Input::TOUCH_UP);
    pos2->set_x(1);
    pos2->set_y(1);
    pos2->set_timestamp(1000);
    observer->EvalCommandHandler(command);
  }
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::NONE);
    command.mutable_input()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  EXPECT_STATS_NOT_EXIST("VirtualKeyboardStats");
  EXPECT_STATS_NOT_EXIST("VirtualKeyboardMissStats");

  // Does not store usage stats anymore.
  Stats stats;
  EXPECT_FALSE(
      UsageStats::GetVirtualKeyboardForTest("VirtualKeyboardStats", &stats));
  EXPECT_FALSE(UsageStats::GetVirtualKeyboardForTest("VirtualKeyboardMissStats",
                                                     &stats));
}

}  // namespace session
}  // namespace mozc

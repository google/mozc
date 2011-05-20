// Copyright 2010-2011, Google Inc.
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

#include <map>
#include "client/session_mock.h"
#include "testing/base/public/gunit.h"
#include "unix/ibus/mozc_engine.h"

namespace mozc {
namespace ibus {

class MozcEngineTest : public testing::Test {
 protected:
  virtual void SetUp() {
    currently_pressed_modifiers_.clear();
    modifiers_to_be_sent_.clear();
    keyval_to_modifier_.clear();
    keyval_to_modifier_[IBUS_Shift_L] = commands::KeyEvent::SHIFT;
    keyval_to_modifier_[IBUS_Shift_R] = commands::KeyEvent::SHIFT;
    keyval_to_modifier_[IBUS_Control_L] = commands::KeyEvent::CTRL;
    keyval_to_modifier_[IBUS_Control_R] = commands::KeyEvent::CTRL;
    keyval_to_modifier_[IBUS_Alt_L] = commands::KeyEvent::ALT;
    keyval_to_modifier_[IBUS_Alt_R] = commands::KeyEvent::ALT;
  }

  bool ProcessKey(bool is_key_up, gint keyval, commands::KeyEvent *key) {
    if (keyval_to_modifier_.find(keyval) != keyval_to_modifier_.end()) {
      key->add_modifier_keys(keyval_to_modifier_[keyval]);
    } else {
      key->set_key_code(keyval);
    }
    return MozcEngine::ProcessModifiers(is_key_up,
                                        keyval,
                                        key,
                                        &currently_pressed_modifiers_,
                                        &modifiers_to_be_sent_);
  }

  bool IsPressed(gint keyval) const {
    return currently_pressed_modifiers_.end() !=
        currently_pressed_modifiers_.find(keyval);
  }

  set<gint> currently_pressed_modifiers_;
  set<commands::KeyEvent::ModifierKey> modifiers_to_be_sent_;
  map<gint, commands::KeyEvent::ModifierKey> keyval_to_modifier_;
};

class LaunchToolTest : public testing::Test {
 public:
  LaunchToolTest() {
    g_type_init();
    mozc_engine_.reset(new MozcEngine());
  }
 protected:
  virtual void SetUp() {
    mock_.ClearFunctionCounter();
    mozc_engine_->session_.reset(new client::SessionMock());
  }

  client::SessionMock mock_;
  scoped_ptr<MozcEngine> mozc_engine_;
 private:
  DISALLOW_COPY_AND_ASSIGN(LaunchToolTest);
};

TEST_F(LaunchToolTest, LaunchToolTest) {
  commands::Output output;

  // Launch config dialog
  mock_.ClearFunctionCounter();
  mock_.SetBoolFunctionReturn("LaunchTool", true);
  output.set_launch_tool_mode(commands::Output::CONFIG_DIALOG);
  EXPECT_TRUE(mozc_engine_->LaunchTool(output));
  EXPECT_EQ(1, mock_.GetFunctionCallCount("LaunchTool"));

  // Launch dictionary tool
  mock_.ClearFunctionCounter();
  mock_.SetBoolFunctionReturn("LaunchTool", true);
  output.set_launch_tool_mode(commands::Output::DICTIONARY_TOOL);
  EXPECT_TRUE(mozc_engine_->LaunchTool(output));
  EXPECT_EQ(1, mock_.GetFunctionCallCount("LaunchTool"));

  // Launch word register dialog
  mock_.ClearFunctionCounter();
  mock_.SetBoolFunctionReturn("LaunchTool", true);
  output.set_launch_tool_mode(commands::Output::WORD_REGISTER_DIALOG);
  EXPECT_TRUE(mozc_engine_->LaunchTool(output));
  EXPECT_EQ(1, mock_.GetFunctionCallCount("LaunchTool"));

  // Launch no tool(means do nothing)
  mock_.ClearFunctionCounter();
  mock_.SetBoolFunctionReturn("LaunchTool", true);
  output.set_launch_tool_mode(commands::Output::NO_TOOL);
  EXPECT_FALSE(mozc_engine_->LaunchTool(output));
  EXPECT_EQ(0, mock_.GetFunctionCallCount("LaunchTool"));

  // Something occurring in client::Session::LaunchTool
  mock_.ClearFunctionCounter();
  mock_.SetBoolFunctionReturn("LaunchTool", false);
  output.set_launch_tool_mode(commands::Output::CONFIG_DIALOG);
  EXPECT_FALSE(mozc_engine_->LaunchTool(output));
  EXPECT_EQ(1, mock_.GetFunctionCallCount("LaunchTool"));
}

TEST_F(MozcEngineTest, ProcessShiftModifiers) {
  commands::KeyEvent key;

  // 'Shift-a' senario
  // Shift down
  EXPECT_FALSE(ProcessKey(false, IBUS_Shift_L, &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_NE(modifiers_to_be_sent_.end(),
            modifiers_to_be_sent_.find(commands::KeyEvent::SHIFT));
  EXPECT_EQ(modifiers_to_be_sent_.size(), 1);

  // "a" down
  key.Clear();
  EXPECT_TRUE(ProcessKey(false, 'a', &key));
  EXPECT_FALSE(IsPressed(IBUS_Shift_L));
  EXPECT_EQ(modifiers_to_be_sent_.size(), 0);

  // "a" up
  key.Clear();
  EXPECT_FALSE(ProcessKey(true, 'a', &key));
  EXPECT_FALSE(IsPressed(IBUS_Shift_L));
  EXPECT_EQ(modifiers_to_be_sent_.size(), 0);

  // Shift up
  key.Clear();
  EXPECT_FALSE(ProcessKey(true, IBUS_Shift_L, &key));
  EXPECT_TRUE(currently_pressed_modifiers_.empty());
  EXPECT_TRUE(modifiers_to_be_sent_.empty());

  /* Currently following test scenario does not pass.
   * This bug was issued as b/4338394
  // 'Shift-0' senario
  // Shift down
  EXPECT_FALSE(ProcessKey(false, IBUS_Shift_L, &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_NE(modifiers_to_be_sent_.end(),
            modifiers_to_be_sent_.find(commands::KeyEvent::SHIFT));
  EXPECT_EQ(modifiers_to_be_sent_.size(), 1);

  // "0" down
  key.Clear();
  EXPECT_TRUE(ProcessKey(false, '0', &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_EQ(modifiers_to_be_sent_.size(), 0);

  // "0" up
  key.Clear();
  EXPECT_FALSE(ProcessKey(true, '0', &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_EQ(modifiers_to_be_sent_.size(), 0);

  // Shift up
  key.Clear();
  EXPECT_TRUE(ProcessKey(true, IBUS_Shift_L, &key));
  EXPECT_TRUE(currently_pressed_modifiers_.empty());
  EXPECT_TRUE(modifiers_to_be_sent_.empty());
  */
}

TEST_F(MozcEngineTest, ProcessAltModifiers) {
  commands::KeyEvent key;

  // Alt down
  EXPECT_FALSE(ProcessKey(false, IBUS_Alt_L, &key));
  EXPECT_TRUE(IsPressed(IBUS_Alt_L));
  EXPECT_NE(modifiers_to_be_sent_.end(),
            modifiers_to_be_sent_.find(commands::KeyEvent::ALT));
  EXPECT_EQ(modifiers_to_be_sent_.size(), 1);

  // "a" down
  key.Clear();
  key.add_modifier_keys(commands::KeyEvent::ALT);
  EXPECT_TRUE(ProcessKey(false, 'a', &key));
  EXPECT_TRUE(IsPressed(IBUS_Alt_L));
  EXPECT_NE(modifiers_to_be_sent_.end(),
            modifiers_to_be_sent_.find(commands::KeyEvent::ALT));
  EXPECT_EQ(modifiers_to_be_sent_.size(), 1);

  // "a" up
  key.Clear();
  key.add_modifier_keys(commands::KeyEvent::ALT);
  EXPECT_FALSE(ProcessKey(true, 'a', &key));
  EXPECT_TRUE(IsPressed(IBUS_Alt_L));
  EXPECT_NE(modifiers_to_be_sent_.end(),
            modifiers_to_be_sent_.find(commands::KeyEvent::ALT));
  EXPECT_EQ(modifiers_to_be_sent_.size(), 1);

  // ALt up
  key.Clear();
  EXPECT_TRUE(ProcessKey(true, IBUS_Alt_L, &key));
  EXPECT_TRUE(currently_pressed_modifiers_.empty());
  EXPECT_TRUE(modifiers_to_be_sent_.empty());
}

TEST_F(MozcEngineTest, ProcessCtrlModifiers) {
  commands::KeyEvent key;

  // Ctrl down
  EXPECT_FALSE(ProcessKey(false, IBUS_Control_L, &key));
  EXPECT_TRUE(IsPressed(IBUS_Control_L));
  EXPECT_NE(modifiers_to_be_sent_.end(),
            modifiers_to_be_sent_.find(commands::KeyEvent::CTRL));
  EXPECT_EQ(modifiers_to_be_sent_.size(), 1);

  // "a" down
  key.Clear();
  key.add_modifier_keys(commands::KeyEvent::CTRL);
  EXPECT_TRUE(ProcessKey(false, 'a', &key));
  EXPECT_TRUE(IsPressed(IBUS_Control_L));
  EXPECT_NE(modifiers_to_be_sent_.end(),
            modifiers_to_be_sent_.find(commands::KeyEvent::CTRL));
  EXPECT_EQ(modifiers_to_be_sent_.size(), 1);

  // "a" up
  key.Clear();
  key.add_modifier_keys(commands::KeyEvent::CTRL);
  EXPECT_FALSE(ProcessKey(true, 'a', &key));
  EXPECT_TRUE(IsPressed(IBUS_Control_L));
  EXPECT_NE(modifiers_to_be_sent_.end(),
            modifiers_to_be_sent_.find(commands::KeyEvent::CTRL));
  EXPECT_EQ(modifiers_to_be_sent_.size(), 1);

  // Ctrl up
  key.Clear();
  EXPECT_TRUE(ProcessKey(true, IBUS_Control_L, &key));
  EXPECT_TRUE(currently_pressed_modifiers_.empty());
  EXPECT_TRUE(modifiers_to_be_sent_.empty());
}

TEST_F(MozcEngineTest, ProcessModifiers) {
  commands::KeyEvent key;

  // Shift down => Shift up
  key.Clear();
  ProcessKey(false, IBUS_Shift_L, &key);

  key.Clear();
  EXPECT_TRUE(ProcessKey(true, IBUS_Shift_L, &key));
  EXPECT_TRUE(currently_pressed_modifiers_.empty());
  EXPECT_TRUE(modifiers_to_be_sent_.empty());
  EXPECT_EQ(commands::KeyEvent::SHIFT, key.modifier_keys(0));

  // Shift down => Ctrl down => Shift up => Alt down => Ctrl up => Alt up
  key.Clear();
  ProcessKey(false, IBUS_Shift_L, &key);
  key.Clear();
  EXPECT_FALSE(ProcessKey(false, IBUS_Control_L, &key));
  key.Clear();
  EXPECT_FALSE(ProcessKey(true, IBUS_Shift_L, &key));
  key.Clear();
  EXPECT_FALSE(ProcessKey(false, IBUS_Alt_L, &key));
  key.Clear();
  EXPECT_FALSE(ProcessKey(true, IBUS_Control_L, &key));
  key.Clear();
  EXPECT_TRUE(ProcessKey(true, IBUS_Alt_L, &key));
  EXPECT_TRUE(currently_pressed_modifiers_.empty());
  EXPECT_TRUE(modifiers_to_be_sent_.empty());
  {
    bool has_shift = false, has_ctrl = false, has_alt = false;
    for (size_t i = 0; i < key.modifier_keys_size(); ++i) {
      switch (key.modifier_keys(i)) {
        case commands::KeyEvent::SHIFT:
          has_shift = true;
          break;
        case commands::KeyEvent::CTRL:
          has_ctrl = true;
          break;
        case commands::KeyEvent::ALT:
          has_alt = true;
          break;
        case commands::KeyEvent::KEY_DOWN:
        case commands::KeyEvent::KEY_UP:
        case commands::KeyEvent::LEFT_CTRL:
        case commands::KeyEvent::LEFT_ALT:
        case commands::KeyEvent::LEFT_SHIFT:
        case commands::KeyEvent::RIGHT_CTRL:
        case commands::KeyEvent::RIGHT_ALT:
        case commands::KeyEvent::RIGHT_SHIFT:
          ADD_FAILURE() << "Incorrect modifier key: " << key.modifier_keys(i);
          break;
      }
    }
    EXPECT_TRUE(has_shift);
    EXPECT_TRUE(has_ctrl);
    EXPECT_TRUE(has_alt);
  }
}

}  // namespace ibus
}  // namespace mozc

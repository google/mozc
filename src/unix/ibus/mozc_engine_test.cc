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

#include "unix/ibus/mozc_engine.h"

#include <memory>

#include "base/port.h"
#include "client/client_mock.h"
#include "protocol/commands.pb.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace ibus {

class LaunchToolTest : public testing::Test {
 public:
  LaunchToolTest() { g_type_init(); }

 protected:
  virtual void SetUp() {
    mozc_engine_.reset(new MozcEngine());

    mock_ = new client::ClientMock();
    mock_->ClearFunctionCounter();
    mozc_engine_->client_.reset(mock_);
  }

  virtual void TearDown() { mozc_engine_.reset(); }

  client::ClientMock* mock_;
  std::unique_ptr<MozcEngine> mozc_engine_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LaunchToolTest);
};

TEST_F(LaunchToolTest, LaunchToolTest) {
  commands::Output output;

  // Launch config dialog
  mock_->ClearFunctionCounter();
  mock_->SetBoolFunctionReturn("LaunchToolWithProtoBuf", true);
  output.set_launch_tool_mode(commands::Output::CONFIG_DIALOG);
  EXPECT_TRUE(mozc_engine_->LaunchTool(output));

  // Launch dictionary tool
  mock_->ClearFunctionCounter();
  mock_->SetBoolFunctionReturn("LaunchToolWithProtoBuf", true);
  output.set_launch_tool_mode(commands::Output::DICTIONARY_TOOL);
  EXPECT_TRUE(mozc_engine_->LaunchTool(output));

  // Launch word register dialog
  mock_->ClearFunctionCounter();
  mock_->SetBoolFunctionReturn("LaunchToolWithProtoBuf", true);
  output.set_launch_tool_mode(commands::Output::WORD_REGISTER_DIALOG);
  EXPECT_TRUE(mozc_engine_->LaunchTool(output));

  // Launch no tool(means do nothing)
  mock_->ClearFunctionCounter();
  mock_->SetBoolFunctionReturn("LaunchToolWithProtoBuf", false);
  output.set_launch_tool_mode(commands::Output::NO_TOOL);
  EXPECT_FALSE(mozc_engine_->LaunchTool(output));

  // Something occurring in client::Client::LaunchTool
  mock_->ClearFunctionCounter();
  mock_->SetBoolFunctionReturn("LaunchToolWithProtoBuf", false);
  output.set_launch_tool_mode(commands::Output::CONFIG_DIALOG);
  EXPECT_FALSE(mozc_engine_->LaunchTool(output));
}

}  // namespace ibus
}  // namespace mozc

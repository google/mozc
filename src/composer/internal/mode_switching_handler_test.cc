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

#include <string>
#include "composer/internal/mode_switching_handler.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"

namespace mozc {
namespace composer {
class ModeSwitchingHandlerTest : public testing::Test {
  void SetUp() {
    handler_ = ModeSwitchingHandler::GetModeSwitchingHandler();
  }
 protected:
  ModeSwitchingHandler *handler_;
};

TEST_F(ModeSwitchingHandlerTest, GetModeSwitchingRule) {
  ModeSwitchingHandler::ModeSwitching display_mode =
      ModeSwitchingHandler::NO_CHANGE;
  ModeSwitchingHandler::ModeSwitching input_mode =
      ModeSwitchingHandler::NO_CHANGE;

  EXPECT_TRUE(handler_->GetModeSwitchingRule("google",
                                             &display_mode, &input_mode));
  EXPECT_EQ(ModeSwitchingHandler::PREFERRED_ALPHANUMERIC, display_mode);
  EXPECT_EQ(ModeSwitchingHandler::REVERT_TO_PREVIOUS_MODE, input_mode);

  EXPECT_TRUE(handler_->GetModeSwitchingRule("Google",
                                             &display_mode, &input_mode));
  EXPECT_EQ(ModeSwitchingHandler::PREFERRED_ALPHANUMERIC, display_mode);
  EXPECT_EQ(ModeSwitchingHandler::REVERT_TO_PREVIOUS_MODE, input_mode);

  EXPECT_TRUE(handler_->GetModeSwitchingRule("http",
                                             &display_mode, &input_mode));
  EXPECT_EQ(ModeSwitchingHandler::HALF_ALPHANUMERIC, display_mode);
  EXPECT_EQ(ModeSwitchingHandler::HALF_ALPHANUMERIC, input_mode);

  EXPECT_TRUE(handler_->GetModeSwitchingRule("www.",
                                             &display_mode, &input_mode));
  EXPECT_EQ(ModeSwitchingHandler::HALF_ALPHANUMERIC, display_mode);
  EXPECT_EQ(ModeSwitchingHandler::HALF_ALPHANUMERIC, input_mode);

  EXPECT_TRUE(handler_->GetModeSwitchingRule("\\\\",
                                             &display_mode, &input_mode));
  EXPECT_EQ(ModeSwitchingHandler::HALF_ALPHANUMERIC, display_mode);
  EXPECT_EQ(ModeSwitchingHandler::HALF_ALPHANUMERIC, input_mode);

  EXPECT_TRUE(handler_->GetModeSwitchingRule("C:\\",
                                             &display_mode, &input_mode));
  EXPECT_EQ(ModeSwitchingHandler::HALF_ALPHANUMERIC, display_mode);
  EXPECT_EQ(ModeSwitchingHandler::HALF_ALPHANUMERIC, input_mode);

  // Normal text should return false.
  EXPECT_FALSE(handler_->GetModeSwitchingRule("foobar",
                                              &display_mode, &input_mode));
  EXPECT_EQ(ModeSwitchingHandler::NO_CHANGE, display_mode);
  EXPECT_EQ(ModeSwitchingHandler::NO_CHANGE, input_mode);
}

TEST_F(ModeSwitchingHandlerTest, DriveLetters) {
  EXPECT_TRUE(handler_->IsDriveLetter("C:\\"));
  EXPECT_TRUE(handler_->IsDriveLetter("c:\\"));
  EXPECT_FALSE(handler_->IsDriveLetter("C:"));
  EXPECT_FALSE(handler_->IsDriveLetter("6:\\"));
}
}  // namespace composer
}  // namespace mozc

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

#include "composer/mode_switching_handler.h"

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace composer {
namespace {

using ::testing::PrintToString;

MATCHER_P2(RuleEq, display_mode, input_mode,
           absl::StrFormat("%s to %s", negation ? "equals" : "doesn't equal",
                           PrintToString(ModeSwitchingHandler::Rule{
                               display_mode, input_mode}))) {
  return arg.display_mode == display_mode && arg.input_mode == input_mode;
}

TEST(ModeSwitchingHandlerTest, GetModeSwitchingRule) {
  ModeSwitchingHandler handler;

  EXPECT_THAT(handler.GetModeSwitchingRule("google"),
              RuleEq(ModeSwitchingHandler::PREFERRED_ALPHANUMERIC,
                     ModeSwitchingHandler::REVERT_TO_PREVIOUS_MODE));

  EXPECT_THAT(handler.GetModeSwitchingRule("Google"),
              RuleEq(ModeSwitchingHandler::PREFERRED_ALPHANUMERIC,
                     ModeSwitchingHandler::REVERT_TO_PREVIOUS_MODE));

  EXPECT_THAT(handler.GetModeSwitchingRule("Chrome"),
              RuleEq(ModeSwitchingHandler::PREFERRED_ALPHANUMERIC,
                     ModeSwitchingHandler::REVERT_TO_PREVIOUS_MODE));

  EXPECT_THAT(handler.GetModeSwitchingRule("chrome"),
              RuleEq(ModeSwitchingHandler::PREFERRED_ALPHANUMERIC,
                     ModeSwitchingHandler::REVERT_TO_PREVIOUS_MODE));

  EXPECT_THAT(handler.GetModeSwitchingRule("Android"),
              RuleEq(ModeSwitchingHandler::PREFERRED_ALPHANUMERIC,
                     ModeSwitchingHandler::REVERT_TO_PREVIOUS_MODE));

  EXPECT_THAT(handler.GetModeSwitchingRule("android"),
              RuleEq(ModeSwitchingHandler::PREFERRED_ALPHANUMERIC,
                     ModeSwitchingHandler::REVERT_TO_PREVIOUS_MODE));

  EXPECT_THAT(handler.GetModeSwitchingRule("http"),
              RuleEq(ModeSwitchingHandler::HALF_ALPHANUMERIC,
                     ModeSwitchingHandler::HALF_ALPHANUMERIC));

  EXPECT_THAT(handler.GetModeSwitchingRule("www."),
              RuleEq(ModeSwitchingHandler::HALF_ALPHANUMERIC,
                     ModeSwitchingHandler::HALF_ALPHANUMERIC));

  EXPECT_THAT(handler.GetModeSwitchingRule("\\\\"),
              RuleEq(ModeSwitchingHandler::HALF_ALPHANUMERIC,
                     ModeSwitchingHandler::HALF_ALPHANUMERIC));

  EXPECT_THAT(handler.GetModeSwitchingRule("C:\\"),
              RuleEq(ModeSwitchingHandler::HALF_ALPHANUMERIC,
                     ModeSwitchingHandler::HALF_ALPHANUMERIC));

  // Normal text should return NO_CHANGE.
  EXPECT_THAT(
      handler.GetModeSwitchingRule("foobar"),
      RuleEq(ModeSwitchingHandler::NO_CHANGE, ModeSwitchingHandler::NO_CHANGE));
}

TEST(ModeSwitchingHandlerTest, IsDriveLetter) {
  EXPECT_TRUE(ModeSwitchingHandler::IsDriveLetter("C:\\"));
  EXPECT_TRUE(ModeSwitchingHandler::IsDriveLetter("c:\\"));
  EXPECT_FALSE(ModeSwitchingHandler::IsDriveLetter("C:"));
  EXPECT_FALSE(ModeSwitchingHandler::IsDriveLetter("6:\\"));
}

}  // namespace
}  // namespace composer
}  // namespace mozc

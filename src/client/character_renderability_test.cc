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

#include "client/character_renderability.h"

#include <vector>

#include "absl/algorithm/container.h"
#include "protocol/commands.pb.h"
#include "testing/gunit.h"

namespace mozc::client {
namespace {

using commands::Request;

TEST(CharacterRenderabilityTest, WindowsVersions) {
  // Windows 10 2004 (Build 19041) only supports Emoji 12.0 (no additional
  // groups)
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForWindows(19041);
    EXPECT_TRUE(groups.empty());
  }

  // Windows 11 21H2 (Build 22000)
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForWindows(22000);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_12_1));
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_13_0));
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_13_1));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_14_0));
  }

  // Windows 11 22H2 (Build 22621)
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForWindows(22621);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_14_0));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_15_0));
  }

  // Windows 11 23H2 (Build 22631)
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForWindows(22631);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_15_0));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_15_1));
  }

  // Windows 11 24H2 (Build 26100)
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForWindows(26100);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_15_1));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_16_0));
  }

  // Windows 11 25H2 (Build 26200)
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForWindows(26200);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_16_0));
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_17_0));
  }
}

TEST(CharacterRenderabilityTest, MacVersions) {
  // macOS 12.0 Monterey
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForMac(12, 0);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_12_1));
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_13_0));
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_13_1));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_14_0));
  }

  // macOS 12.3 Monterey
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForMac(12, 3);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_14_0));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_15_0));
  }

  // macOS 13.3 Ventura
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForMac(13, 3);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_15_0));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_15_1));
  }

  // macOS 14.4 Sonoma
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForMac(14, 4);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_15_1));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_16_0));
  }

  // macOS 15.0 Sequoia
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForMac(15, 0);
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_16_0));
  }

  // macOS 15.4 Sequoia
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForMac(15, 4);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_16_0));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_17_0));
  }

  // macOS 26.0 Tahoe
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForMac(26, 0);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_16_0));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_17_0));
  }

  // macOS 26.4 Tahoe
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForMac(26, 4);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_17_0));
  }
}

TEST(CharacterRenderabilityTest, IosVersions) {
  // iOS 17.0
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForIos(17, 0);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_12_1));
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_13_0));
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_13_1));
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_14_0));
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_15_0));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_15_1));
  }

  // iOS 17.4
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForIos(17, 4);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_15_1));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_16_0));
  }

  // iOS 18.0
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForIos(18, 0);
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_16_0));
  }

  // iOS 18.4
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForIos(18, 4);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_16_0));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_17_0));
  }

  // iOS 26.0
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForIos(26, 0);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_16_0));
    EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_17_0));
  }

  // iOS 26.4
  {
    const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
        CharacterRenderability::GetSupportedGroupsForIos(26, 4);
    EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_17_0));
  }
}

TEST(CharacterRenderabilityTest, LinuxVersions) {
  const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
      CharacterRenderability::GetSupportedGroupsForLinux();
  EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_12_1));
  EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_13_0));
  EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_13_1));
  EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_14_0));
  EXPECT_FALSE(absl::c_contains(groups, Request::EMOJI_15_0));
}

#if defined(__linux__)
TEST(CharacterRenderabilityTest, CurrentPlatformLinux) {
  const std::vector<Request::AdditionalRenderableCharacterGroup> groups =
      CharacterRenderability::GetSupportedGroups();
  EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_12_1));
  EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_13_0));
  EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_13_1));
  EXPECT_TRUE(absl::c_contains(groups, Request::EMOJI_14_0));
}
#endif  // __linux__

}  // namespace
}  // namespace mozc::client

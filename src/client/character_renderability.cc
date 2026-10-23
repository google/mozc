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

#include "base/port.h"
#include "protocol/commands.pb.h"

#if defined(_WIN32)
#include <utility>

#include "base/win32/win_util.h"
#elif defined(__APPLE__)
#include <utility>

#include "base/mac/mac_util.h"
#endif  // _WIN32, __APPLE__

namespace mozc::client {

std::vector<commands::Request::AdditionalRenderableCharacterGroup>
CharacterRenderability::GetSupportedGroupsForWindows(int build_number) {
  std::vector<commands::Request::AdditionalRenderableCharacterGroup> groups;
  // Windows updates introduce newer emoji sets via cumulative updates by
  // incrementing the Update Build Revision (UBR), such as 22000.348
  // (Emoji 13.1), 22621.2070 (Emoji 15.0), and 22621.3810 (Emoji 15.1).
  // However, RtlGetVersion only returns the base build number without UBR.
  // To be safe and prevent displaying tofu on unpatched machines, we require
  // the subsequent major build that is guaranteed to include the emoji set:
  // - 22000 (Win 11 21H2): Emoji 12.1, 13.0, 13.1
  // - 22621 (Win 11 22H2): Emoji 14.0
  // - 22631 (Win 11 23H2): Emoji 15.0 (safe fallback for 22621.2070)
  // - 26100 (Win 11 24H2): Emoji 15.1 (safe fallback for 22621.3810)
  // - 26200 (Win 11 25H2): Emoji 16.0, 17.0 (safe fallback for 26100 updates)
  if (build_number >= 22000) {
    groups.push_back(commands::Request::EMOJI_12_1);
    groups.push_back(commands::Request::EMOJI_13_0);
    groups.push_back(commands::Request::EMOJI_13_1);
  }
  if (build_number >= 22621) {
    groups.push_back(commands::Request::EMOJI_14_0);
  }
  if (build_number >= 22631) {
    groups.push_back(commands::Request::EMOJI_15_0);
  }
  if (build_number >= 26100) {
    groups.push_back(commands::Request::EMOJI_15_1);
  }
  if (build_number >= 26200) {
    groups.push_back(commands::Request::EMOJI_16_0);
    groups.push_back(commands::Request::EMOJI_17_0);
  }
  return groups;
}

std::vector<commands::Request::AdditionalRenderableCharacterGroup>
CharacterRenderability::GetSupportedGroupsForMac(int major, int minor) {
  std::vector<commands::Request::AdditionalRenderableCharacterGroup> groups;
  const int version = major * 10000 + minor;

  // macOS 12.0+ supports Emoji 12.1, 13.0, and 13.1.
  groups.push_back(commands::Request::EMOJI_12_1);
  groups.push_back(commands::Request::EMOJI_13_0);
  groups.push_back(commands::Request::EMOJI_13_1);

  // macOS 12.3+ supports Emoji 14.0.
  if (version >= 120003) {
    groups.push_back(commands::Request::EMOJI_14_0);
  }
  // macOS 13.3+ supports Emoji 15.0.
  if (version >= 130003) {
    groups.push_back(commands::Request::EMOJI_15_0);
  }
  // macOS 14.4+ supports Emoji 15.1.
  if (version >= 140004) {
    groups.push_back(commands::Request::EMOJI_15_1);
  }
  // macOS 15.4+ supports Emoji 16.0.
  if (version >= 150004) {
    groups.push_back(commands::Request::EMOJI_16_0);
  }
  // macOS 26.4+ (Tahoe) supports Emoji 17.0.
  if (version >= 260004) {
    groups.push_back(commands::Request::EMOJI_17_0);
  }
  return groups;
}

std::vector<commands::Request::AdditionalRenderableCharacterGroup>
CharacterRenderability::GetSupportedGroupsForIos(int major, int minor) {
  std::vector<commands::Request::AdditionalRenderableCharacterGroup> groups;
  const int version = major * 10000 + minor;

  // iOS 17.0+ supports Emoji 12.1 through 15.0.
  groups.push_back(commands::Request::EMOJI_12_1);
  groups.push_back(commands::Request::EMOJI_13_0);
  groups.push_back(commands::Request::EMOJI_13_1);
  groups.push_back(commands::Request::EMOJI_14_0);
  groups.push_back(commands::Request::EMOJI_15_0);

  // iOS 17.4+ supports Emoji 15.1.
  if (version >= 170004) {
    groups.push_back(commands::Request::EMOJI_15_1);
  }
  // iOS 18.4+ supports Emoji 16.0.
  if (version >= 180004) {
    groups.push_back(commands::Request::EMOJI_16_0);
  }
  // iOS 26.4+ supports Emoji 17.0.
  if (version >= 260004) {
    groups.push_back(commands::Request::EMOJI_17_0);
  }
  return groups;
}

std::vector<commands::Request::AdditionalRenderableCharacterGroup>
CharacterRenderability::GetSupportedGroupsForLinux() {
  // Background:
  // Unlike Windows or macOS, Linux does not have a single OS-level font update
  // mechanism; emoji rendering capability depends on installed font packages
  // (e.g., fonts-noto-color-emoji). Modern Linux distributions (such as Ubuntu
  // 22.04 LTS and later) widely package Noto Color Emoji supporting at least
  // Emoji 14.0. Therefore, Emoji 12.1 through 14.0 are statically enabled by
  // default as a safe baseline.
  //
  // Future possibilities:
  // 1. Dynamic glyph detection: Check font coverage at runtime using Fontconfig
  //    (FcFontMatch) and FreeType (FT_Get_Char_Index) or HarfBuzz to verify if
  //    canary glyphs for newer emoji versions (e.g., Emoji 15.0+) exist in the
  //    system font.
  // 2. User configuration: Allow users to configure the supported emoji version
  //    via the Mozc GUI config dialog.
  return {
      commands::Request::EMOJI_12_1,
      commands::Request::EMOJI_13_0,
      commands::Request::EMOJI_13_1,
      commands::Request::EMOJI_14_0,
  };
}

std::vector<commands::Request::AdditionalRenderableCharacterGroup>
CharacterRenderability::GetSupportedGroups() {
#if defined(_WIN32)
  const std::pair<int, int> os_version = WinUtil::GetOSVersion();
  return GetSupportedGroupsForWindows(os_version.second);
#elif defined(__APPLE__)
  const std::pair<int, int> os_version = MacUtil::GetOSVersion();
  if constexpr (port::IsIos()) {
    return GetSupportedGroupsForIos(os_version.first, os_version.second);
  } else {
    return GetSupportedGroupsForMac(os_version.first, os_version.second);
  }
#else   // _WIN32, __APPLE__
  if constexpr (port::IsLinux()) {
    return GetSupportedGroupsForLinux();
  }
  return {};
#endif  // _WIN32, __APPLE__
}

}  // namespace mozc::client

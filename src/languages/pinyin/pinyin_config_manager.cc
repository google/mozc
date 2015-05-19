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

#include "languages/pinyin/pinyin_config_manager.h"

#include <pyzy-1.0/PyZyConfig.h>

#include "config/config.pb.h"
#include "languages/pinyin/session_config.h"

namespace mozc {
namespace pinyin {

namespace {
// These magic numbers are extracted from libpyzy.
// TODO(hsumita): Refactors libpyzy to export these values.
const uint32 kCorrectPinyinOption = 0x000001fe;
const uint32 kFuzzyPinyinOption = 0x1fe2aa00;
}  // namespace

void PinyinConfigManager::UpdateWithGlobalConfig(
    const config::PinyinConfig &pinyin_config) {
  PyZy::PinyinConfig &pyzy_config = PyZy::PinyinConfig::instance();

  uint32 conversion_option = pyzy_config.option();

  if (pinyin_config.has_correct_pinyin()) {
    conversion_option = pinyin_config.correct_pinyin()
        ? (conversion_option | kCorrectPinyinOption)
        : (conversion_option & ~kCorrectPinyinOption);
  }
  if (pinyin_config.has_fuzzy_pinyin()) {
    conversion_option = pinyin_config.fuzzy_pinyin()
        ? (conversion_option | kFuzzyPinyinOption)
        : (conversion_option & ~kFuzzyPinyinOption);
  }

  if (conversion_option != pyzy_config.option()) {
    pyzy_config.setOption(conversion_option);
  }
}

void PinyinConfigManager::UpdateWithSessionConfig(
    const SessionConfig &session_config) {
  PyZy::PinyinConfig &pyzy_config = PyZy::PinyinConfig::instance();

  if (pyzy_config.modeSimp() != session_config.simplified_chinese_mode) {
    pyzy_config.setModeSimp(session_config.simplified_chinese_mode);
  }
}

}  // namespace pinyin
}  // namespace mozc

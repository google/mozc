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

#include <pyzy-1.0/PyZyConfig.h>
#include <vector>

#include "config/config.pb.h"
#include "languages/pinyin/pinyin_config_manager.h"
#include "languages/pinyin/session_config.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace pinyin {

TEST(PinyinConfigManagerTest, UpdateWithGlobalConfig) {
  PyZy::PinyinConfig &pyzy_config = PyZy::PinyinConfig::instance();
  config::PinyinConfig pinyin_config;
  vector<uint32> options;

  pinyin_config.set_correct_pinyin(false);
  pinyin_config.set_fuzzy_pinyin(false);
  PinyinConfigManager::UpdateWithGlobalConfig(pinyin_config);
  options.push_back(pyzy_config.option());

  pinyin_config.set_correct_pinyin(false);
  pinyin_config.set_fuzzy_pinyin(true);
  PinyinConfigManager::UpdateWithGlobalConfig(pinyin_config);
  options.push_back(pyzy_config.option());

  pinyin_config.set_correct_pinyin(true);
  pinyin_config.set_fuzzy_pinyin(false);
  PinyinConfigManager::UpdateWithGlobalConfig(pinyin_config);
  options.push_back(pyzy_config.option());

  pinyin_config.set_correct_pinyin(true);
  pinyin_config.set_fuzzy_pinyin(true);
  PinyinConfigManager::UpdateWithGlobalConfig(pinyin_config);
  options.push_back(pyzy_config.option());

  EXPECT_NE(options[0], options[1]);
  EXPECT_NE(options[0], options[2]);
  EXPECT_NE(options[0], options[3]);
  EXPECT_NE(options[1], options[2]);
  EXPECT_NE(options[1], options[3]);
  EXPECT_NE(options[2], options[3]);

  pinyin_config.set_correct_pinyin(false);
  pinyin_config.set_fuzzy_pinyin(false);
  PinyinConfigManager::UpdateWithGlobalConfig(pinyin_config);
  EXPECT_EQ(options[0], pyzy_config.option());
}

TEST(PinyinConfigManagerTest, UpdateWithLocalConfig) {
  PyZy::PinyinConfig &pyzy_config = PyZy::PinyinConfig::instance();
  SessionConfig session_config;

  session_config.simplified_chinese_mode = true;
  PinyinConfigManager::UpdateWithSessionConfig(session_config);
  EXPECT_TRUE(pyzy_config.modeSimp());

  session_config.simplified_chinese_mode = false;
  PinyinConfigManager::UpdateWithSessionConfig(session_config);
  EXPECT_FALSE(pyzy_config.modeSimp());
}

}  // namespace pinyin
}  // namespace mozc

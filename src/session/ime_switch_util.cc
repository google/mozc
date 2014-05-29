// Copyright 2010-2014, Google Inc.
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

#include "session/ime_switch_util.h"

#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "base/port.h"
#include "base/singleton.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/key_info_util.h"

namespace mozc {
namespace config {
namespace {

class ImeSwitchUtilImpl {
 public:
  ImeSwitchUtilImpl() {
    Reload();
  }

  bool IsDirectModeCommand(const commands::KeyEvent &key) const {
    return KeyInfoUtil::ContainsKey(direct_mode_keys_, key);
  }

  void Reload() {
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    direct_mode_keys_ = KeyInfoUtil::ExtractSortedDirectModeKeys(config);
  }

 private:
  vector<KeyInformation> direct_mode_keys_;

  DISALLOW_COPY_AND_ASSIGN(ImeSwitchUtilImpl);
};

}  // namespace

bool ImeSwitchUtil::IsDirectModeCommand(const commands::KeyEvent &key) {
  return Singleton<ImeSwitchUtilImpl>::get()->IsDirectModeCommand(key);
}

void ImeSwitchUtil::Reload() {
  Singleton<ImeSwitchUtilImpl>::get()->Reload();
}

}  // namespace config
}  // namespace mozc

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

#include "session/internal/keymap_factory.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/port.h"
#include "protocol/config.pb.h"
#include "session/internal/keymap.h"

namespace mozc {
namespace keymap {
namespace {

using config::Config;

}  // namespace

KeyMapManager *KeyMapFactory::GetKeyMapManager(
    const Config::SessionKeymap keymap) {
  KeyMapManagerList& keymaps = GetKeyMaps();
  KeyMapManagerList::iterator iter =
      std::find_if(keymaps.begin(), keymaps.end(),
                   [&keymap](std::unique_ptr<KeyMapManager> const& m) {
                     return m->GetKeymap() == keymap;
                   });

  if (iter != keymaps.end()) {
    return iter->get();
  }

  // create new instance
  return keymaps.emplace_back(new KeyMapManager(keymap)).get();
}

void KeyMapFactory::ReloadConfig(const Config &config) {
  KeyMapManagerList& keymaps = GetKeyMaps();
  // TODO(matsuzakit): Special handling for CUSTOM will soon be removed.
  KeyMapManagerList::iterator iter =
      std::find_if(keymaps.begin(), keymaps.end(),
                   [](std::unique_ptr<KeyMapManager> const& m) {
                     return m->GetKeymap() == Config::CUSTOM;
                   });
  if (iter == keymaps.end()) {
    return;
  }

  (*iter)->ReloadConfig(config);
}

KeyMapFactory::KeyMapManagerList& KeyMapFactory::GetKeyMaps() {
  static KeyMapManagerList* keymaps = new KeyMapFactory::KeyMapManagerList();
  return *keymaps;
}

}  // namespace keymap
}  // namespace mozc

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

#include "session/internal/keymap_factory.h"

#include <map>

#include "base/base.h"
#include "base/freelist.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/internal/keymap.h"

namespace mozc {
namespace keymap {

// static member variable
ObjectPool<KeyMapManager> KeyMapFactory::pool_(6);
KeyMapFactory::KeyMapManagerMap KeyMapFactory::keymaps_;

KeyMapManager *KeyMapFactory::GetKeyMapManager(
    config::Config::SessionKeymap keymap) {
  map<config::Config::SessionKeymap, KeyMapManager *>::iterator iter =
      keymaps_.find(keymap);

  if (iter == keymaps_.end()) {
    // create new instance
    KeyMapManager *manager = pool_.Alloc();
    iter = keymaps_.insert(make_pair(keymap, manager)).first;
  }

  iter->second->ReloadWithKeymap(keymap);

  return iter->second;
}

}  // namespace keymap
}  // namespace mozc

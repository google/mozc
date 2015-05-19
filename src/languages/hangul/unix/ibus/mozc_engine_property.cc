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

#include <ibus.h>

#include "unix/ibus/mozc_engine_property.h"

#include "base/base.h"
#include "session/commands.pb.h"

namespace mozc {
namespace ibus {

// The list of properties used in ibus-mozc-hangul.
// In ChromeOS, we do not use toggle interface, because ChromeOS does not
// support toggle interface.
const MozcEngineProperty kMozcEngineProperties[] = {
  {
    // Treat commands::HIRAGANA as Hangul mode.
    commands::HIRAGANA,
    "CompositionMode.Hangul",
    "Hangul mode",
    "\xED\x95\x9C",  // "한"
    "hiragana.png",
  }, {
    // Treat commands::FULL_ASCII as Hanja lock mode.
    commands::FULL_ASCII,
    "CompositionMode.Hanja",
    "Hanja mode",
    "\xED\x95\x9C",  // "한" TODO(nona) replace more suitable symbol.
    "alpha_full.png",
  },
};

// The IMEOff state is not available in Hangul
const MozcEngineProperty *kMozcEnginePropertyIMEOffState = NULL;
const size_t kMozcEnginePropertiesSize = arraysize(kMozcEngineProperties);

const commands::CompositionMode kMozcEngineInitialCompositionMode =
    commands::HIRAGANA;

const MozcEngineSwitchProperty kMozcEngineSwitchProperties[] = {};
const size_t kMozcEngineSwitchPropertiesSize =
    ARRAYSIZE(kMozcEngineSwitchProperties);

// TODO(nona) : Fixed for hangule input. This is copied from
// chewing/unix/ibu/mozc_engine_property.cc.
const MozcEngineToolProperty kMozcEngineToolProperties[] = {
  {
    "Tool.ConfigDialog",
    "config_dialog",
    "Properties",
    "properties.png",
  }, {
    "Tool.DictionaryTool",
    "dictionary_tool",
    "Dictionary tool",
    "dictionary.png",
  }, {
    "Tool.AboutDialog",
    "about_dialog",
    "About Mozc",
    NULL,
  },
};

const size_t kMozcEngineToolPropertiesSize =
    arraysize(kMozcEngineToolProperties);

// Ibus lookup up table size (that is, the number of candidates per page).
// TODO(nona) make this variable editable in config.
const unsigned int kPageSize = 10;
}  // namespace ibus
}  // namespace mozc

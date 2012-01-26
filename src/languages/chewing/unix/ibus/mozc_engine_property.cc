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

// The list of properties used in ibus-mozc.
const MozcEngineProperty kMozcEngineProperties[] = {
  {
    commands::HALF_ASCII,
    "CompositionMode.English",
    "English",
    "A",
    "direct.png",
  },
  // We use 'HIRAGANA' mode for the chewing Chinese input mode.
  // TODO(mukai): add CHEWING mode and fix here.
  {
    commands::HIRAGANA,
    "CompositionMode.Chinese",
    "Chinese",
    "\xe9\x85\xB7",  //U+9177 "chewing" kanji
    "hiragana.png",
  },
  {
    commands::FULL_ASCII,
    "CompositionMode.WideLatin",
    "Full-width English",
    "\xef\xbc\xa1",  // Full width ASCII letter A
    "alpha_full.png",
  },
};

// The IMEOff state is not available in Chewing.
const MozcEngineProperty *kMozcEnginePropertyIMEOffState = NULL;
const size_t kMozcEnginePropertiesSize = arraysize(kMozcEngineProperties);

const commands::CompositionMode kMozcEngineInitialCompositionMode =
    commands::HIRAGANA;

// TODO(mukai): Fix the following properties when we suppport
// non-ChromeOS Linux.
const MozcEngineToolProperty kMozcEngineToolProperties[] = {
  {
    "Tool.ConfigDialog",
    "config_dialog",
    "Property",
    "properties.png",
  },
  {
    "Tool.DictionaryTool",
    "dictionary_tool",
    "Dictionary tool",
    "dictionary.png",
  },
  {
    "Tool.AboutDialog",
    "about_dialog",
    "About Mozc",
    NULL,
  },
};

const size_t kMozcEngineToolPropertiesSize =
    arraysize(kMozcEngineToolProperties);

const unsigned int kPageSize = 10;
}  // namespace ibus
}  // namespace mozc

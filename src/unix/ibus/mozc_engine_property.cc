// Copyright 2010-2020, Google Inc.
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

#include "unix/ibus/mozc_engine_property.h"

#include "base/port.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace ibus {

namespace {
// The list of properties used in ibus-mozc.
const MozcEngineProperty kMozcEnginePropertiesArray[] = {
    {
        commands::DIRECT,
        "InputMode.Direct",
        "Direct input",
        "A",
        "direct.png",
    },
    {
        commands::HIRAGANA,
        "InputMode.Hiragana",
        "Hiragana",
        "あ",
        "hiragana.png",
    },
    {
        commands::FULL_KATAKANA,
        "InputMode.Katakana",
        "Katakana",
        "ア",
        "katakana_full.png",
    },
    {
        commands::HALF_ASCII,
        "InputMode.Latin",
        "Latin",
        "_A",
        "alpha_half.png",
    },
    {
        commands::FULL_ASCII,
        "InputMode.WideLatin",
        "Wide Latin",
        "Ａ",
        "alpha_full.png",
    },
    {
        commands::HALF_KATAKANA,
        "InputMode.HalfWidthKatakana",
        "Half width katakana",
        "_ｱ",
        "katakana_half.png",
    },
};

const MozcEngineToolProperty kMozcEngineToolPropertiesArray[] = {
    {
        "Tool.ConfigDialog",
        "config_dialog",
        "Properties",
        "properties.png",
    },
    {
        "Tool.DictionaryTool",
        "dictionary_tool",
        "Dictionary Tool",
        "dictionary.png",
    },
    {
        "Tool.WordRegisterDialog",
        "word_register_dialog",
        "Add Word",
        "word_register.png",
    },
    {
        "Tool.AboutDialog",
        "about_dialog",
        "About Mozc",
        nullptr,
    },
};
}  // namespace

// The list of properties used in ibus-mozc.
const MozcEngineProperty *kMozcEngineProperties =
    &kMozcEnginePropertiesArray[0];

const MozcEngineProperty *kMozcEnginePropertyIMEOffState =
    &kMozcEngineProperties[0];
const size_t kMozcEnginePropertiesSize = arraysize(kMozcEnginePropertiesArray);
static_assert(commands::NUM_OF_COMPOSITIONS == kMozcEnginePropertiesSize,
              "commands::NUM_OF_COMPOSITIONS must be the property size.");
const commands::CompositionMode kMozcEngineInitialCompositionMode =
    commands::HIRAGANA;

const MozcEngineToolProperty *kMozcEngineToolProperties =
    &kMozcEngineToolPropertiesArray[0];
const size_t kMozcEngineToolPropertiesSize =
    arraysize(kMozcEngineToolPropertiesArray);

const unsigned int kPageSize = 9;
}  // namespace ibus
}  // namespace mozc

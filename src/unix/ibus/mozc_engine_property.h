// Copyright 2010-2013, Google Inc.
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

#ifndef MOZC_UNIX_IBUS_MOZC_ENGINE_PROPERTY_H_
#define MOZC_UNIX_IBUS_MOZC_ENGINE_PROPERTY_H_

#include "session/commands.pb.h"

namespace mozc {
namespace ibus {

// The list of properties used in ibus-mozc.
struct MozcEngineProperty {
  commands::CompositionMode composition_mode;
  const char *key;  // IBus property key for the mode.
  const char *label;  // text for the radio menu (ibus-anthy compatible).
  const char *label_for_panel;  // text for the language panel.
  const char *icon;
};

// This pointer should be NULL when properties size is 0.
extern const MozcEngineProperty *kMozcEngineProperties;
extern const size_t kMozcEnginePropertiesSize;

// If kMozcEnginePropertiesIMEOffState is NULL, it means IME should be always
// On.
extern const MozcEngineProperty *kMozcEnginePropertyIMEOffState;

extern const commands::CompositionMode kMozcEngineInitialCompositionMode;

struct MozcEngineSwitchProperty {
  // Specifies the command id to trigger.
  commands::SessionCommand::LanguageBarCommandId id;
  const char *key;      // IBus property key.
  const char *label;    // text for the menu.
  const char *icon;     // icon.
  const char *tooltip;  // tooltip.
};

// This pointer should be NULL when properties size is 0.
extern const MozcEngineSwitchProperty *kMozcEngineSwitchProperties;
extern const size_t kMozcEngineSwitchPropertiesSize;

struct MozcEngineToolProperty {
  const char *key;    // IBus property key for the MozcTool.
  const char *mode;   // command line passed as --mode=
  const char *label;  // text for the menu.
  const char *icon;   // icon
};

// This pointer should be NULL when properties size is 0.
extern const MozcEngineToolProperty *kMozcEngineToolProperties;
extern const size_t kMozcEngineToolPropertiesSize;

extern const unsigned int kPageSize;
}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_MOZC_ENGINE_PROPERTY_H_

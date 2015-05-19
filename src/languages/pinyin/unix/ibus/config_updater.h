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

// We don't use the config updating logic for Chrome OS in mozc_engine.cc.
// Rather we use another one and invoke it from our main.cc.

#ifndef MOZC_LANGUAGES_PINYIN_UNIX_IBUS_CONFIG_UPDATER_H_
#define MOZC_LANGUAGES_PINYIN_UNIX_IBUS_CONFIG_UPDATER_H_

#include <ibus.h>
#include <map>
#include <string>

#include "config/config.pb.h"

namespace mozc {
namespace pinyin {
#ifdef OS_CHROMEOS
class ConfigUpdater {
 public:
  ConfigUpdater();

  static void ConfigValueChanged(IBusConfig *config,
                                 const gchar *section,
                                 const gchar *name,
#if IBUS_CHECK_VERSION(1, 3, 99)
                                 GVariant *value,
#else
                                 GValue *value,
#endif  // IBUS_CHECK_VERSION
                                 gpointer user_data);
  void UpdateConfig(const gchar *section,
                    const gchar *name,
#if IBUS_CHECK_VERSION(1, 3, 99)
                    GVariant *value
#else
                    GValue *value
#endif  // IBUS_CHECK_VERSION
                    );

  // Initializes mozc pinyin config.
  static void InitConfig(IBusConfig *config);

 private:
  const map<string, const char*>& name_to_field();

  map<string, const char *> name_to_field_;
};
#endif  // OS_CHROMEOS
}  // namespace pinyin
}  // namespace mozc

#endif  // MOZC_LANGUAGES_PINYIN_UNIX_IBUS_CONFIG_UPDATER_H_

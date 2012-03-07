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

// Auxiliary functions to handling config value updates of iBus.

#ifndef MOZC_UNIX_IBUS_CONFIG_UTIL_H_
#define MOZC_UNIX_IBUS_CONFIG_UTIL_H_

#include <ibus.h>

#include <map>
#include <string>

#include "base/base.h"
#include "base/protobuf/message.h"

namespace mozc {
namespace ibus {
// The following class is only used for Chrome OS.  The reason is that, in
// Linux, configuration is modified by Mozc's own GUI tool which does
// not rely on IBus and not by IBus signal. In ChromeOS, configuration is
// modified by ChromeOS configuration page and IBus send signal to each IME.
// Each IME read IBus's memconf and update their configuration.
class ConfigUtil {
 public:
#if IBUS_CHECK_VERSION(1, 3, 99)
  static bool GetString(GVariant *value, const gchar **out_string);
  static bool GetInteger(GVariant *value, gint *out_integer);
  static bool GetBoolean(GVariant *value, gint *out_boolean);
#else
  static bool GetString(GValue *value, const gchar **out_string);
  static bool GetInteger(GValue *value, gint *out_integer);
  static bool GetBoolean(GValue *value, gint *out_boolean);
#endif

  // Find the related field in |result| message by |name| and update
  // its value to |value|.  This function does not take ownership of
  // the |value|.
  static bool SetFieldForName(const gchar *name,
#if IBUS_CHECK_VERSION(1, 3, 99)
                              GVariant *value,
#else
                              GValue *value,
#endif
                              protobuf::Message *result);

#ifdef OS_CHROMEOS
  // Load config from ibus-memconf.
  static void InitConfig(IBusConfig* config,
                         const char *section_name,
                         const map<string, const char*> &name_to_field);
#endif

 private:
  // Disallow instantiation
  ConfigUtil() {}

  DISALLOW_COPY_AND_ASSIGN(ConfigUtil);
};
}  // namespace ibus
}  // namespace mozc

#endif

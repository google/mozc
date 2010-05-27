// Copyright 2010, Google Inc.
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

#include <cstdio>

#include <ibus.h>

#include "base/base.h"
#include "base/version.h"
#include "unix/ibus/mozc_engine.h"
#include "unix/ibus/path_util.h"

DEFINE_bool(ibus, false, "The engine is started by ibus-daemon");
DEFINE_bool(xml, false, "Dump supported engines as XML");

#ifdef OS_CHROMEOS
static const bool kEnableUsEngineDefault = true;
#else
static const bool kEnableUsEngineDefault = false;
#endif
DEFINE_bool(enable_us_engine,
            kEnableUsEngineDefault, "Enable Mozc engine for a US keyboard");

namespace {

IBusBus *g_bus = NULL;

const char kComponentName[] = "com.google.IBus.Mozc";
const char kComponentDescription[] = "Mozc Component";
const char kLicense[] = "New BSD";
const char kAuthor[] = "Google Inc.";
const char kHomePage[] = "http://code.google.com/p/mozc/";
const char kTextDomain[] = "ibus-mozc";
const char *kEngineLongNames[] = {
  "Mozc (US keyboard layout)",
#ifdef OS_CHROMEOS
  "Mozc (Japanese keyboard layout)",
#else
  "Mozc",
#endif
};
const char kEngineDescription[] = "Mozc Japanese input method";
const char kEngineIcon[] = "product_icon.png";
const char kEngineLanguage[] = "ja";
#ifdef OS_CHROMEOS
// TODO(yusukes): Check if Kana key in the Chrome OS Japanese keyboard actually
// generates the Hiragana_Katakana keysym on "jp" XKB layout when the keyboard
// gets ready.
const char kEngineHotkeys[] = "Hiragana_Katakana";
#endif
const size_t kEngineNamesLen = arraysize(mozc::ibus::kEngineNames);
COMPILE_ASSERT(kEngineNamesLen == arraysize(kEngineLongNames), bad_array_size);

#ifndef OS_CHROMEOS
void IgnoreSigChild() {
  // Don't wait() child process termination.
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  ::sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  CHECK_EQ(0, ::sigaction(SIGCHLD, &sa, NULL));
  // TODO(taku): move this function inside client::Session::LaunchTool
}
#endif

// Creates a IBusComponent object and add engine(s) to the object.
IBusComponent *GetIBusComponent() {
  IBusComponent *component = ibus_component_new(
      kComponentName,
      kComponentDescription,
      mozc::Version::GetMozcVersion().c_str(),
      kLicense,
      kAuthor,
      kHomePage,
      "",
      kTextDomain);
  const string &icon_path = mozc::ibus::GetIconPath(kEngineIcon);

  for (size_t i = 0; i < kEngineNamesLen; ++i) {
    if (!FLAGS_enable_us_engine &&
        !strcmp(mozc::ibus::kEngineLayouts[i], "us")) {
      continue;
    }

    ibus_component_add_engine(
        component,
#ifdef OS_CHROMEOS
        // On Chrome OS, we use "Kana" key for selecting mozc or mozc-jp engines
        // unconditionally (i.e. no-op if mozc is already active).
        // Note that the ibus_engine_desc_new2 API is Chrome OS specific as of
        // writing.
        ibus_engine_desc_new2(mozc::ibus::kEngineNames[i],
                              kEngineLongNames[i],
                              kEngineDescription,
                              kEngineLanguage,
                              kLicense,
                              kAuthor,
                              icon_path.c_str(),
                              mozc::ibus::kEngineLayouts[i],
                              kEngineHotkeys));
#else
        ibus_engine_desc_new(mozc::ibus::kEngineNames[i],
                             kEngineLongNames[i],
                             kEngineDescription,
                             kEngineLanguage,
                             kLicense,
                             kAuthor,
                             icon_path.c_str(),
                             mozc::ibus::kEngineLayouts[i]));
#endif
  }
  return component;
}

// Initializes ibus components and adds Mozc engine.
void InitIBusComponent(bool executed_by_ibus_daemon) {
  g_bus = ibus_bus_new();
  g_signal_connect(g_bus,
                   "disconnected",
                   G_CALLBACK(mozc::ibus::MozcEngine::Disconnected),
                   NULL);

  IBusComponent *component = GetIBusComponent();
  IBusFactory *factory = ibus_factory_new(ibus_bus_get_connection(g_bus));
  GList *engines = ibus_component_get_engines(component);
  for (GList *p = engines; p; p = p->next) {
    IBusEngineDesc *engine = reinterpret_cast<IBusEngineDesc*>(p->data);
    ibus_factory_add_engine(
        factory, engine->name, mozc::ibus::MozcEngine::GetType());
  }

  if (executed_by_ibus_daemon) {
    ibus_bus_request_name(g_bus, kComponentName, 0);
  } else {
    ibus_bus_register_component(g_bus, component);
  }
  g_object_unref(component);
}

}  // namespace

int main(gint argc, gchar **argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  ibus_init();
  if (FLAGS_xml) {
    GString *xml = g_string_new("");
    IBusComponent *component = GetIBusComponent();
    ibus_component_output_engines(component, xml, 0);
    printf("%s", xml->str);
    fflush(stdout);
  } else {
    InitIBusComponent(FLAGS_ibus);
#ifndef OS_CHROMEOS
    IgnoreSigChild();
#endif
    ibus_main();
  }
  return 0;
}

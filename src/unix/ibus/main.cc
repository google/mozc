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

#include "unix/ibus/main.h"

#include <cstddef>
#include <cstdio>

#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/version.h"
#include "unix/ibus/ibus_config.h"
#include "unix/ibus/mozc_engine.h"
#include "unix/ibus/path_util.h"
#include "absl/flags/flag.h"

ABSL_FLAG(bool, ibus, false, "The engine is started by ibus-daemon");
ABSL_FLAG(bool, xml, false, "Output xml data for the engine.");

namespace {

IBusBus *g_bus = nullptr;

#ifndef MOZC_NO_LOGGING
void EnableVerboseLog() {
  const int kDefaultVerboseLevel = 1;
  if (mozc::Logging::GetVerboseLevel() < kDefaultVerboseLevel) {
    mozc::Logging::SetVerboseLevel(kDefaultVerboseLevel);
  }
}
#endif  // MOZC_NO_LOGGING

void IgnoreSigChild() {
  // Don't wait() child process termination.
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  ::sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  CHECK_EQ(0, ::sigaction(SIGCHLD, &sa, nullptr));
  // TODO(taku): move this function inside client::Session::LaunchTool
}

// Creates a IBusComponent object and add engine(s) to the object.
IBusComponent *GetIBusComponent() {
  IBusComponent *component = ibus_component_new(
      kComponentName, kComponentDescription,
      mozc::Version::GetMozcVersion().c_str(), kComponentLicense,
      kComponentAuthor, kComponentHomepage, "", kComponentTextdomain);
  const std::string icon_path = mozc::ibus::GetIconPath(kEngineIcon);

  mozc::IbusConfig ibus_config;
  ibus_config.InitEnginesXml();
  for (const mozc::ibus::Engine &engine : ibus_config.GetConfig().engines()) {
    ibus_component_add_engine(
        component,
        ibus_engine_desc_new(engine.name().c_str(), engine.longname().c_str(),
                             kEngineDescription, kEngineLanguage,
                             kComponentLicense, kComponentAuthor,
                             icon_path.c_str(), engine.layout().c_str()));
  }
  return component;
}

// Initializes ibus components and adds Mozc engine.
void InitIBusComponent(bool executed_by_ibus_daemon) {
  g_bus = ibus_bus_new();
  g_signal_connect(g_bus, "disconnected",
                   G_CALLBACK(mozc::ibus::MozcEngine::Disconnected), nullptr);

  IBusComponent *component = GetIBusComponent();
  IBusFactory *factory = ibus_factory_new(ibus_bus_get_connection(g_bus));
  GList *engines = ibus_component_get_engines(component);
  for (GList *p = engines; p; p = p->next) {
    IBusEngineDesc *engine = reinterpret_cast<IBusEngineDesc *>(p->data);
    const gchar *const engine_name = ibus_engine_desc_get_name(engine);
    ibus_factory_add_engine(factory, engine_name,
                            mozc::ibus::MozcEngine::GetType());
  }

  if (executed_by_ibus_daemon) {
    ibus_bus_request_name(g_bus, kComponentName, 0);
  } else {
    ibus_bus_register_component(g_bus, component);
  }
  g_object_unref(component);
}

void OutputXml() {
  mozc::IbusConfig ibus_config;
  std::cout << ibus_config.InitEnginesXml() << std::endl;
}

}  // namespace

int main(gint argc, gchar **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  if (absl::GetFlag(FLAGS_xml)) {
    OutputXml();
    return 0;
  }

  ibus_init();
  InitIBusComponent(absl::GetFlag(FLAGS_ibus));
#ifndef MOZC_NO_LOGGING
  EnableVerboseLog();
#endif  // MOZC_NO_LOGGING
  IgnoreSigChild();
  ibus_main();
  return 0;
}

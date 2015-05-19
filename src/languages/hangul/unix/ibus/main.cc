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

// This file is copied from chewing/unix/ibus/main.cc.

#include <ibus.h>
#include <cstdio>

#include "base/base.h"
#include "base/logging.h"
#include "base/version.h"
#include "config/config_handler.h"
#include "languages/hangul/hangul_session_factory.h"
#include "languages/hangul/session.h"
#include "languages/hangul/unix/ibus/config_updater.h"
#include "languages/hangul/unix/ibus/main.h"
#include "session/session_factory_manager.h"
#include "unix/ibus/config_util.h"
#include "unix/ibus/mozc_engine.h"
#include "unix/ibus/path_util.h"

DEFINE_bool(ibus, false, "The engine is started by ibus-daemon");

namespace {

IBusBus *g_bus = NULL;
#ifdef OS_CHROMEOS
// We use the ibus configuration daemon only on Chromium OS.
IBusConfig *g_config = NULL;
#endif  // OS_CHROMEOS

// Creates a IBusComponent object and add engine(s) to the object.
IBusComponent *GetIBusComponent() {
  IBusComponent *component = ibus_component_new(
      kComponentName,
      kComponentDescription,
      mozc::Version::GetMozcVersion().c_str(),
      kComponentLicense,
      kComponentAuthor,
      kComponentHomepage,
      "",
      kComponentTextdomain);
  const string icon_path = mozc::ibus::GetIconPath(kEngineIcon);
  for (size_t i = 0; i < kEngineArrayLen; ++i) {
    ibus_component_add_engine(component,
                              ibus_engine_desc_new(kEngineNameArray[i],
                                                   kEngineLongnameArray[i],
                                                   kEngineDescription,
                                                   kEngineLanguage,
                                                   kComponentLicense,
                                                   kComponentAuthor,
                                                   icon_path.c_str(),
                                                   kEngineLayoutArray[i]));
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
#ifdef OS_CHROMEOS
  g_config = ibus_bus_get_config(g_bus);
  g_object_ref_sink(g_config);
  g_signal_connect(g_config,
                   "value-changed",
                   G_CALLBACK(mozc::hangul::ConfigUpdater::ConfigValueChanged),
                   NULL);
#endif  // OS_CHROMEOS
  IBusComponent *component = GetIBusComponent();
  IBusFactory *factory = ibus_factory_new(ibus_bus_get_connection(g_bus));
  GList *engines = ibus_component_get_engines(component);
  for (GList *p = engines; p; p = p->next) {
    IBusEngineDesc *engine = reinterpret_cast<IBusEngineDesc*>(p->data);
#if IBUS_CHECK_VERSION(1, 3, 99)
    const gchar * const engine_name = ibus_engine_desc_get_name(engine);
#else
    const gchar * const engine_name = engine->name;
#endif  // IBUS_CHECK_VERSION
    ibus_factory_add_engine(
        factory, engine_name, mozc::ibus::MozcEngine::GetType());
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
#ifdef OS_CHROMEOS
  mozc::hangul::HangulSessionFactory session_factory;
  mozc::session::SessionFactoryManager::SetSessionFactory(&session_factory);
#endif  // OS_CHROMEOS
  ibus_init();
  InitIBusComponent(FLAGS_ibus);
#ifdef OS_CHROMEOS
  mozc::config::ConfigHandler::SetConfigFileName("memory://hangul_config.1.db");
  mozc::hangul::ConfigUpdater::InitConfig(g_config);
#endif  // OS_CHROMEOS
  ibus_main();

#ifdef OS_CHROMEOS
  if (g_config) {
    g_object_unref(g_config);
  }
#endif  // OS_CHROMEOS

  return 0;
}

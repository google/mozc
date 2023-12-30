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
#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/version.h"
#include "unix/ibus/engine_registrar.h"
#include "unix/ibus/ibus_config.h"
#include "unix/ibus/mozc_engine.h"
#include "unix/ibus/path_util.h"

ABSL_FLAG(bool, ibus, false, "The engine is started by ibus-daemon");
ABSL_FLAG(bool, xml, false, "Output xml data for the engine.");

namespace mozc {
namespace ibus {
namespace {

void EnableVerboseLog() {
#ifndef MOZC_NO_LOGGING
  constexpr int kDefaultVerboseLevel = 1;
  if (mozc::Logging::GetVerboseLevel() < kDefaultVerboseLevel) {
    mozc::Logging::SetVerboseLevel(kDefaultVerboseLevel);
  }
#endif  // MOZC_NO_LOGGING
}

void IgnoreSigChild() {
  // Don't wait() child process termination.
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  ::sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  CHECK_EQ(::sigaction(SIGCHLD, &sa, nullptr), 0);
  // TODO(taku): move this function inside client::Session::LaunchTool
}

// Callback function to the "disconnected" signal to the bus object.
void OnDisconnected(IBusBus *bus, void *null_data) { IbusWrapper::Quit(); }

// Creates a IBusComponent object and add engine(s) to the object.
IbusComponentWrapper GetIbusComponent() {
  IbusComponentWrapper component(kComponentName, kComponentDescription,
                                 mozc::Version::GetMozcVersion(),
                                 kComponentLicense, kComponentAuthor,
                                 kComponentHomepage, "", kComponentTextdomain);
  const std::string icon_path = GetIconPath(kEngineIcon);

  mozc::IbusConfig ibus_config;
  ibus_config.Initialize();
  for (const Engine &engine : ibus_config.GetConfig().engines()) {
    component.AddEngine(engine.name(), engine.longname(), kEngineDescription,
                        kEngineLanguage, kComponentLicense, kComponentAuthor,
                        icon_path, engine.layout());
  }
  return component;
}

// Initializes ibus components and adds Mozc engine.
void InitIbusComponent(IbusBusWrapper *bus, MozcEngine *engine,
                       bool executed_by_ibus_daemon) {
  // Set callback on disconnected from Ibus.
  constexpr void *null_data = nullptr;
  bus->SignalConnect("disconnected", OnDisconnected, null_data);

  // Bind engine specifications in the user configuration to Ibus engine object.
  IbusComponentWrapper component = GetIbusComponent();
  const GType type_id = RegisterEngine(engine);
  bus->AddEngines(component.GetEngineNames(), type_id);

  if (executed_by_ibus_daemon) {
    bus->RequestName(kComponentName);
  } else {
    bus->RegisterComponent(&component);
  }
  component.Unref();
}

void OutputXml() {
  IbusConfig ibus_config;
  ibus_config.Initialize();
  std::cout << ibus_config.GetEnginesXml() << std::endl;
}

void RunIbus() {
  IbusWrapper::Init();
  IbusBusWrapper bus;
  MozcEngine engine;
  InitIbusComponent(&bus, &engine, absl::GetFlag(FLAGS_ibus));
  EnableVerboseLog();  // Do nothing if MOZC_NO_LOGGING is defined.
  IgnoreSigChild();
  IbusWrapper::Main();
}

}  // namespace
}  // namespace ibus
}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  if (absl::GetFlag(FLAGS_xml)) {
    mozc::ibus::OutputXml();
    return 0;
  }
  mozc::ibus::RunIbus();
  return 0;
}

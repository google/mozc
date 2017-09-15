/*
* Copyright (C) 2017~2017 by CSSlayer
* wengxt@gmail.com
*
* This library is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as
* published by the Free Software Foundation; either version 2 of the
* License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; see the file COPYING. If not,
* see <http://www.gnu.org/licenses/>.
*/

#include "unix/fcitx5/mozc_engine.h"

#include <fcitx-utils/i18n.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <vector>

#include "base/init_mozc.h"
#include "unix/fcitx5/mozc_connection.h"
#include "unix/fcitx5/mozc_response_parser.h"

namespace fcitx {

Instance *Init(Instance *instance) {
  int argc = 1;
  char argv0[] = "fcitx_mozc";
  char *_argv[] = {argv0};
  char **argv = _argv;
  mozc::InitMozc(argv[0], &argc, &argv, true);
  return instance;
}

MozcEngine::MozcEngine(Instance *instance)
    : instance_(Init(instance)),
      connection_(std::make_unique<MozcConnection>()),
      factory_([this](InputContext &ic) {
        return new MozcState(&ic, connection_->CreateClient(), this);
      }) {
  instance_->inputContextManager().registerProperty("mozcState", &factory_);
}

MozcEngine::~MozcEngine() {}

vector<InputMethodEntry> MozcEngine::listInputMethods() {
  vector<InputMethodEntry> result;
  result.push_back(move(
      InputMethodEntry("mozc", _("Mozc Japanese Input Method"), "ja_JP", "mozc")
          .setIcon("mozc")
          .setLabel("あ")));
  return result;
}

void MozcEngine::reloadConfig() {}
void MozcEngine::activate(const fcitx::InputMethodEntry &entry,
                          fcitx::InputContextEvent &event) {
  auto mozc_state = mozcState(event.inputContext());
  mozc_state->FocusIn();
}
void MozcEngine::deactivate(const fcitx::InputMethodEntry &entry,
                            fcitx::InputContextEvent &event) {
  auto mozc_state = mozcState(event.inputContext());
  mozc_state->FocusOut();
}
void MozcEngine::keyEvent(const InputMethodEntry &entry, KeyEvent &event) {
  auto mozc_state = mozcState(event.inputContext());

  // TODO: check layout
  if (mozc_state->ProcessKeyEvent(event.rawKey().sym(), event.rawKey().code(),
                                  event.rawKey().states(), false,
                                  event.isRelease())) {
    event.filterAndAccept();
  }
}

void MozcEngine::reset(const InputMethodEntry &, InputContextEvent &event) {
  auto mozc_state = mozcState(event.inputContext());
  mozc_state->Reset();
}

void MozcEngine::save() {}

MozcState *MozcEngine::mozcState(InputContext *ic) {
  return ic->propertyFor(&factory_);
}

AddonInstance *MozcEngine::clipboardAddon() { return clipboard(); }
}

FCITX_ADDON_FACTORY(fcitx::MozcEngineFactory)

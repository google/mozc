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
#include <fcitx-utils/log.h>
#include <fcitx-utils/standardpath.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/userinterfacemanager.h>

#include <vector>

#include "base/init_mozc.h"
#include "base/process.h"
#include "unix/fcitx5/mozc_connection.h"
#include "unix/fcitx5/mozc_response_parser.h"

namespace fcitx {

const struct CompositionMode {
  const char *name;
  const char *icon;
  const char *label;
  const char *description;
  mozc::commands::CompositionMode mode;
} kPropCompositionModes[] = {
    {
        "mozc-mode-direct",
        "mozc-direct.png",
        "A",
        N_("Direct"),
        mozc::commands::DIRECT,
    },
    {
        "mozc-mode-hiragana",
        "mozc-hiragana.png",
        "\xe3\x81\x82",  // Hiragana letter A in UTF-8.
        N_("Hiragana"),
        mozc::commands::HIRAGANA,
    },
    {
        "mozc-mode-katakana_full",
        "mozc-katakana_full.png",
        "\xe3\x82\xa2",  // Katakana letter A.
        N_("Full Katakana"),
        mozc::commands::FULL_KATAKANA,
    },
    {

        "mozc-mode-alpha_half",
        "mozc-alpha_half.png",
        "A",
        N_("Half ASCII"),
        mozc::commands::HALF_ASCII,
    },
    {

        "mozc-mode-alpha_full",
        "mozc-alpha_full.png",
        "\xef\xbc\xa1",  // Full width ASCII letter A.
        N_("Full ASCII"),
        mozc::commands::FULL_ASCII,
    },
    {
        "mozc-mode-katakana_full",
        "mozc-katakana_half.png",
        "\xef\xbd\xb1",  // Half width Katakana letter A.
        N_("Half Katakana"),
        mozc::commands::HALF_KATAKANA,
    },
};
const size_t kNumCompositionModes = arraysize(kPropCompositionModes);

std::string MozcModeAction::shortText(InputContext *) const {
  return _("Composition Mode");
}

std::string MozcModeAction::longText(InputContext *ic) const {
  auto mozc_state = engine_->mozcState(ic);
  return _(kPropCompositionModes[mozc_state->GetCompositionMode()].description);
}

std::string MozcModeAction::icon(InputContext *ic) const {
  auto mozc_state = engine_->mozcState(ic);
  return stringutils::joinPath(
      StandardPath::global().fcitxPath("pkgdatadir"), "mozc/icon",
      kPropCompositionModes[mozc_state->GetCompositionMode()].icon);
}

MozcModeSubAction::MozcModeSubAction(MozcEngine *engine,
                                     mozc::commands::CompositionMode mode)
    : engine_(engine), mode_(mode) {
  setShortText(kPropCompositionModes[mode].label);
  setLongText(_(kPropCompositionModes[mode].description));
  setIcon(stringutils::joinPath(StandardPath::global().fcitxPath("pkgdatadir"),
                                "mozc/icon", kPropCompositionModes[mode].icon));
  setCheckable(true);
}

bool MozcModeSubAction::isChecked(InputContext *ic) const {
  auto mozc_state = engine_->mozcState(ic);
  return mozc_state->GetCompositionMode() == mode_;
}

void MozcModeSubAction::activate(InputContext *ic) {
  auto mozc_state = engine_->mozcState(ic);
  mozc_state->SendCompositionMode(mode_);
}

// This array must correspond with the CompositionMode enum in the
// mozc/session/command.proto file.
static_assert(mozc::commands::NUM_OF_COMPOSITIONS == kNumCompositionModes,
              "number of modes must match");

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
      }),
      modeAction_(this) {
  for (auto command :
       {mozc::commands::DIRECT, mozc::commands::HIRAGANA,
        mozc::commands::FULL_KATAKANA, mozc::commands::FULL_ASCII,
        mozc::commands::HALF_ASCII, mozc::commands::HALF_KATAKANA}) {
    modeActions_.push_back(std::make_unique<MozcModeSubAction>(this, command));
  }
  instance_->inputContextManager().registerProperty("mozcState", &factory_);
  instance_->userInterfaceManager().registerAction("mozc-mode", &modeAction_);
  instance_->userInterfaceManager().registerAction("mozc-tool", &toolAction_);
  toolAction_.setShortText(_("Tool"));
  toolAction_.setLongText(_("Tool"));
  toolAction_.setIcon(
      stringutils::joinPath(StandardPath::global().fcitxPath("pkgdatadir"),
                            "mozc/icon", "mozc-tool.png"));

  int i = 0;
  for (auto &modeAction : modeActions_) {
    instance_->userInterfaceManager().registerAction(
        kPropCompositionModes[i].name, modeAction.get());
    modeMenu_.addAction(modeAction.get());
    i++;
  }

  instance_->userInterfaceManager().registerAction("mozc-tool-config",
                                                   &configToolAction_);
  configToolAction_.setShortText(_("Configuration Tool"));
  configToolAction_.connect<SimpleAction::Activated>([](InputContext *) {
    mozc::Process::SpawnMozcProcess("mozc_tool", "--mode=config_dialog");
  });

  instance_->userInterfaceManager().registerAction("mozc-tool-dict",
                                                   &dictionaryToolAction_);
  dictionaryToolAction_.setShortText(_("Dictionary Tool"));
  dictionaryToolAction_.connect<SimpleAction::Activated>([](InputContext *) {
    mozc::Process::SpawnMozcProcess("mozc_tool", "--mode=dictionary_tool");
  });

  instance_->userInterfaceManager().registerAction("mozc-tool-handwriting",
                                                   &handWritingAction_);
  handWritingAction_.setShortText(_("Hand Writing"));
  handWritingAction_.connect<SimpleAction::Activated>([](InputContext *) {
    mozc::Process::SpawnMozcProcess("mozc_tool", "--mode=hand_writing");
  });

  instance_->userInterfaceManager().registerAction("mozc-tool-character",
                                                   &characterPaletteAction_);
  characterPaletteAction_.setShortText(_("Character Palette"));
  characterPaletteAction_.connect<SimpleAction::Activated>([](InputContext *) {
    mozc::Process::SpawnMozcProcess("mozc_tool", "--mode=character_palette");
  });

  instance_->userInterfaceManager().registerAction("mozc-tool-add",
                                                   &addWordAction_);
  addWordAction_.setShortText(_("Add Word"));
  addWordAction_.connect<SimpleAction::Activated>([](InputContext *) {
    mozc::Process::SpawnMozcProcess("mozc_tool", "--mode=word_register_dialog");
  });

  instance_->userInterfaceManager().registerAction("mozc-tool-about",
                                                   &aboutAction_);
  aboutAction_.setShortText(_("About Mozc"));
  aboutAction_.connect<SimpleAction::Activated>([](InputContext *) {
    mozc::Process::SpawnMozcProcess("mozc_tool", "--mode=about_dialog");
  });

  toolMenu_.addAction(&configToolAction_);
  toolMenu_.addAction(&dictionaryToolAction_);
  toolMenu_.addAction(&handWritingAction_);
  toolMenu_.addAction(&characterPaletteAction_);
  toolMenu_.addAction(&addWordAction_);
  toolMenu_.addAction(&aboutAction_);

  modeAction_.setMenu(&modeMenu_);
  toolAction_.setMenu(&toolMenu_);
}

MozcEngine::~MozcEngine() {}

void MozcEngine::reloadConfig() {}
void MozcEngine::activate(const fcitx::InputMethodEntry &,
                          fcitx::InputContextEvent &event) {
  auto ic = event.inputContext();
  auto mozc_state = mozcState(ic);
  mozc_state->FocusIn();
  ic->statusArea().addAction(StatusGroup::InputMethod, &modeAction_);
  ic->statusArea().addAction(StatusGroup::InputMethod, &toolAction_);
}
void MozcEngine::deactivate(const fcitx::InputMethodEntry &,
                            fcitx::InputContextEvent &event) {
  auto ic = event.inputContext();
  auto mozc_state = mozcState(ic);
  mozc_state->FocusOut();
  ic->statusArea().clearGroup(StatusGroup::InputMethod);
}
void MozcEngine::keyEvent(const InputMethodEntry &, KeyEvent &event) {
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

std::string MozcEngine::subMode(const fcitx::InputMethodEntry &,
                                fcitx::InputContext &ic) {
  return modeAction_.longText(&ic);
}

MozcState *MozcEngine::mozcState(InputContext *ic) {
  return ic->propertyFor(&factory_);
}

void MozcEngine::compositionModeUpdated(InputContext *ic) {
  modeAction_.update(ic);
  for (const auto &modeAction : modeActions_) {
    modeAction->update(ic);
  }
}

AddonInstance *MozcEngine::clipboardAddon() { return clipboard(); }
}  // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::MozcEngineFactory)

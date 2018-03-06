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
#ifndef _FCITX_UNIX_FCITX5_MOZC_ENGINE_H_
#define _FCITX_UNIX_FCITX5_MOZC_ENGINE_H_

#include <fcitx/action.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx/menu.h>

#include "unix/fcitx5/mozc_state.h"

namespace fcitx {

class MozcConnection;
class MozcResponseParser;
class MozcEngine;

class MozcModeAction : public Action {
 public:
  MozcModeAction(MozcEngine *engine) : engine_(engine) {}

  std::string shortText(fcitx::InputContext *) const override;
  std::string longText(fcitx::InputContext *) const override;
  std::string icon(fcitx::InputContext *) const override;

 private:
  MozcEngine *engine_;
};

class MozcModeSubAction : public SimpleAction {
 public:
  MozcModeSubAction(MozcEngine *engine, mozc::commands::CompositionMode mode);
  bool isChecked(fcitx::InputContext *) const override;
  void activate(fcitx::InputContext *) override;

 private:
  MozcEngine *engine_;
  mozc::commands::CompositionMode mode_;
};

class MozcEngine final : public InputMethodEngine {
 public:
  MozcEngine(Instance *instance);
  ~MozcEngine();
  Instance *instance() { return instance_; }
  void activate(const InputMethodEntry &entry,
                InputContextEvent &event) override;
  void deactivate(const fcitx::InputMethodEntry &entry,
                  fcitx::InputContextEvent &event) override;
  void keyEvent(const InputMethodEntry &entry, KeyEvent &keyEvent) override;
  void reloadConfig() override;
  void reset(const InputMethodEntry &entry, InputContextEvent &event) override;
  void save() override;
  std::string subMode(const fcitx::InputMethodEntry &,
                      fcitx::InputContext &) override;

  MozcState *mozcState(InputContext *ic);
  AddonInstance *clipboardAddon();

  void compositionModeUpdated(InputContext *ic);

 private:
  Instance *instance_;
  std::unique_ptr<MozcConnection> connection_;
  FactoryFor<MozcState> factory_;
  MozcModeAction modeAction_;
  SimpleAction toolAction_;
  MozcModeSubAction modeActions_[mozc::commands::NUM_OF_COMPOSITIONS] = {
      {this, mozc::commands::DIRECT},
      {this, mozc::commands::HIRAGANA},
      {this, mozc::commands::FULL_KATAKANA},
      {this, mozc::commands::FULL_ASCII},
      {this, mozc::commands::HALF_ASCII},
      {this, mozc::commands::HALF_KATAKANA},
  };

  SimpleAction configToolAction_, dictionaryToolAction_, handWritingAction_,
      characterPaletteAction_, addWordAction_, aboutAction_;
  Menu toolMenu_;
  Menu modeMenu_;

  FCITX_ADDON_DEPENDENCY_LOADER(clipboard, instance_->addonManager());
};

class MozcEngineFactory : public AddonFactory {
 public:
  AddonInstance *create(AddonManager *manager) override {
    return new MozcEngine(manager->instance());
  }
};
}  // namespace fcitx

#endif  // _FCITX_UNIX_FCITX5_MOZC_ENGINE_H_

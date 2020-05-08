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

#include <fcitx-config/configuration.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/stringutils.h>
#include <fcitx/action.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx/menu.h>

#include "base/file_util.h"
#include "base/system_util.h"
#include "unix/fcitx5/mozc_state.h"

namespace fcitx {

class MozcConnection;
class MozcResponseParser;
class MozcEngine;

FCITX_CONFIGURATION(
    MozcEngineConfig, const std::string toolPath_ = mozc::FileUtil::JoinPath(
                          mozc::SystemUtil::GetServerDirectory(), "mozc_tool");
    std::string toolCommand(const char *arg) {
      return stringutils::concat(toolPath_, " ", arg);
    }

    ExternalOption configTool{this, "ConfigTool", _("Configuration Tool"),
                              toolCommand("--mode=config_dialog")};
    ExternalOption dictTool{this, "Dictionary Tool", _("Dictionary Tool"),
                            toolCommand("--mode=dictionary_tool")};
    ExternalOption handWriting{this, "Hand Writing", _("Hand Writing"),
                               toolCommand("--mode=hand_writing")};
    ExternalOption charTool{this, "Character Palette", _("Character Palette"),
                            toolCommand("--mode=character_palette")};
    ExternalOption addTool{this, "Add Word", _("Add Word"),
                           toolCommand("--mode=word_register_dialog")};
    ExternalOption aboutTool{this, "About Mozc", _("About Mozc"),
                             toolCommand("--mode=about_dialog")};);

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
  const Configuration *getConfig() const override { return &config_; }

  MozcState *mozcState(InputContext *ic);
  AddonInstance *clipboardAddon();

  void compositionModeUpdated(InputContext *ic);

 private:
  Instance *instance_;
  std::unique_ptr<MozcConnection> connection_;
  FactoryFor<MozcState> factory_;
  MozcModeAction modeAction_;
  SimpleAction toolAction_;
  std::vector<std::unique_ptr<MozcModeSubAction>> modeActions_;

  SimpleAction configToolAction_, dictionaryToolAction_, handWritingAction_,
      characterPaletteAction_, addWordAction_, aboutAction_;
  Menu toolMenu_;
  Menu modeMenu_;
  MozcEngineConfig config_;

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

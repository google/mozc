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
#include <fcitx-config/enum.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/key.h>
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

enum class ExpandMode { Always, OnFocus, Hotkey };

using CompositionMode = mozc::commands::CompositionMode;

FCITX_CONFIG_ENUM_NAME_WITH_I18N(ExpandMode, N_("Always"), N_("On Focus"),
                                 N_("Hotkey"));

FCITX_CONFIG_ENUM_NAME_WITH_I18N(CompositionMode, N_("Direct"), N_("Hiragana"),
                                 N_("Full Katakana"), N_("Half ASCII"),
                                 N_("Full ASCII"), N_("Half Katakana"));

FCITX_CONFIGURATION(
    MozcEngineConfig, const std::string toolPath_ = mozc::FileUtil::JoinPath(
                          mozc::SystemUtil::GetServerDirectory(), "mozc_tool");
    std::string toolCommand(const char *arg) {
      return stringutils::concat(toolPath_, " ", arg);
    }

    OptionWithAnnotation<CompositionMode, CompositionModeI18NAnnotation>
        initialMode{this, "InitialMode", _("Initial Mode"),
                    mozc::commands::HIRAGANA};
    Option<bool> verticalList{this, "Vertical", _("Vertical candidate list"),
                              true};
    OptionWithAnnotation<ExpandMode, ExpandModeI18NAnnotation> expandMode{
        this, "ExpandMode",
        _("Expand Usage (Requires vertical candidate list)"),
        ExpandMode::OnFocus};
    Option<bool> preeditCursorPositionAtBeginning{
        this, "PreeditCursorPositionAtBeginning",
        _("Fix embedded preedit cursor at the beginning of the preedit"),
        false};
    Option<Key> expand{this, "ExpandKey", _("Hotkey to expand usage"),
                       Key("Control+Alt+H")};

    ExternalOption configTool{this, "ConfigTool", _("Configuration Tool"),
                              toolCommand("--mode=config_dialog")};
    ExternalOption dictTool{this, "Dictionary Tool", _("Dictionary Tool"),
                            toolCommand("--mode=dictionary_tool")};
    ExternalOption addTool{this, "Add Word", _("Add Word"),
                           toolCommand("--mode=word_register_dialog")};
    ExternalOption aboutTool{this, "About Mozc", _("About Mozc"),
                             toolCommand("--mode=about_dialog")};);

class MozcModeSubAction : public SimpleAction {
 public:
  MozcModeSubAction(MozcEngine *engine, mozc::commands::CompositionMode mode);
  bool isChecked(fcitx::InputContext *) const override;
  void activate(fcitx::InputContext *) override;

 private:
  MozcEngine *engine_;
  mozc::commands::CompositionMode mode_;
};

class MozcEngine final : public InputMethodEngineV2 {
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
  std::string subModeIconImpl(const InputMethodEntry &entry,
                              InputContext &ic) override;

  const Configuration *getConfig() const override { return &config_; }
  void setConfig(const RawConfig &config) override;

  auto &config() const { return config_; }
  auto factory() const { return &factory_; }

  MozcState *mozcState(InputContext *ic);
  AddonInstance *clipboardAddon();

  void compositionModeUpdated(InputContext *ic);

  void SyncData(bool force);

 private:
  Instance *instance_;
  std::unique_ptr<MozcConnection> connection_;
  std::unique_ptr<mozc::client::ClientInterface> client_;
  FactoryFor<MozcState> factory_;
  SimpleAction toolAction_;
  std::vector<std::unique_ptr<MozcModeSubAction>> modeActions_;

  SimpleAction configToolAction_, dictionaryToolAction_, addWordAction_,
      aboutAction_;
  Menu toolMenu_;
  MozcEngineConfig config_;
  uint64 lastSyncTime_;

  FCITX_ADDON_DEPENDENCY_LOADER(clipboard, instance_->addonManager());
};

}  // namespace fcitx

#endif  // _FCITX_UNIX_FCITX5_MOZC_ENGINE_H_

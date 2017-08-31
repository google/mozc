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

#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>

#include "base/namespace.h"
#include "unix/fcitx5/mozc_state.h"

namespace fcitx {

class MozcConnection;
class MozcResponseParser;

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
  vector<InputMethodEntry> listInputMethods() override;
  void reloadConfig() override;
  void reset(const InputMethodEntry &entry, InputContextEvent &event) override;
  void save() override;

  MozcState *mozcState(InputContext *ic);

  AddonInstance *clipboardAddon();

 private:
  Instance *instance_;
  unique_ptr<MozcConnection> connection_;
  FactoryFor<MozcState> factory_;

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

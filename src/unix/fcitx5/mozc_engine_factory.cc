/*
 * Copyright (C) 2020~2020 by CSSlayer
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
#include <fcitx/addonfactory.h>
#include "mozc_engine.h"

namespace fcitx {
class MozcEngineFactory : public AddonFactory {
 public:
  AddonInstance *create(AddonManager *manager) override {
    return new MozcEngine(manager->instance());
  }
};
}  // namespace fcitx


FCITX_ADDON_FACTORY(fcitx::MozcEngineFactory)

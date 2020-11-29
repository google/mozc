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
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/misc.h>
#include <fcitx-utils/stringutils.h>
#include <fcitx/addonfactory.h>
#include <stdlib.h>

#include <string_view>

#include "base/system_util.h"
#include "mozc_engine.h"

namespace fcitx {
class MozcEngineFactory : public AddonFactory {
 public:
  AddonInstance *create(AddonManager *manager) override {
    // We don't have a direct way to detect, so we simply try.
    auto baseDirectory = makeUniqueCPtr(
        realpath(mozc::SystemUtil::GetServerDirectory().data(), nullptr));
    int numberOfSlash = 0;
    if (baseDirectory) {
      std::string_view view(baseDirectory.get());
      for (auto c : view) {
        if (c == '/') {
          numberOfSlash += 1;
        }
      }
      if (view.empty()) {
        baseDirectory.reset();
      }
    }

    // Make sure we don't deadloop.
    while (baseDirectory && numberOfSlash >= 0) {
      auto path = stringutils::joinPath(baseDirectory.get(), "share/locale");
      if (fs::isdir(path)) {
        registerDomain("fcitx5-mozc", path.data());
      }
      baseDirectory = cdUp(baseDirectory.get());
      if (baseDirectory && std::string_view(baseDirectory.get()).empty()) {
        baseDirectory.reset();
      }
      numberOfSlash -= 1;
    }
    return new MozcEngine(manager->instance());
  }

 private:
  UniqueCPtr<char> cdUp(const char *path) {
    return makeUniqueCPtr(
        realpath(stringutils::joinPath(path, "..").data(), nullptr));
  }
};
}  // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::MozcEngineFactory)

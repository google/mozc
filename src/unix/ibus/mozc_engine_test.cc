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

#include "unix/ibus/mozc_engine.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "client/client_mock.h"
#include "protocol/commands.pb.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/test_peer.h"
#include "unix/ibus/ibus_config.h"

namespace mozc {
namespace ibus {

class MozcEngineTestPeer : public ::mozc::testing::TestPeer<MozcEngine> {
 public:
  explicit MozcEngineTestPeer(MozcEngine &engine)
      : testing::TestPeer<MozcEngine>(engine) {}

  PEER_METHOD(LaunchTool);
  PEER_VARIABLE(client_);
};

namespace {

bool CallCanUseMozcCandidateWindow(
    const std::string &text_proto,
    const absl::flat_hash_map<std::string, std::string> &env) {
  IbusConfig ibus_config;
  EXPECT_TRUE(ibus_config.LoadConfig(text_proto));
  return CanUseMozcCandidateWindow(ibus_config, env);
}

}  // namespace

using ::testing::_;
using ::testing::Return;

class LaunchToolTest : public ::testing::Test {
 public:
  LaunchToolTest() = default;
  LaunchToolTest(const LaunchToolTest &) = delete;
  LaunchToolTest &operator=(const LaunchToolTest &) = delete;

 protected:
  void SetUp() override {
    mozc_engine_ = std::make_unique<MozcEngine>();

    // Keep the reference to `mock` to access the mock methods.
    // It's safe because `mozc_engine_` owns `mock`.
    // This is a test-only usage.
    auto mock = std::make_unique<client::ClientMock>();
    mock_ = mock.get();
    mozc_engine_peer().client_() = std::move(mock);
  }

  void TearDown() override { mozc_engine_.reset(); }

  MozcEngineTestPeer mozc_engine_peer() {
    return MozcEngineTestPeer(*mozc_engine_);
  }

  client::ClientMock *mock_;
  std::unique_ptr<MozcEngine> mozc_engine_;
};

TEST_F(LaunchToolTest, LaunchToolTest) {
  commands::Output output;

  // Launch config dialog
  output.set_launch_tool_mode(commands::Output::CONFIG_DIALOG);
  EXPECT_CALL(*mock_, LaunchToolWithProtoBuf(_)).WillOnce(Return(true));
  EXPECT_TRUE(mozc_engine_peer().LaunchTool(output));

  // Launch dictionary tool
  output.set_launch_tool_mode(commands::Output::DICTIONARY_TOOL);
  EXPECT_CALL(*mock_, LaunchToolWithProtoBuf(_)).WillOnce(Return(true));
  EXPECT_TRUE(mozc_engine_peer().LaunchTool(output));

  // Launch word register dialog
  output.set_launch_tool_mode(commands::Output::WORD_REGISTER_DIALOG);
  EXPECT_CALL(*mock_, LaunchToolWithProtoBuf(_)).WillOnce(Return(true));
  EXPECT_TRUE(mozc_engine_peer().LaunchTool(output));

  // Launch no tool(means do nothing)
  output.set_launch_tool_mode(commands::Output::NO_TOOL);
  EXPECT_CALL(*mock_, LaunchToolWithProtoBuf(_)).WillOnce(Return(false));
  EXPECT_FALSE(mozc_engine_peer().LaunchTool(output));

  // Something occurring in client::Client::LaunchTool
  output.set_launch_tool_mode(commands::Output::CONFIG_DIALOG);
  EXPECT_CALL(*mock_, LaunchToolWithProtoBuf(_)).WillOnce(Return(false));
  EXPECT_FALSE(mozc_engine_peer().LaunchTool(output));
}

// clang-format off
TEST(MozcEngineTest, CanUseMozcCandidateWindowTest_X11) {
  // Default enabled
  EXPECT_TRUE(CallCanUseMozcCandidateWindow(R"(
    mozc_renderer {
    }
  )", {})) << "mozc_renderer is enabled by default";

  EXPECT_TRUE(CallCanUseMozcCandidateWindow(R"(
    mozc_renderer {
      enabled : True
    }
  )", {}));

  EXPECT_FALSE(CallCanUseMozcCandidateWindow(R"(
    mozc_renderer {
      enabled : False
    }
  )", {}));

  EXPECT_FALSE(CallCanUseMozcCandidateWindow(R"(
    mozc_renderer {
      enabled : True
    }
  )", {
    {"MOZC_IBUS_CANDIDATE_WINDOW", "ibus"}
  })) << "MOZC_IBUS_CANDIDATE_WINDOW=ibus is still supported.";
}

TEST(MozcEngineTest, CanUseMozcCandidateWindowTest_Wayland) {
  EXPECT_FALSE(CallCanUseMozcCandidateWindow(R"(
    mozc_renderer {
      enabled : True
    }
  )", {
    {"WAYLAND_DISPLAY", "wayland-0"},
  }));

  EXPECT_FALSE(CallCanUseMozcCandidateWindow(R"(
    mozc_renderer {
      enabled : True
      compatible_wayland_desktop_names : []
    }
  )", {
    {"WAYLAND_DISPLAY", "wayland-0"},
    {"XDG_CURRENT_DESKTOP", "GNOME"}
  }));

  EXPECT_TRUE(CallCanUseMozcCandidateWindow(R"(
    mozc_renderer {
      enabled : True
      compatible_wayland_desktop_names : ["GNOME"]
    }
  )", {
    {"WAYLAND_DISPLAY", "wayland-0"},
    {"XDG_CURRENT_DESKTOP", "GNOME"}
  }));

  EXPECT_TRUE(CallCanUseMozcCandidateWindow(R"(
    mozc_renderer {
      enabled : True
      compatible_wayland_desktop_names : ["GNOME"]
    }
  )", {
    {"WAYLAND_DISPLAY", "wayland-0"},
    {"XDG_CURRENT_DESKTOP", "KDE:GNOME"}
  }));

  EXPECT_FALSE(CallCanUseMozcCandidateWindow(R"(
    mozc_renderer {
      enabled : True
      compatible_wayland_desktop_names : ["GNOME"]
    }
  )", {
    {"WAYLAND_DISPLAY", "wayland-0"},
    {"XDG_CURRENT_DESKTOP", "KDE"}
  }));

  EXPECT_FALSE(CallCanUseMozcCandidateWindow(R"(
    mozc_renderer {
      enabled : True
      compatible_wayland_desktop_names : ["GNOME"]
    }
  )", {
    {"WAYLAND_DISPLAY", "wayland-0"},
  }));

  EXPECT_TRUE(CallCanUseMozcCandidateWindow(R"(
    mozc_renderer {
      enabled : True
      compatible_wayland_desktop_names : ["GNOME", "KDE"]
    }
  )", {
    {"WAYLAND_DISPLAY", "wayland-0"},
    {"XDG_CURRENT_DESKTOP", "KDE"}
  }));
}
// clang-format on

}  // namespace ibus
}  // namespace mozc

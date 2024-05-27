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

#include "unix/ibus/ibus_config.h"

#include <string>

#include "testing/gunit.h"
#include "unix/ibus/ibus_config.pb.h"

namespace mozc {
namespace ibus {

TEST(IbusConfigTest, LoadConfig) {
  IbusConfig config;
  const std::string config_data = R"(engines {
  name : "mozc-jp"
  longname : "Mozc"
  layout : "default"
  layout_variant : ""
  layout_option : ""
  rank : 80
}
active_on_launch: False
)";
  EXPECT_TRUE(config.LoadConfig(config_data));

  const std::string expected_xml = R"(<engines>
<engine>
  <description>Mozc (Japanese Input Method)</description>
  <language>ja</language>
  <icon>/usr/share/ibus-mozc/product_icon.png</icon>
  <rank>80</rank>
  <icon_prop_key>InputMode</icon_prop_key>
  <symbol>あ</symbol>
  <setup>/usr/lib/mozc/mozc_tool --mode=config_dialog</setup>
  <name>mozc-jp</name>
  <longname>Mozc</longname>
  <layout>default</layout>
  <layout_variant></layout_variant>
  <layout_option></layout_option>
</engine>
</engines>
)";
  EXPECT_EQ(config.GetEnginesXml(), expected_xml);
}

TEST(IbusConfigTest, NormalizeLayout) {
  IbusConfig config;
  const std::string config_data = R"(engines {
  name : "mozc-jp"
  longname : "Mozc"
  layout : "nec_vndr/jp"
  layout_variant : "Hello World!"
  layout_option : "09AZaz!@#$%^&*()_+|;-"
  rank : 80
}
active_on_launch: False
)";
  EXPECT_TRUE(config.LoadConfig(config_data));

  const ibus::Config &config_proto = config.GetConfig();
  EXPECT_EQ(config_proto.engines_size(), 1);
  EXPECT_EQ(config_proto.engines(0).layout(), "nec_vndr/jp");
  EXPECT_EQ(config_proto.engines(0).layout_variant(), "Hello_World_");
  EXPECT_EQ(config_proto.engines(0).layout_option(), "09AZaz______________-");
}

}  // namespace ibus
}  // namespace mozc

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

#include "session/ime_context.h"

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/candidate.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "engine/engine_converter.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/keymap.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/testing_util.h"

namespace mozc {
namespace session {

using ::mozc::engine::EngineConverter;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

TEST(ImeContextTest, DefaultValues) {
  ImeContext context;
  EXPECT_EQ(context.create_time(), absl::InfinitePast());
  EXPECT_EQ(context.last_command_time(), absl::InfinitePast());
  EXPECT_FALSE(context.mutable_converter());
  EXPECT_TRUE(context.mutable_composer());
  EXPECT_EQ(context.state(), ImeContext::NONE);
  EXPECT_PROTO_EQ(commands::Request::default_instance(), context.GetRequest());
  EXPECT_PROTO_EQ(config::ConfigHandler::DefaultConfig(), context.GetConfig());
}

TEST(ImeContextTest, BasicTest) {
  auto request = std::make_shared<const commands::Request>();
  auto config = std::make_shared<const config::Config>();
  auto keymap = std::make_shared<const keymap::KeyMapManager>();

  auto converter = std::make_shared<MockConverter>();
  ImeContext context(std::make_unique<EngineConverter>(converter));

  context.set_create_time(absl::FromUnixSeconds(100));
  EXPECT_EQ(context.create_time(), absl::FromUnixSeconds(100));

  context.set_last_command_time(absl::FromUnixSeconds(12345));
  EXPECT_EQ(context.last_command_time(), absl::FromUnixSeconds(12345));

  context.set_state(ImeContext::COMPOSITION);
  EXPECT_EQ(context.state(), ImeContext::COMPOSITION);

  context.SetRequest(request);
  context.SetConfig(config);
  context.SetKeyMapManager(keymap);
  EXPECT_PROTO_EQ(*request, context.GetRequest());
  EXPECT_PROTO_EQ(*config, context.GetConfig());
  EXPECT_EQ(keymap.get(), &context.GetKeyMapManager());

  context.mutable_client_capability()->set_text_deletion(
      commands::Capability::DELETE_PRECEDING_TEXT);
  EXPECT_EQ(commands::Capability::DELETE_PRECEDING_TEXT,
            context.client_capability().text_deletion());

  context.mutable_application_info()->set_process_id(123);
  EXPECT_EQ(context.application_info().process_id(), 123);

  context.mutable_output()->set_id(1414);
  EXPECT_EQ(context.output().id(), 1414);
}

TEST(ImeContextTest, CopyContext) {
  auto table = std::make_shared<composer::Table>();
  table->AddRule("a", "あ", "");
  table->AddRule("n", "ん", "");
  table->AddRule("na", "な", "");
  auto request = std::make_shared<const commands::Request>();
  auto config = std::make_shared<config::Config>();
  auto keymap = std::make_shared<const keymap::KeyMapManager>();

  config->set_session_keymap(config::Config::CHROMEOS);

  auto converter = std::make_shared<MockConverter>();

  Segments segments;
  Segment* segment = segments.add_segment();
  segment->set_key("あん");
  converter::Candidate* candidate = segment->add_candidate();
  candidate->value = "庵";
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  {
    ImeContext source(std::make_unique<EngineConverter>(converter));
    source.SetRequest(request);
    source.SetConfig(config);
    source.SetKeyMapManager(keymap);
    source.mutable_composer()->SetTable(table);

    source.set_state(ImeContext::COMPOSITION);
    source.mutable_composer()->InsertCharacter("a");
    source.mutable_composer()->InsertCharacter("n");

    std::string composition = source.composer().GetStringForSubmission();
    EXPECT_EQ(composition, "あｎ");

    ImeContext destination(source);

    EXPECT_PROTO_EQ(source.GetRequest(), destination.GetRequest());
    EXPECT_PROTO_EQ(source.GetConfig(), destination.GetConfig());
    EXPECT_EQ(&source.GetKeyMapManager(), &destination.GetKeyMapManager());
    EXPECT_EQ(destination.state(), ImeContext::COMPOSITION);
    composition = destination.composer().GetStringForSubmission();
    EXPECT_EQ(composition, "あｎ");
  }

  {
    constexpr absl::Time kCreateTime = absl::FromUnixSeconds(100);
    constexpr absl::Time kLastCommandTime = absl::FromUnixSeconds(200);

    ImeContext source(std::make_unique<EngineConverter>(converter));
    source.SetRequest(request);
    source.SetConfig(config);
    source.SetKeyMapManager(keymap);
    source.mutable_composer()->SetTable(table);
    source.set_create_time(kCreateTime);
    source.set_last_command_time(kLastCommandTime);

    source.set_state(ImeContext::CONVERSION);
    source.mutable_composer()->InsertCharacter("a");
    source.mutable_composer()->InsertCharacter("n");
    source.mutable_converter()->Convert(source.composer());
    constexpr absl::string_view kQuick = "早い";
    source.mutable_composer()->set_source_text(kQuick);

    std::string composition = source.composer().GetQueryForConversion();
    EXPECT_EQ(composition, "あん");

    commands::Output output;
    source.converter().FillOutput(source.composer(), &output);
    EXPECT_EQ(output.preedit().segment_size(), 1);
    EXPECT_EQ(output.preedit().segment(0).value(), "庵");

    ImeContext destination(source);
    EXPECT_EQ(destination.create_time(), kCreateTime);
    EXPECT_EQ(destination.last_command_time(), kLastCommandTime);
    EXPECT_EQ(destination.state(), ImeContext::CONVERSION);
    composition = destination.composer().GetQueryForConversion();
    EXPECT_EQ(composition, "あん");

    output.Clear();
    destination.converter().FillOutput(source.composer(), &output);
    EXPECT_EQ(output.preedit().segment_size(), 1);
    EXPECT_EQ(output.preedit().segment(0).value(), "庵");

    EXPECT_EQ(destination.composer().source_text(), kQuick);
  }
}

}  // namespace session
}  // namespace mozc

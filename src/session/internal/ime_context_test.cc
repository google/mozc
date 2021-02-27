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

#include "session/internal/ime_context.h"

#include <cstdint>
#include <memory>
#include <string>

#include "composer/composer.h"
#include "composer/key_parser.h"
#include "composer/table.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "engine/mock_converter_engine.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/internal/keymap.h"
#include "session/internal/keymap_factory.h"
#include "session/internal/keymap_interface.h"
#include "session/session_converter.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/testing_util.h"

namespace mozc {
namespace session {

TEST(ImeContextTest, DefaultValues) {
  ImeContext context;

  EXPECT_EQ(0, context.create_time());
  EXPECT_EQ(0, context.last_command_time());

  // Don't access composer().
  // Before using composer, set_composer() must be called with non-null-value.

  EXPECT_TRUE(nullptr == context.mutable_converter());

  EXPECT_EQ(ImeContext::NONE, context.state());

  EXPECT_PROTO_EQ(commands::Request::default_instance(), context.GetRequest());
}

TEST(ImeContextTest, BasicTest) {
  ImeContext context;
  config::Config config;

  context.set_create_time(100);
  EXPECT_EQ(100, context.create_time());

  context.set_last_command_time(12345);
  EXPECT_EQ(12345, context.last_command_time());

  const commands::Request request;

  // The ownership of composer is moved to context.
  composer::Composer *composer =
      new composer::Composer(nullptr, &request, &config);
  context.set_composer(composer);
  EXPECT_EQ(composer, &context.composer());
  EXPECT_EQ(composer, context.mutable_composer());

  std::unique_ptr<ConverterMock> converter_mock(new ConverterMock);
  SessionConverter *converter =
      new SessionConverter(converter_mock.get(), &request, &config);
  context.set_converter(converter);
  EXPECT_EQ(converter, &context.converter());
  EXPECT_EQ(converter, context.mutable_converter());

  context.set_state(ImeContext::COMPOSITION);
  EXPECT_EQ(ImeContext::COMPOSITION, context.state());

  context.SetRequest(&request);
  EXPECT_PROTO_EQ(request, context.GetRequest());

  context.mutable_client_capability()->set_text_deletion(
      commands::Capability::DELETE_PRECEDING_TEXT);
  EXPECT_EQ(commands::Capability::DELETE_PRECEDING_TEXT,
            context.client_capability().text_deletion());

  context.mutable_application_info()->set_process_id(123);
  EXPECT_EQ(123, context.application_info().process_id());

  context.mutable_output()->set_id(1414);
  EXPECT_EQ(1414, context.output().id());

  // Get/Set keymap
  context.set_keymap(config::Config::ATOK);
  EXPECT_EQ(config::Config::ATOK, context.keymap());
  context.set_keymap(config::Config::MSIME);
  EXPECT_EQ(config::Config::MSIME, context.keymap());

  // Set keymp via SetConfig.
  config.set_session_keymap(config::Config::KOTOERI);
  context.SetConfig(&config);
  EXPECT_EQ(config::Config::KOTOERI, context.keymap());
}

TEST(ImeContextTest, CopyContext) {
  composer::Table table;
  table.AddRule("a", "あ", "");
  table.AddRule("n", "ん", "");
  table.AddRule("na", "な", "");
  const commands::Request request;
  const config::Config config;

  std::unique_ptr<MockConverterEngine> engine(new MockConverterEngine);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("あん");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "庵";

  engine->mutable_converter_mock()->SetStartConversionForRequest(&segments,
                                                                 true);
  {
    ImeContext source;
    source.set_composer(new composer::Composer(&table, &request, &config));
    source.set_converter(
        new SessionConverter(engine->GetConverter(), &request, &config));

    ImeContext destination;
    destination.set_composer(new composer::Composer(&table, &request, &config));
    destination.set_converter(
        new SessionConverter(engine->GetConverter(), &request, &config));

    source.set_state(ImeContext::COMPOSITION);
    source.mutable_composer()->InsertCharacter("a");
    source.mutable_composer()->InsertCharacter("n");

    std::string composition;
    source.composer().GetStringForSubmission(&composition);
    EXPECT_EQ("あｎ", composition);

    ImeContext::CopyContext(source, &destination);
    EXPECT_EQ(ImeContext::COMPOSITION, destination.state());
    composition.clear();
    source.composer().GetStringForSubmission(&composition);
    EXPECT_EQ("あｎ", composition);
  }

  {
    const uint64_t kCreateTime = 100;
    const uint64_t kLastCommandTime = 200;
    ImeContext source;
    source.set_create_time(kCreateTime);
    source.set_last_command_time(kLastCommandTime);
    source.set_composer(new composer::Composer(&table, &request, &config));
    source.set_converter(
        new SessionConverter(engine->GetConverter(), &request, &config));

    ImeContext destination;
    destination.set_composer(new composer::Composer(&table, &request, &config));
    destination.set_converter(
        new SessionConverter(engine->GetConverter(), &request, &config));

    source.set_state(ImeContext::CONVERSION);
    source.mutable_composer()->InsertCharacter("a");
    source.mutable_composer()->InsertCharacter("n");
    source.mutable_converter()->Convert(source.composer());
    const std::string &kQuick = "早い";
    source.mutable_composer()->set_source_text(kQuick);

    std::string composition;
    source.composer().GetQueryForConversion(&composition);
    EXPECT_EQ("あん", composition);

    commands::Output output;
    source.converter().FillOutput(source.composer(), &output);
    EXPECT_EQ(1, output.preedit().segment_size());
    EXPECT_EQ("庵", output.preedit().segment(0).value());

    ImeContext::CopyContext(source, &destination);
    EXPECT_EQ(kCreateTime, destination.create_time());
    EXPECT_EQ(kLastCommandTime, destination.last_command_time());
    EXPECT_EQ(ImeContext::CONVERSION, destination.state());
    composition.clear();
    destination.composer().GetQueryForConversion(&composition);
    EXPECT_EQ("あん", composition);

    output.Clear();
    destination.converter().FillOutput(source.composer(), &output);
    EXPECT_EQ(1, output.preedit().segment_size());
    EXPECT_EQ("庵", output.preedit().segment(0).value());

    EXPECT_EQ(kQuick, destination.composer().source_text());
  }
}

TEST(ImeContextTest, CustomKeymap) {
  ImeContext context;

  // Init config with custom keymap.  It will be set later.
  config::Config config;
  const std::string custom_keymap_table =
      "status\tkey\tcommand\n"
      "Precomposition\tCtrl a\tUndo\n";
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(custom_keymap_table);

  // Set composer
  const commands::Request request;
  composer::Composer *composer =
      new composer::Composer(nullptr, &request, &config);
  context.set_composer(composer);

  // Set converter
  std::unique_ptr<ConverterMock> converter_mock(new ConverterMock);
  SessionConverter *converter =
      new SessionConverter(converter_mock.get(), &request, &config);
  context.set_converter(converter);

  // Set config.
  context.SetConfig(&config);

  const keymap::KeyMapManager *keymap =
      keymap::KeyMapFactory::GetKeyMapManager(context.keymap());

  commands::KeyEvent key_event;
  KeyParser::ParseKey("Ctrl a", &key_event);

  keymap::PrecompositionState::Commands command;
  keymap->GetCommandPrecomposition(key_event, &command);
  EXPECT_EQ(keymap::PrecompositionState::UNDO, command);
}

}  // namespace session
}  // namespace mozc

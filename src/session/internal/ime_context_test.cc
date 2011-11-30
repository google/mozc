// Copyright 2010-2011, Google Inc.
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

#include "composer/composer.h"
#include "composer/table.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "session/session_converter.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"

namespace mozc {
namespace session {

TEST(ImeContextTest, DefaultValues) {
  ImeContext context;

  EXPECT_EQ(0, context.create_time());
  EXPECT_EQ(0, context.last_command_time());

  // Don't access composer().
  // Before using composer, set_composer() must be called with non-null-value.

  EXPECT_TRUE(NULL == context.mutable_converter());

  EXPECT_EQ(ImeContext::NONE, context.state());

  EXPECT_TRUE(context.transform_table().empty());
}

TEST(ImeContextTest, BasicTest) {
  ImeContext context;

  context.set_create_time(100);
  EXPECT_EQ(100, context.create_time());

  context.set_last_command_time(12345);
  EXPECT_EQ(12345, context.last_command_time());

  // The ownership of composer is moved to context.
  composer::Composer *composer = new composer::Composer;
  context.set_composer(composer);
  EXPECT_EQ(composer, &context.composer());
  EXPECT_EQ(composer, context.mutable_composer());

  ConverterMock converter_mock;
  SessionConverter *converter = new SessionConverter(&converter_mock);
  context.set_converter(converter);
  EXPECT_EQ(converter, &context.converter());
  EXPECT_EQ(converter, context.mutable_converter());

  context.set_state(ImeContext::COMPOSITION);
  EXPECT_EQ(ImeContext::COMPOSITION, context.state());

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
}

TEST(ImeContextTest, CopyContext) {
  composer::Table table;
  // "あ"
  table.AddRule("a", "\xE3\x81\x82", "");
  // "ん"
  table.AddRule("n", "\xE3\x82\x93", "");
  // "な"
  table.AddRule("na", "\xE3\x81\xAA", "");

  ConverterMock convertermock;
  ConverterFactory::SetConverter(&convertermock);

  Segments segments;
  Segment *segment = segments.add_segment();
  // "あん"
  segment->set_key("\xE3\x81\x82\xE3\x82\x93");
  Segment::Candidate *candidate = segment->add_candidate();
  // "庵"
  candidate->value = "\xE5\xBA\xB5";

  convertermock.SetStartConversionWithComposer(&segments, true);

  {
    ImeContext source;
    source.set_composer(new composer::Composer);
    source.mutable_composer()->SetTable(&table);
    source.set_converter(
        new SessionConverter(ConverterFactory::GetConverter()));

    ImeContext destination;
    destination.set_composer(new composer::Composer);
    destination.mutable_composer()->SetTable(&table);
    destination.set_converter(
        new SessionConverter(ConverterFactory::GetConverter()));

    source.set_state(ImeContext::COMPOSITION);
    source.mutable_composer()->InsertCharacter("a");
    source.mutable_composer()->InsertCharacter("n");

    string composition;
    source.composer().GetStringForSubmission(&composition);
    // "あｎ"
    EXPECT_EQ("\xE3\x81\x82\xEF\xBD\x8E", composition);

    ImeContext::CopyContext(source, &destination);
    EXPECT_EQ(ImeContext::COMPOSITION, destination.state());
    composition.clear();
    source.composer().GetStringForSubmission(&composition);
    // "あｎ"
    EXPECT_EQ("\xE3\x81\x82\xEF\xBD\x8E", composition);
  }

  {
    ImeContext source;
    source.set_composer(new composer::Composer);
    source.mutable_composer()->SetTable(&table);
    source.set_converter(
        new SessionConverter(ConverterFactory::GetConverter()));

    ImeContext destination;
    destination.set_composer(new composer::Composer);
    destination.mutable_composer()->SetTable(&table);
    destination.set_converter(
        new SessionConverter(ConverterFactory::GetConverter()));

    source.set_state(ImeContext::CONVERSION);
    source.mutable_composer()->InsertCharacter("a");
    source.mutable_composer()->InsertCharacter("n");
    source.mutable_converter()->Convert(source.composer());
    // "早い"
    const string &kQuick = "\xE6\x97\xA9\xE3\x81\x84";
    source.mutable_composer()->set_source_text(kQuick);

    string composition;
    source.composer().GetQueryForConversion(&composition);
    // "あん"
    EXPECT_EQ("\xE3\x81\x82\xE3\x82\x93", composition);

    commands::Output output;
    source.converter().FillOutput(source.composer(), &output);
    EXPECT_EQ(1, output.preedit().segment_size());
    // "庵"
    EXPECT_EQ("\xE5\xBA\xB5", output.preedit().segment(0).value());

    ImeContext::CopyContext(source, &destination);
    EXPECT_EQ(ImeContext::CONVERSION, destination.state());
    composition.clear();
    destination.composer().GetQueryForConversion(&composition);
    // "あん"
    EXPECT_EQ("\xE3\x81\x82\xE3\x82\x93", composition);

    output.Clear();
    destination.converter().FillOutput(source.composer(), &output);
    EXPECT_EQ(1, output.preedit().segment_size());
    // "庵"
    EXPECT_EQ("\xE5\xBA\xB5", output.preedit().segment(0).value());

    EXPECT_EQ(kQuick, destination.composer().source_text());
  }
}

}  // namespace session
}  // namespace mozc

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

#include <memory>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "composer/composer.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "engine/engine_factory.h"
#include "engine/engine_interface.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

ConversionRequest ConvReq(absl::string_view key) {
  composer::Composer composer;
  composer.SetPreeditTextForTestOnly(key);
  return ConversionRequestBuilder().SetComposer(composer).Build();
}

class ConverterRegressionTest : public testing::TestWithTempUserProfile {};

TEST_F(ConverterRegressionTest, QueryOfDeathTest) {
  std::unique_ptr<EngineInterface> engine = EngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();

  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(
        converter->StartConversion(ConvReq("りゅきゅけmぽ"), &segments));
  }
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(ConvReq("5.1,||t:1"), &segments));
  }
  {
    Segments segments;
    // Converter returns false, but not crash.
    EXPECT_FALSE(converter->StartConversion(ConvReq(""), &segments));
  }
  {
    Segments segments;
    const ConversionRequest conv_request;
    // Converter returns false, but not crash.
    EXPECT_FALSE(converter->StartConversion(conv_request, &segments));
  }
}

TEST_F(ConverterRegressionTest, Regression3323108) {
  std::unique_ptr<EngineInterface> engine = EngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  Segments segments;

  EXPECT_TRUE(
      converter->StartConversion(ConvReq("ここではきものをぬぐ"), &segments));
  EXPECT_EQ(segments.conversion_segments_size(), 3);
  const ConversionRequest default_request;
  EXPECT_TRUE(converter->ResizeSegment(&segments, default_request, 1, 2));
  EXPECT_EQ(segments.conversion_segments_size(), 2);
  EXPECT_EQ(segments.conversion_segment(1).key(), "きものをぬぐ");
}

}  // namespace
}  // namespace mozc

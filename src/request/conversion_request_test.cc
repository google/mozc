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

#include "request/conversion_request.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/gunit.h"

namespace mozc {

TEST(CopyOrViewPtrTest, BasicTest) {
  {
    // Default constructor is view.
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr(s);
    EXPECT_EQ(*ptr, s);
    EXPECT_EQ(&*ptr, &s);
  }

  {
    // Copy
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr;
    EXPECT_FALSE(ptr);
    ptr.copy_from(s);
    EXPECT_EQ(*ptr, s);
    EXPECT_NE(&*ptr, &s);
  }

  {
    // View
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr;
    EXPECT_FALSE(ptr);
    ptr.set_view(s);
    EXPECT_EQ(*ptr, s);
    EXPECT_EQ(&*ptr, &s);
  }

  {
    // Move
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr;
    EXPECT_FALSE(ptr);
    ptr.move_from(std::move(s));
    EXPECT_EQ(*ptr, "test");
  }

  {
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr;
    ptr.copy_from(s);
    internal::copy_or_view_ptr<std::string> ptr2 = ptr;
    EXPECT_EQ(*ptr, s);
    EXPECT_EQ(*ptr2, s);
    EXPECT_NE(&*ptr, &s);
    EXPECT_NE(&*ptr2, &s);
  }

  {
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr;
    ptr.set_view(s);
    internal::copy_or_view_ptr<std::string> ptr2 = ptr;
    EXPECT_EQ(*ptr, s);
    EXPECT_EQ(*ptr2, s);
    EXPECT_EQ(&*ptr, &s);
    EXPECT_EQ(&*ptr2, &s);
  }

  {
    std::string s = "test";
    internal::copy_or_view_ptr<std::string> ptr;
    ptr.set_view(s);
    internal::copy_or_view_ptr<std::string> ptr2 = std::move(ptr);
    EXPECT_EQ(*ptr2, s);
    EXPECT_EQ(&*ptr2, &s);
  }
}

TEST(ConversionRequestTest, SetKeyTest) {
  auto request = std::make_shared<commands::Request>();
  auto config = std::make_shared<config::Config>();
  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(*request, *config);

  composer::Composer composer(table, request, config);
  composer.SetPreeditTextForTestOnly("query");

  // SetKey("") is ignored. The actual key is created from composer.
  ConversionRequest conversion_request1 = ConversionRequestBuilder()
                                              .SetConfigView(*config)
                                              .SetRequestView(*request)
                                              .SetComposer(composer)
                                              .SetKey("")
                                              .Build();
  EXPECT_EQ("query", conversion_request1.key());

  ConversionRequest conversion_request2 = ConversionRequestBuilder()
                                              .SetConfigView(*config)
                                              .SetRequestView(*request)
                                              .SetComposer(composer)
                                              .SetKey("foo")
                                              .Build();
  EXPECT_EQ("foo", conversion_request2.key());
}

TEST(ConversionRequestTest, SetHistorySegmentsTest) {
  Segments segments;
  for (int i = 0; i < 3; ++i) {
    Segment *seg = segments.push_back_segment();
    seg->set_segment_type(Segment::HISTORY);
    converter::Candidate *c = seg->add_candidate();
    c->key = absl::StrCat("k", i);
    c->value = absl::StrCat("v", i);
  }

  {
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetHistorySegmentsView(segments).Build();

    EXPECT_EQ(convreq.converter_history_key(), "k0k1k2");
    EXPECT_EQ(convreq.converter_history_value(), "v0v1v2");
    EXPECT_EQ(convreq.converter_history_key(10), "k0k1k2");
    EXPECT_EQ(convreq.converter_history_value(10), "v0v1v2");
    EXPECT_EQ(convreq.converter_history_key(0), "");
    EXPECT_EQ(convreq.converter_history_value(0), "");
    EXPECT_EQ(convreq.converter_history_key(2), "k1k2");
    EXPECT_EQ(convreq.converter_history_value(2), "v1v2");
    EXPECT_EQ(convreq.converter_history_key(1), "k2");
    EXPECT_EQ(convreq.converter_history_value(1), "v2");
  }

  {
    const ConversionRequest convreq = ConversionRequestBuilder().Build();
    EXPECT_EQ(convreq.converter_history_key(), "");
    EXPECT_EQ(convreq.converter_history_value(), "");
    EXPECT_EQ(convreq.converter_history_key(10), "");
    EXPECT_EQ(convreq.converter_history_value(10), "");
    EXPECT_EQ(convreq.converter_history_key(0), "");
    EXPECT_EQ(convreq.converter_history_value(0), "");
    EXPECT_EQ(convreq.converter_history_key(2), "");
    EXPECT_EQ(convreq.converter_history_value(2), "");
    EXPECT_EQ(convreq.converter_history_key(1), "");
    EXPECT_EQ(convreq.converter_history_value(1), "");
  }
}

TEST(ConversionRequestTest, IsZeroQuerySuggestionTest) {
  // Segments are not set => use key().
  EXPECT_TRUE(ConversionRequestBuilder().Build().IsZeroQuerySuggestion());
  EXPECT_FALSE(
      ConversionRequestBuilder().SetKey("key").Build().IsZeroQuerySuggestion());

  // Segments are specified => use segments.key()
  Segments segments;
  segments.InitForConvert("");
  EXPECT_TRUE(ConversionRequestBuilder()
                  .SetHistorySegmentsView(segments)
                  .Build()
                  .IsZeroQuerySuggestion());

  EXPECT_TRUE(ConversionRequestBuilder()
                  .SetHistorySegmentsView(segments)
                  .SetKey("key")
                  .Build()
                  .IsZeroQuerySuggestion());

  segments.InitForConvert("key");
  EXPECT_FALSE(ConversionRequestBuilder()
                   .SetHistorySegmentsView(segments)
                   .SetKey("")
                   .Build()
                   .IsZeroQuerySuggestion());

  EXPECT_FALSE(ConversionRequestBuilder()
                   .SetHistorySegmentsView(segments)
                   .SetKey("key")
                   .Build()
                   .IsZeroQuerySuggestion());
}

TEST(ConversionRequestTest, ConverterKeyTest) {
  EXPECT_EQ(ConversionRequestBuilder().SetKey("foo").Build().converter_key(),
            "foo");
  EXPECT_EQ(ConversionRequestBuilder().Build().converter_key(), "");

  Segments segments;
  segments.InitForConvert("bar");
  EXPECT_EQ(ConversionRequestBuilder()
                .SetHistorySegmentsView(segments)
                .SetKey("bar")  // key must be the same as Segment key.
                .Build()
                .converter_key(),
            "bar");

#ifdef NDEBUG
  EXPECT_EQ(ConversionRequestBuilder()
                .SetHistorySegmentsView(segments)
                .Build()
                .converter_key(),
            "bar");
#endif  // NDEBUG
}

TEST(ConversionRequestTest, IncognitoModeTest) {
  {
    const ConversionRequest convreq = ConversionRequestBuilder().Build();
    EXPECT_FALSE(convreq.incognito_mode());
  }
  {
    config::Config config;
    config.set_incognito_mode(true);
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetConfig(config).Build();
    EXPECT_TRUE(convreq.incognito_mode());
  }
  {
    commands::Request request;
    request.set_is_incognito_mode(true);
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_TRUE(convreq.incognito_mode());
  }
  {
    ConversionRequest::Options options;
    options.incognito_mode = true;
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetOptions(std::move(options)).Build();
    EXPECT_TRUE(convreq.incognito_mode());
  }
  {
    config::Config config;
    config.set_incognito_mode(true);
    commands::Request request;
    request.set_is_incognito_mode(false);
    ConversionRequest::Options options;
    options.incognito_mode = false;
    const ConversionRequest convreq = ConversionRequestBuilder()
                                          .SetConfig(config)
                                          .SetRequest(request)
                                          .SetOptions(std::move(options))
                                          .Build();
    EXPECT_TRUE(convreq.incognito_mode());
  }
  {
    config::Config config;
    config.set_incognito_mode(false);
    commands::Request request;
    request.set_is_incognito_mode(true);
    ConversionRequest::Options options;
    options.incognito_mode = false;
    const ConversionRequest convreq = ConversionRequestBuilder()
                                          .SetConfig(config)
                                          .SetRequest(request)
                                          .SetOptions(std::move(options))
                                          .Build();
    EXPECT_TRUE(convreq.incognito_mode());
  }
  {
    config::Config config;
    config.set_incognito_mode(false);
    commands::Request request;
    request.set_is_incognito_mode(false);
    ConversionRequest::Options options;
    options.incognito_mode = true;
    const ConversionRequest convreq = ConversionRequestBuilder()
                                          .SetConfig(config)
                                          .SetRequest(request)
                                          .SetOptions(std::move(options))
                                          .Build();
    EXPECT_TRUE(convreq.incognito_mode());
  }
}
}  // namespace mozc

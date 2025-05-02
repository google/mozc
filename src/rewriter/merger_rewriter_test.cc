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

#include "rewriter/merger_rewriter.h"

#include <cstddef>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

// Tests in which order methods of each instance should be called
// and what value should be returned.
class TestRewriter : public RewriterInterface {
 public:
  TestRewriter(std::string *buffer, const absl::string_view name,
               bool return_value)
      : buffer_(buffer),
        name_(name),
        return_value_(return_value),
        capability_(RewriterInterface::CONVERSION) {}

  TestRewriter(std::string *buffer, const absl::string_view name,
               bool return_value, int capability)
      : buffer_(buffer),
        name_(name),
        return_value_(return_value),
        capability_(capability) {}

  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override {
    buffer_->append(name_ + ".Rewrite();");
    return return_value_;
  }

  virtual void set_capability(int capability) { capability_ = capability; }

  int capability(const ConversionRequest &request) const override {
    return capability_;
  }

  bool Focus(Segments *segments, size_t segment_index,
             int candidate_index) const override {
    buffer_->append(name_ + ".Focus();");
    return return_value_;
  }

  void Finish(const ConversionRequest &request,
              const Segments &segments) override {
    buffer_->append(name_ + ".Finish();");
  }

  void Revert(const Segments &segments) override {
    buffer_->append(name_ + ".Revert();");
  }

  bool Sync() override {
    buffer_->append(name_ + ".Sync();");
    return return_value_;
  }

  bool Reload() override {
    buffer_->append(name_ + ".Reload();");
    return return_value_;
  }

  void Clear() override { buffer_->append(name_ + ".Clear();"); }

 private:
  std::string *buffer_;
  const std::string name_;
  const bool return_value_;
  int capability_;
};

class MergerRewriterTest : public testing::TestWithTempUserProfile {};

ConversionRequest ConvReq(ConversionRequest::RequestType request_type) {
  return ConversionRequestBuilder().SetRequestType(request_type).Build();
}

TEST_F(MergerRewriterTest, Rewrite) {
  std::string call_result;
  MergerRewriter merger;
  Segments segments;
  const ConversionRequest request;

  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "a", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "b", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "c", false));
  EXPECT_FALSE(merger.Rewrite(request, &segments));
  EXPECT_EQ(call_result,
            "a.Rewrite();"
            "b.Rewrite();"
            "c.Rewrite();");
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "d", true));
  call_result.clear();
  EXPECT_TRUE(merger.Rewrite(request, &segments));
  EXPECT_EQ(call_result,
            "a.Rewrite();"
            "b.Rewrite();"
            "c.Rewrite();"
            "d.Rewrite();");
}

TEST_F(MergerRewriterTest, RewriteSuggestion) {
  std::string call_result;
  MergerRewriter merger;
  Segments segments;
  const ConversionRequest request = ConvReq(ConversionRequest::SUGGESTION);

  merger.AddRewriter(std::make_unique<TestRewriter>(
      &call_result, "a", true, RewriterInterface::SUGGESTION));

  EXPECT_EQ(segments.conversion_segments_size(), 0);
  Segment *segment = segments.push_back_segment();
  EXPECT_EQ(segments.conversion_segments_size(), 1);

  EXPECT_EQ(segment->candidates_size(), 0);
  segment->push_back_candidate();
  segment->push_back_candidate();
  segment->push_back_candidate();
  segment->push_back_candidate();
  EXPECT_EQ(segment->candidates_size(), 4);
  EXPECT_EQ(request.config().suggestions_size(), 3);

  EXPECT_TRUE(merger.Rewrite(request, &segments));
  EXPECT_EQ(call_result, "a.Rewrite();");

  EXPECT_EQ(segment->candidates_size(), 3);
}

TEST_F(MergerRewriterTest, RewriteSuggestionWithMixedConversion) {
  std::string call_result;
  MergerRewriter merger;
  Segments segments;

  // Initialize a ConversionRequest with mixed_conversion == true, which
  // should result that the merger rewriter does not trim exceeded suggestions.
  commands::Request commands_request;
  commands_request.set_mixed_conversion(true);
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetRequest(commands_request)
          .SetRequestType(ConversionRequest::SUGGESTION)
          .Build();
  EXPECT_TRUE(request.request().mixed_conversion());

  merger.AddRewriter(std::make_unique<TestRewriter>(
      &call_result, "a", true, RewriterInterface::SUGGESTION));

  EXPECT_EQ(segments.conversion_segments_size(), 0);
  Segment *segment = segments.push_back_segment();
  EXPECT_EQ(segments.conversion_segments_size(), 1);

  EXPECT_EQ(segment->candidates_size(), 0);
  segment->push_back_candidate();
  segment->push_back_candidate();
  segment->push_back_candidate();
  segment->push_back_candidate();
  EXPECT_EQ(segment->candidates_size(), 4);
  EXPECT_EQ(request.config().suggestions_size(), 3);

  EXPECT_TRUE(merger.Rewrite(request, &segments));
  EXPECT_EQ(call_result, "a.Rewrite();");

  // If mixed_conversion is true, the suggestions are not deleted.
  EXPECT_EQ(segment->candidates_size(), 4);
}

TEST_F(MergerRewriterTest, RewriteCheckTest) {
  std::string call_result;
  MergerRewriter merger;
  Segments segments;
  merger.AddRewriter(std::make_unique<TestRewriter>(
      &call_result, "a", false, RewriterInterface::CONVERSION));
  merger.AddRewriter(std::make_unique<TestRewriter>(
      &call_result, "b", false, RewriterInterface::SUGGESTION));
  merger.AddRewriter(std::make_unique<TestRewriter>(
      &call_result, "c", false, RewriterInterface::PREDICTION));
  merger.AddRewriter(std::make_unique<TestRewriter>(
      &call_result, "d", false,
      RewriterInterface::PREDICTION | RewriterInterface::CONVERSION));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "e", false,
                                                    RewriterInterface::ALL));

  const ConversionRequest request_conversion =
      ConvReq(ConversionRequest::CONVERSION);
  EXPECT_FALSE(merger.Rewrite(request_conversion, &segments));
  EXPECT_EQ(
      "a.Rewrite();"
      "d.Rewrite();"
      "e.Rewrite();",
      call_result);
  call_result.clear();

  const ConversionRequest request_prediction =
      ConvReq(ConversionRequest::PREDICTION);
  EXPECT_FALSE(merger.Rewrite(request_prediction, &segments));
  EXPECT_EQ(call_result,
            "c.Rewrite();"
            "d.Rewrite();"
            "e.Rewrite();");
  call_result.clear();

  const ConversionRequest request_suggestion =
      ConvReq(ConversionRequest::SUGGESTION);
  EXPECT_FALSE(merger.Rewrite(request_suggestion, &segments));
  EXPECT_EQ(call_result,
            "b.Rewrite();"
            "e.Rewrite();");
  call_result.clear();

  const ConversionRequest request_partial_suggestion =
      ConvReq(ConversionRequest::PARTIAL_SUGGESTION);
  EXPECT_FALSE(merger.Rewrite(request_partial_suggestion, &segments));
  EXPECT_EQ(call_result,
            "b.Rewrite();"
            "e.Rewrite();");
  call_result.clear();

  const ConversionRequest request_partial_prediction =
      ConvReq(ConversionRequest::PARTIAL_PREDICTION);
  EXPECT_FALSE(merger.Rewrite(request_partial_prediction, &segments));
  EXPECT_EQ(call_result,
            "c.Rewrite();"
            "d.Rewrite();"
            "e.Rewrite();");
  call_result.clear();
}

TEST_F(MergerRewriterTest, Focus) {
  std::string call_result;
  MergerRewriter merger;
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "a", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "b", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "c", false));
  EXPECT_FALSE(merger.Focus(nullptr, 0, 0));
  EXPECT_EQ(call_result,
            "a.Focus();"
            "b.Focus();"
            "c.Focus();");
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "d", true));
  call_result.clear();
  EXPECT_TRUE(merger.Focus(nullptr, 0, 0));
  EXPECT_EQ(call_result,
            "a.Focus();"
            "b.Focus();"
            "c.Focus();"
            "d.Focus();");
}

TEST_F(MergerRewriterTest, Finish) {
  std::string call_result;
  const ConversionRequest request;
  const Segments segments;
  MergerRewriter merger;
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "a", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "b", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "c", false));
  merger.Finish(request, segments);
  EXPECT_EQ(call_result,
            "a.Finish();"
            "b.Finish();"
            "c.Finish();");
}

TEST_F(MergerRewriterTest, Revert) {
  std::string call_result;
  const ConversionRequest request;
  const Segments segments;
  MergerRewriter merger;
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "a", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "b", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "c", false));
  merger.Revert(segments);
  EXPECT_EQ(call_result,
            "a.Revert();"
            "b.Revert();"
            "c.Revert();");
}

TEST_F(MergerRewriterTest, Sync) {
  std::string call_result;
  MergerRewriter merger;
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "a", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "b", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "c", false));
  EXPECT_FALSE(merger.Sync());
  EXPECT_EQ(call_result,
            "a.Sync();"
            "b.Sync();"
            "c.Sync();");
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "d", true));
  call_result.clear();
  EXPECT_TRUE(merger.Sync());
  EXPECT_EQ(call_result,
            "a.Sync();"
            "b.Sync();"
            "c.Sync();"
            "d.Sync();");
}

TEST_F(MergerRewriterTest, Reload) {
  std::string call_result;
  MergerRewriter merger;
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "a", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "b", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "c", false));
  EXPECT_FALSE(merger.Reload());
  EXPECT_EQ(call_result,
            "a.Reload();"
            "b.Reload();"
            "c.Reload();");
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "d", true));
  call_result.clear();
  EXPECT_TRUE(merger.Reload());
  EXPECT_EQ(call_result,
            "a.Reload();"
            "b.Reload();"
            "c.Reload();"
            "d.Reload();");
}

TEST_F(MergerRewriterTest, Clear) {
  std::string call_result;
  MergerRewriter merger;
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "a", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "b", false));
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "c", false));
  merger.Clear();
  EXPECT_EQ(call_result,
            "a.Clear();"
            "b.Clear();"
            "c.Clear();");
  merger.AddRewriter(std::make_unique<TestRewriter>(&call_result, "d", true));
  call_result.clear();
  merger.Clear();
  EXPECT_EQ(call_result,
            "a.Clear();"
            "b.Clear();"
            "c.Clear();"
            "d.Clear();");
}

}  // namespace
}  // namespace mozc

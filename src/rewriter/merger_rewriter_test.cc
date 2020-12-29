// Copyright 2010-2020, Google Inc.
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

#include <string>

#include "base/system_util.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {

// Tests in which order methods of each instance should be called
// and what value should be returned.
class TestRewriter : public RewriterInterface {
 public:
  TestRewriter(std::string *buffer, const std::string &name, bool return_value)
      : buffer_(buffer),
        name_(name),
        return_value_(return_value),
        capability_(RewriterInterface::CONVERSION) {}

  TestRewriter(std::string *buffer, const std::string &name, bool return_value,
               int capability)
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

  void Finish(const ConversionRequest &request, Segments *segments) override {
    buffer_->append(name_ + ".Finish();");
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

class MergerRewriterTest : public testing::Test {
 protected:
  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }
};

TEST_F(MergerRewriterTest, Rewrite) {
  std::string call_result;
  MergerRewriter merger;
  Segments segments;
  const ConversionRequest request;

  segments.set_request_type(Segments::CONVERSION);
  merger.AddRewriter(new TestRewriter(&call_result, "a", false));
  merger.AddRewriter(new TestRewriter(&call_result, "b", false));
  merger.AddRewriter(new TestRewriter(&call_result, "c", false));
  EXPECT_FALSE(merger.Rewrite(request, &segments));
  EXPECT_EQ(
      "a.Rewrite();"
      "b.Rewrite();"
      "c.Rewrite();",
      call_result);
  merger.AddRewriter(new TestRewriter(&call_result, "d", true));
  call_result.clear();
  EXPECT_TRUE(merger.Rewrite(request, &segments));
  EXPECT_EQ(
      "a.Rewrite();"
      "b.Rewrite();"
      "c.Rewrite();"
      "d.Rewrite();",
      call_result);
}

TEST_F(MergerRewriterTest, RewriteSuggestion) {
  std::string call_result;
  MergerRewriter merger;
  Segments segments;
  const ConversionRequest request;

  segments.set_request_type(Segments::SUGGESTION);
  merger.AddRewriter(
      new TestRewriter(&call_result, "a", true, RewriterInterface::SUGGESTION));

  EXPECT_EQ(0, segments.conversion_segments_size());
  Segment *segment = segments.push_back_segment();
  EXPECT_EQ(1, segments.conversion_segments_size());

  EXPECT_EQ(0, segment->candidates_size());
  segment->push_back_candidate();
  segment->push_back_candidate();
  segment->push_back_candidate();
  segment->push_back_candidate();
  EXPECT_EQ(4, segment->candidates_size());
  EXPECT_EQ(3, request.config().suggestions_size());

  EXPECT_TRUE(merger.Rewrite(request, &segments));
  EXPECT_EQ("a.Rewrite();", call_result);

  EXPECT_EQ(3, segment->candidates_size());
}

TEST_F(MergerRewriterTest, RewriteSuggestionWithMixedConversion) {
  std::string call_result;
  MergerRewriter merger;
  Segments segments;

  // Initialize a ConversionRequest with mixed_conversion == true, which
  // should result that the merger rewriter does not trim exceeded suggestions.
  commands::Request commands_request;
  commands_request.set_mixed_conversion(true);
  ConversionRequest request;
  request.set_request(&commands_request);
  EXPECT_TRUE(request.request().mixed_conversion());

  segments.set_request_type(Segments::SUGGESTION);
  merger.AddRewriter(
      new TestRewriter(&call_result, "a", true, RewriterInterface::SUGGESTION));

  EXPECT_EQ(0, segments.conversion_segments_size());
  Segment *segment = segments.push_back_segment();
  EXPECT_EQ(1, segments.conversion_segments_size());

  EXPECT_EQ(0, segment->candidates_size());
  segment->push_back_candidate();
  segment->push_back_candidate();
  segment->push_back_candidate();
  segment->push_back_candidate();
  EXPECT_EQ(4, segment->candidates_size());
  EXPECT_EQ(3, request.config().suggestions_size());

  EXPECT_TRUE(merger.Rewrite(request, &segments));
  EXPECT_EQ("a.Rewrite();", call_result);

  // If mixed_conversion is true, the suggestions are not deleted.
  EXPECT_EQ(4, segment->candidates_size());
}

TEST_F(MergerRewriterTest, RewriteCheckTest) {
  std::string call_result;
  MergerRewriter merger;
  Segments segments;
  const ConversionRequest request;
  merger.AddRewriter(new TestRewriter(&call_result, "a", false,
                                      RewriterInterface::CONVERSION));
  merger.AddRewriter(new TestRewriter(&call_result, "b", false,
                                      RewriterInterface::SUGGESTION));
  merger.AddRewriter(new TestRewriter(&call_result, "c", false,
                                      RewriterInterface::PREDICTION));
  merger.AddRewriter(new TestRewriter(
      &call_result, "d", false,
      RewriterInterface::PREDICTION | RewriterInterface::CONVERSION));
  merger.AddRewriter(
      new TestRewriter(&call_result, "e", false, RewriterInterface::ALL));

  segments.set_request_type(Segments::CONVERSION);
  EXPECT_FALSE(merger.Rewrite(request, &segments));
  EXPECT_EQ(
      "a.Rewrite();"
      "d.Rewrite();"
      "e.Rewrite();",
      call_result);
  call_result.clear();

  segments.set_request_type(Segments::PREDICTION);
  EXPECT_FALSE(merger.Rewrite(request, &segments));
  EXPECT_EQ(
      "c.Rewrite();"
      "d.Rewrite();"
      "e.Rewrite();",
      call_result);
  call_result.clear();

  segments.set_request_type(Segments::SUGGESTION);
  EXPECT_FALSE(merger.Rewrite(request, &segments));
  EXPECT_EQ(
      "b.Rewrite();"
      "e.Rewrite();",
      call_result);
  call_result.clear();

  segments.set_request_type(Segments::PARTIAL_SUGGESTION);
  EXPECT_FALSE(merger.Rewrite(request, &segments));
  EXPECT_EQ(
      "b.Rewrite();"
      "e.Rewrite();",
      call_result);
  call_result.clear();

  segments.set_request_type(Segments::PARTIAL_PREDICTION);
  EXPECT_FALSE(merger.Rewrite(request, &segments));
  EXPECT_EQ(
      "c.Rewrite();"
      "d.Rewrite();"
      "e.Rewrite();",
      call_result);
  call_result.clear();
}

TEST_F(MergerRewriterTest, Focus) {
  std::string call_result;
  MergerRewriter merger;
  merger.AddRewriter(new TestRewriter(&call_result, "a", false));
  merger.AddRewriter(new TestRewriter(&call_result, "b", false));
  merger.AddRewriter(new TestRewriter(&call_result, "c", false));
  EXPECT_FALSE(merger.Focus(nullptr, 0, 0));
  EXPECT_EQ(
      "a.Focus();"
      "b.Focus();"
      "c.Focus();",
      call_result);
  merger.AddRewriter(new TestRewriter(&call_result, "d", true));
  call_result.clear();
  EXPECT_TRUE(merger.Focus(nullptr, 0, 0));
  EXPECT_EQ(
      "a.Focus();"
      "b.Focus();"
      "c.Focus();"
      "d.Focus();",
      call_result);
}

TEST_F(MergerRewriterTest, Finish) {
  std::string call_result;
  const ConversionRequest request;
  MergerRewriter merger;
  merger.AddRewriter(new TestRewriter(&call_result, "a", false));
  merger.AddRewriter(new TestRewriter(&call_result, "b", false));
  merger.AddRewriter(new TestRewriter(&call_result, "c", false));
  merger.Finish(request, nullptr);
  EXPECT_EQ(
      "a.Finish();"
      "b.Finish();"
      "c.Finish();",
      call_result);
}

TEST_F(MergerRewriterTest, Sync) {
  std::string call_result;
  MergerRewriter merger;
  merger.AddRewriter(new TestRewriter(&call_result, "a", false));
  merger.AddRewriter(new TestRewriter(&call_result, "b", false));
  merger.AddRewriter(new TestRewriter(&call_result, "c", false));
  EXPECT_FALSE(merger.Sync());
  EXPECT_EQ(
      "a.Sync();"
      "b.Sync();"
      "c.Sync();",
      call_result);
  merger.AddRewriter(new TestRewriter(&call_result, "d", true));
  call_result.clear();
  EXPECT_TRUE(merger.Sync());
  EXPECT_EQ(
      "a.Sync();"
      "b.Sync();"
      "c.Sync();"
      "d.Sync();",
      call_result);
}

TEST_F(MergerRewriterTest, Reload) {
  std::string call_result;
  MergerRewriter merger;
  merger.AddRewriter(new TestRewriter(&call_result, "a", false));
  merger.AddRewriter(new TestRewriter(&call_result, "b", false));
  merger.AddRewriter(new TestRewriter(&call_result, "c", false));
  EXPECT_FALSE(merger.Reload());
  EXPECT_EQ(
      "a.Reload();"
      "b.Reload();"
      "c.Reload();",
      call_result);
  merger.AddRewriter(new TestRewriter(&call_result, "d", true));
  call_result.clear();
  EXPECT_TRUE(merger.Reload());
  EXPECT_EQ(
      "a.Reload();"
      "b.Reload();"
      "c.Reload();"
      "d.Reload();",
      call_result);
}

TEST_F(MergerRewriterTest, Clear) {
  std::string call_result;
  MergerRewriter merger;
  merger.AddRewriter(new TestRewriter(&call_result, "a", false));
  merger.AddRewriter(new TestRewriter(&call_result, "b", false));
  merger.AddRewriter(new TestRewriter(&call_result, "c", false));
  merger.Clear();
  EXPECT_EQ(
      "a.Clear();"
      "b.Clear();"
      "c.Clear();",
      call_result);
  merger.AddRewriter(new TestRewriter(&call_result, "d", true));
  call_result.clear();
  merger.Clear();
  EXPECT_EQ(
      "a.Clear();"
      "b.Clear();"
      "c.Clear();"
      "d.Clear();",
      call_result);
}

}  // namespace mozc

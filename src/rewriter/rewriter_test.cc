// Copyright 2010-2012, Google Inc.
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

#include "rewriter/rewriter.h"

#include <string>
#include "base/singleton.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/rewriter_interface.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

size_t CommandCandidatesSize(const Segment &segment) {
  size_t result = 0;
  for (int i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).attributes &
        Segment::Candidate::COMMAND_CANDIDATE) {
      result++;
    }
  }
  return result;
}

}  // namespace

class RewriterTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
    converter_mock_.reset(new ConverterMock);
    const testing::MockDataManager data_manager;
    rewriter_.reset(new RewriterImpl(converter_mock_.get(), &data_manager));
  }

  virtual void TearDown() {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  const RewriterInterface *GetRewriter() const {
    return rewriter_.get();
  }

  scoped_ptr<ConverterMock> converter_mock_;
  scoped_ptr<RewriterImpl> rewriter_;
};

// Command rewriter should be disabled on Android build. b/5851240
TEST_F(RewriterTest, CommandRewriterAvailability) {
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    // seg->set_key("こまんど");
    // candidate->value = "コマンド";
    seg->set_key("\xE3\x81\x93\xE3\x81\xBE\xE3\x82\x93\xE3\x81\xA9");
    candidate->value = "\xE3\x82\xB3\xE3\x83\x9E"
                       "\xE3\x83\xB3\xE3\x83\x89";
    EXPECT_TRUE(GetRewriter()->Rewrite(request, &segments));
#ifdef OS_ANDROID
    EXPECT_EQ(0, CommandCandidatesSize(*seg));
#else
    EXPECT_EQ(2, CommandCandidatesSize(*seg));
#endif
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    // seg->set_key("さじぇすと");
    // candidate->value = "サジェスト";
    seg->set_key("\xE3\x81\x95\xE3\x81\x98\xE3\x81\x87"
                 "\xE3\x81\x99\xE3\x81\xA8");
    candidate->value = "\xE3\x82\xB5\xE3\x82\xB8\xE3\x82\xA7"
                       "\xE3\x82\xB9\xE3\x83\x88";
    EXPECT_TRUE(GetRewriter()->Rewrite(request, &segments));
#ifdef OS_ANDROID
    EXPECT_EQ(0, CommandCandidatesSize(*seg));
#else
    EXPECT_EQ(1, CommandCandidatesSize(*seg));
#endif
    seg->clear_candidates();
  }
}

}  // namespace mozc

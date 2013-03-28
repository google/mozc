// Copyright 2010-2013, Google Inc.
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

#include "rewriter/zipcode_rewriter.h"

#include <string>

#include "base/logging.h"
#include "base/system_util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#ifdef MOZC_USE_PACKED_DICTIONARY
#include "data_manager/packed/packed_data_manager.h"
#include "data_manager/packed/packed_data_mock.h"
#endif  // MOZC_USE_PACKED_DICTIONARY
#include "data_manager/user_pos_manager.h"
#include "dictionary/pos_matcher.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

namespace {

enum SegmentType {
  ZIPCODE = 1,
  NON_ZIPCODE = 2,
};

void AddSegment(const string &key, const string &value,
                SegmentType type, Segments *segments) {
  segments->Clear();
  Segment *seg = segments->push_back_segment();
  seg->set_key(key);
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = key;
  candidate->content_key = key;
  candidate->content_value = value;

  if (type == ZIPCODE) {
    const POSMatcher *pos_matcher =
        UserPosManager::GetUserPosManager()->GetPOSMatcher();
    candidate->lid = pos_matcher->GetZipcodeId();
    candidate->rid = pos_matcher->GetZipcodeId();
  }
}

bool HasZipcodeAndAddress(const Segments &segments,
                          const string &expected) {
  CHECK_EQ(segments.segments_size(), 1);
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(0).candidate(i);
    if (candidate.description ==
      // "郵便番号と住所"
      "\xE9\x83\xB5\xE4\xBE\xBF\xE7\x95\xAA\xE5"
      "\x8F\xB7\xE3\x81\xA8\xE4\xBD\x8F\xE6\x89\x80") {
      if (candidate.content_value == expected) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

class ZipcodeRewriterTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
#ifdef MOZC_USE_PACKED_DICTIONARY
    // Registers mocked PackedDataManager.
    scoped_ptr<packed::PackedDataManager>
        data_manager(new packed::PackedDataManager());
    CHECK(data_manager->Init(string(kPackedSystemDictionary_data,
                                    kPackedSystemDictionary_size)));
    packed::RegisterPackedDataManager(data_manager.release());
#endif  // MOZC_USE_PACKED_DICTIONARY

    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config default_config;
    config::ConfigHandler::GetDefaultConfig(&default_config);
    config::ConfigHandler::SetConfig(default_config);
  }

  virtual void TearDown() {
    // restore config to default.
    config::Config default_config;
    config::ConfigHandler::GetDefaultConfig(&default_config);
    config::ConfigHandler::SetConfig(default_config);

#ifdef MOZC_USE_PACKED_DICTIONARY
    // Unregisters mocked PackedDataManager.
    packed::RegisterPackedDataManager(NULL);
#endif  // MOZC_USE_PACKED_DICTIONARY
  }

  ZipcodeRewriter *CreateZipcodeRewriter() const {
    return new ZipcodeRewriter(
        UserPosManager::GetUserPosManager()->GetPOSMatcher());
  }
};

TEST_F(ZipcodeRewriterTest, BasicTest) {
  scoped_ptr<ZipcodeRewriter> zipcode_rewriter(CreateZipcodeRewriter());

  const string kZipcode = "107-0052";
  const string kAddress =
     // "東京都港区赤坂"
     "\xE6\x9D\xB1\xE4\xBA\xAC\xE9\x83\xBD\xE6"
     "\xB8\xAF\xE5\x8C\xBA\xE8\xB5\xA4\xE5\x9D\x82";
  const ConversionRequest default_request;

  {
    Segments segments;
    AddSegment("test", "test", NON_ZIPCODE, &segments);
    EXPECT_FALSE(zipcode_rewriter->Rewrite(default_request, &segments));
  }

  {
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    config.set_space_character_form(
        config::Config::FUNDAMENTAL_HALF_WIDTH);
    config::ConfigHandler::SetConfig(config);

    Segments segments;
    AddSegment(kZipcode, kAddress, ZIPCODE, &segments);
    zipcode_rewriter->Rewrite(default_request, &segments);
    EXPECT_TRUE(HasZipcodeAndAddress(segments,
                                     kZipcode + " " + kAddress));
  }

  {
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    config.set_space_character_form(
        config::Config::FUNDAMENTAL_FULL_WIDTH);
    config::ConfigHandler::SetConfig(config);

    Segments segments;
    AddSegment(kZipcode, kAddress, ZIPCODE, &segments);
    zipcode_rewriter->Rewrite(default_request, &segments);
    EXPECT_TRUE(HasZipcodeAndAddress(segments,
                                     // "　" (full-width space)
                                     kZipcode + "\xE3\x80\x80" + kAddress));
  }
}

}  // namespace mozc

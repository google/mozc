// Copyright 2010, Google Inc.
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

#include "prediction/dictionary_predictor.h"

#include "base/util.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "session/config.pb.h"
#include "session/config_handler.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

void MakeSegmentsForSuggestion(const string key,
                               Segments *segments) {
  segments->set_max_prediction_candidates_size(10);
  segments->set_request_type(Segments::SUGGESTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FIXED_VALUE);
}

TEST(DictionaryPredictor, DictionaryPredictorTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  DictionaryPredictor predictor;

  // turn off
  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(false);
  config::ConfigHandler::SetConfig(config);


  MakeSegmentsForSuggestion
      ("\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B\xE3\x81\x82",
       &segments);
  EXPECT_FALSE(predictor.Predict(&segments));

  // turn on
  config.set_use_dictionary_suggest(true);
  MakeSegmentsForSuggestion
      ("\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B\xE3\x81\x82",
       &segments);
  EXPECT_FALSE(predictor.Predict(&segments));
}

TEST(DictionaryPredictor, IsZipCodeRequestTest) {
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("000"));
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest("ABC"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("---"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("0124-"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("0124-0"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("012-0"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("012-3456"));
  // "０１２-０"
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest(
      "\xef\xbc\x90\xef\xbc\x91\xef\xbc\x92\x2d\xef\xbc\x90"));
}
}  // namespace
}  // mozc

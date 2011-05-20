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

#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "converter/segments.h"
#include "converter/converter_interface.h"
#include "converter/quality_regression_util.h"
#include "session/config_handler.h"
#include "session/config.pb.h"

namespace mozc {
namespace quality_regression {
namespace {

const char kConversionExpect[]    = "Conversion Expect";
const char kConversionNotExpect[] = "Conversion Not Expect";
const char kReverseConversionExpect[]    = "ReverseConversion Expect";
const char kReverseConversionNotExpect[] = "ReverseConversion Not Expect";
const char kPredictionExpect[]    = "Prediction Expect";
const char kPredictionNotExpect[] = "Prediction Not Expect";

// copied from evaluation/quality_regression/evaluator.cc
int GetRank(const string &value, const Segments *segments,
              size_t current_pos, size_t current_segment) {
  if (current_segment == segments->segments_size()) {
    if (current_pos == value.size()) {
      return 0;
    } else {
      return -1;
    }
  }
  const Segment &seg = segments->segment(current_segment);
  for (size_t i = 0; i < seg.candidates_size(); ++i) {
    const string &cand_value = seg.candidate(i).value;
    const size_t len = cand_value.size();
    if (current_pos + len > value.size()) {
      continue;
    }
    if (strncmp(cand_value.c_str(),
                value.c_str() + current_pos, len) != 0) {
      continue;
    }
    const int rest = GetRank(value, segments,
                             current_pos + len, current_segment + 1);
    if (rest == -1) {
      continue;
    }
    return i + rest;
  }
  return -1;
}
}   // namespace

QualityRegressionUtil::QualityRegressionUtil()
    : converter_(ConverterFactory::GetConverter()),
      segments_(new Segments) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config::ConfigHandler::SetConfig(config);
}

QualityRegressionUtil::~QualityRegressionUtil() {}

bool QualityRegressionUtil::ConvertAndTest(const string &key,
                                           const string &expected_value,
                                           const string &command,
                                           int expected_rank,
                                           string *actual_value) {
  CHECK(actual_value);
  segments_->Clear();
  converter_->ResetConversion(segments_.get());
  actual_value->clear();

  if (command == kConversionExpect ||
      command == kConversionNotExpect) {
    converter_->StartConversion(segments_.get(), key);
  } else if (command == kReverseConversionExpect ||
    command == kReverseConversionNotExpect) {
    converter_->StartReverseConversion(segments_.get(), key);
  } else if (command == kPredictionExpect ||
             command == kPredictionNotExpect) {
    converter_->StartPrediction(segments_.get(), key);
  } else {
    LOG(FATAL) << "Unknown command: " << command;
  }

  // No results is OK if "prediction not expect" command
  if (command == kPredictionNotExpect &&
      (segments_->segments_size() == 0 ||
       (segments_->segments_size() >= 1 &&
        segments_->segment(0).candidates_size() == 0))) {
    return true;
  }

  for (size_t i = 0; i < segments_->segments_size(); ++i) {
    *actual_value += segments_->segment(i).candidate(0).value;
  }

  const int32 actual_rank = GetRank(expected_value, segments_.get(),
                                    0, 0);

  bool result = (actual_rank >= 0 && actual_rank <= expected_rank);

  if (command == kConversionNotExpect ||
      command == kReverseConversionNotExpect ||
      command == kPredictionNotExpect) {
    result = !result;
  }

  return result;
}
}   // quality_regression
}   // mozc

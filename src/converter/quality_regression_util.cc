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
#include <sstream>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "converter/segments.h"
#include "converter/converter_interface.h"
#include "converter/quality_regression_util.h"

namespace mozc {
namespace quality_regression {
namespace {

const char kConversionExpect[]    = "Conversion Expected";
const char kConversionNotExpect[] = "Conversion Not Expected";
const char kReverseConversionExpect[]    = "ReverseConversion Expected";
const char kReverseConversionNotExpect[] = "ReverseConversion Not Expected";
const char kPredictionExpect[]    = "Prediction Expected";
const char kPredictionNotExpect[] = "Prediction Not Expected";

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

uint32 GetPlatfromFromString(const string &str) {
  string lower = str;
  Util::LowerString(&lower);
  if (str == "desktop") {
    return QualityRegressionUtil::DESKTOP;
  }
  if (str == "oss") {
    return QualityRegressionUtil::OSS;
  }
  LOG(ERROR) << "Unknown platform name: " << str;
  return QualityRegressionUtil::DESKTOP;
}
}   // namespace

string QualityRegressionUtil::TestItem::OutputAsTSV() const {
  ostringstream os;
  os << label << '\t' << key << '\t' << expected_value << '\t'
     << command << '\t' << expected_rank << '\t' << accuracy
     << '\t' << platform;
  // TODO(toshiyuki): platform enum to string
  return os.str();
}

bool QualityRegressionUtil::TestItem::ParseFromTSV(const string &line) {
  vector<string> tokens;
  Util::SplitStringUsing(line, "\t", &tokens);
  if (tokens.size() < 6) {
    return false;
  }
  label          = tokens[0];
  key            = tokens[1];
  TextNormalizer::NormalizeCandidateText(tokens[2], &expected_value);
  command        = tokens[3];
  expected_rank  = atoi(tokens[4].c_str());
  accuracy       = atof(tokens[5].c_str());
  platform       = 0;
  if (tokens.size() >= 7) {
    vector<string> platforms;
    Util::SplitStringUsing(tokens[6], ",", &platforms);
    for (size_t i = 0; i < platforms.size(); ++i) {
      platform |= GetPlatfromFromString(platforms[i]);
    }
  } else {
    // Default platform: desktop
    platform = QualityRegressionUtil::DESKTOP;
  }
  return true;
}

QualityRegressionUtil::QualityRegressionUtil()
    : converter_(ConverterFactory::GetConverter()),
      segments_(new Segments) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config::ConfigHandler::SetConfig(config);
}

QualityRegressionUtil::QualityRegressionUtil(ConverterInterface *converter)
    : converter_(converter),
      segments_(new Segments) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config::ConfigHandler::SetConfig(config);
}

QualityRegressionUtil::~QualityRegressionUtil() {}

// static
bool QualityRegressionUtil::ParseFile(const string &filename,
                                      vector<TestItem> *outputs) {
  // TODO(taku): support an XML file of Mozcsu.
  outputs->clear();
  InputFileStream ifs(filename.c_str());
  if (!ifs) {
    return false;
  }
  string line;
  while (getline(ifs, line)) {
    if (line.empty() || line.c_str()[0] == '#') {
      continue;
    }
    TestItem item;
    if (!item.ParseFromTSV(line)) {
      LOG(ERROR) << "Cannot parse: " << line;
      return false;
    }
    outputs->push_back(item);
  }
  return true;
}

// static

bool QualityRegressionUtil::ConvertAndTest(const TestItem &item,
                                           string *actual_value) {
  const string &key = item.key;
  const string &expected_value = item.expected_value;
  const string &command = item.command;
  const int expected_rank = item.expected_rank;

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

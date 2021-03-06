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

#include "converter/quality_regression_util.h"

#include <cstdint>
#include <sstream>  // NOLINT
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace quality_regression {
namespace {

const char kConversionExpect[] = "Conversion Expected";
const char kConversionNotExpect[] = "Conversion Not Expected";
const char kReverseConversionExpect[] = "ReverseConversion Expected";
const char kReverseConversionNotExpect[] = "ReverseConversion Not Expected";
// For now, suggestion and prediction are using same implementation
const char kPredictionExpect[] = "Prediction Expected";
const char kPredictionNotExpect[] = "Prediction Not Expected";
const char kSuggestionExpect[] = "Suggestion Expected";
const char kSuggestionNotExpect[] = "Suggestion Not Expected";

// copied from evaluation/quality_regression/evaluator.cc
int GetRank(const std::string &value, const Segments *segments,
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
    const std::string &cand_value = seg.candidate(i).value;
    const size_t len = cand_value.size();
    if (current_pos + len > value.size()) {
      continue;
    }
    if (strncmp(cand_value.c_str(), value.c_str() + current_pos, len) != 0) {
      continue;
    }
    const int rest =
        GetRank(value, segments, current_pos + len, current_segment + 1);
    if (rest == -1) {
      continue;
    }
    return i + rest;
  }
  return -1;
}

uint32_t GetPlatformFromString(absl::string_view str) {
  std::string lower;
  lower.assign(str.data(), str.size());
  Util::LowerString(&lower);
  if (str == "desktop") {
    return QualityRegressionUtil::DESKTOP;
  }
  if (str == "oss") {
    return QualityRegressionUtil::OSS;
  }
  if (str == "mobile") {
    return QualityRegressionUtil::MOBILE;
  }
  if (str == "mobile_ambiguous") {
    return QualityRegressionUtil::MOBILE_AMBIGUOUS;
  }
  LOG(FATAL) << "Unknown platform name: " << str;
  return QualityRegressionUtil::DESKTOP;
}
}  // namespace

std::string QualityRegressionUtil::TestItem::OutputAsTSV() const {
  std::ostringstream os;
  os << label << '\t' << key << '\t' << expected_value << '\t' << command
     << '\t' << expected_rank << '\t' << accuracy << '\t' << platform;
  // TODO(toshiyuki): platform enum to string
  return os.str();
}

bool QualityRegressionUtil::TestItem::ParseFromTSV(const std::string &line) {
  std::vector<absl::string_view> tokens;
  Util::SplitStringUsing(line, "\t", &tokens);
  if (tokens.size() < 6) {
    return false;
  }
  label.assign(tokens[0].data(), tokens[0].size());
  key.assign(tokens[1].data(), tokens[1].size());
  TextNormalizer::NormalizeText(tokens[2], &expected_value);
  command.assign(tokens[3].data(), tokens[3].size());
  expected_rank = NumberUtil::SimpleAtoi(tokens[4]);
  NumberUtil::SafeStrToDouble(tokens[5], &accuracy);
  platform = 0;
  if (tokens.size() >= 7) {
    std::vector<absl::string_view> platforms;
    Util::SplitStringUsing(tokens[6], ",", &platforms);
    for (size_t i = 0; i < platforms.size(); ++i) {
      platform |= GetPlatformFromString(platforms[i]);
    }
  } else {
    // Default platform: desktop
    platform = QualityRegressionUtil::DESKTOP;
  }
  return true;
}

QualityRegressionUtil::QualityRegressionUtil(ConverterInterface *converter)
    : converter_(converter),
      request_(new commands::Request),
      config_(new config::Config),
      segments_(new Segments) {}

QualityRegressionUtil::~QualityRegressionUtil() {}

// static
bool QualityRegressionUtil::ParseFile(const std::string &filename,
                                      std::vector<TestItem> *outputs) {
  // TODO(taku): support an XML file of Mozcsu.
  outputs->clear();
  InputFileStream ifs(filename.c_str());
  if (!ifs.good()) {
    return false;
  }
  std::string line;
  while (!std::getline(ifs, line).fail()) {
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
                                           std::string *actual_value) {
  const std::string &key = item.key;
  const std::string &expected_value = item.expected_value;
  const std::string &command = item.command;
  const int expected_rank = item.expected_rank;

  CHECK(actual_value);
  segments_->Clear();
  converter_->ResetConversion(segments_.get());
  actual_value->clear();

  composer::Table table;

  if (command == kConversionExpect || command == kConversionNotExpect) {
    composer::Composer composer(&table, request_.get(), config_.get());
    composer.SetPreeditTextForTestOnly(key);
    ConversionRequest request(&composer, request_.get(), config_.get());
    converter_->StartConversionForRequest(request, segments_.get());
  } else if (command == kReverseConversionExpect ||
             command == kReverseConversionNotExpect) {
    converter_->StartReverseConversion(segments_.get(), key);
  } else if (command == kPredictionExpect || command == kPredictionNotExpect) {
    composer::Composer composer(&table, request_.get(), config_.get());
    composer.SetPreeditTextForTestOnly(key);
    ConversionRequest request(&composer, request_.get(), config_.get());
    converter_->StartPredictionForRequest(request, segments_.get());
  } else if (command == kSuggestionExpect || command == kSuggestionNotExpect) {
    composer::Composer composer(&table, request_.get(), config_.get());
    composer.SetPreeditTextForTestOnly(key);
    ConversionRequest request(&composer, request_.get(), config_.get());
    converter_->StartSuggestionForRequest(request, segments_.get());
  } else {
    LOG(FATAL) << "Unknown command: " << command;
  }

  // No results is OK if "prediction not expect" command
  if ((command == kPredictionNotExpect || command == kSuggestionNotExpect) &&
      (segments_->segments_size() == 0 ||
       (segments_->segments_size() >= 1 &&
        segments_->segment(0).candidates_size() == 0))) {
    return true;
  }

  for (size_t i = 0; i < segments_->segments_size(); ++i) {
    *actual_value += segments_->segment(i).candidate(0).value;
  }

  const int32_t actual_rank = GetRank(expected_value, segments_.get(), 0, 0);

  bool result = (actual_rank >= 0 && actual_rank <= expected_rank);

  if (command == kConversionNotExpect ||
      command == kReverseConversionNotExpect ||
      command == kPredictionNotExpect || command == kSuggestionNotExpect) {
    result = !result;
  }

  return result;
}

void QualityRegressionUtil::SetRequest(const commands::Request &request) {
  *request_ = request;
}

void QualityRegressionUtil::SetConfig(const config::Config &config) {
  *config_ = config;
}

std::string QualityRegressionUtil::GetPlatformString(
    uint32_t platform_bitfiled) {
  std::vector<std::string> v;
  if (platform_bitfiled & DESKTOP) {
    v.push_back("DESKTOP");
  }
  if (platform_bitfiled & OSS) {
    v.push_back("OSS");
  }
  if (platform_bitfiled & MOBILE) {
    v.push_back("MOBILE");
  }
  if (platform_bitfiled & MOBILE_AMBIGUOUS) {
    v.push_back("MOBILE_AMBIGUOUS");
  }
  if (v.empty()) {
    v.push_back("UNKNOWN");
  }
  std::string s;
  Util::JoinStrings(v, "|", &s);
  return s;
}

}  // namespace quality_regression
}  // namespace mozc

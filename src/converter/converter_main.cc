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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/init_mozc.h"
#include "base/number_util.h"
#include "base/protobuf/text_format.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "converter/lattice.h"
#include "converter/pos_id_printer.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "engine/engine.h"
#include "engine/engine_interface.h"
#include "engine/supplemental_model_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "protocol/engine_builder.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"

#ifndef NDEBUG
#define MOZC_DEBUG
#endif  // NDEBUG

ABSL_FLAG(int32_t, max_conversion_candidates_size, 200,
          "maximum candidates size");
ABSL_FLAG(std::string, user_profile_dir, "", "path to user profile directory");
ABSL_FLAG(std::string, engine_name, "default",
          "Shortcut to select engine_data_path from name: (default|oss|mock)");
ABSL_FLAG(std::string, engine_type, "desktop", "Engine type: (desktop|mobile)");
ABSL_FLAG(bool, output_debug_string, true,
          "output debug string for each input");
ABSL_FLAG(size_t, max_candidates_to_show, 100,
          "Max number of candidates to show per segment");
ABSL_FLAG(bool, show_meta_candidates, false, "if true, show meta candidates");

// Advanced options for data files.  These are automatically set when --engine
// is used but they can be overridden by specifying these flags.
ABSL_FLAG(std::string, engine_data_path, "",
          "Path to engine data file. This overrides the default data path "
          "for engine_name.");
ABSL_FLAG(std::string, magic, "", "Expected magic number of data file");
ABSL_FLAG(std::string, id_def, "",
          "id.def file for POS IDs. If provided, show human readable "
          "POS instead of ID number");
ABSL_FLAG(std::string, decoder_experiment_params, "",
          "If nonempty, a DecoderExperimentParams is parsed from this text "
          "format and it is merged to the default value.");


namespace mozc {
namespace {

// Wrapper class for pos id printing
class PosIdPrintUtil {
 public:
  PosIdPrintUtil(const PosIdPrintUtil &) = delete;
  PosIdPrintUtil &operator=(const PosIdPrintUtil &) = delete;
  static std::string IdToString(int id) {
    return Singleton<PosIdPrintUtil>::get()->IdToStringInternal(id);
  }

 private:
  PosIdPrintUtil()
      : pos_id_printer_(InputFileStream(absl::GetFlag(FLAGS_id_def))) {}

  std::string IdToStringInternal(int id) const {
    const absl::string_view pos_string = pos_id_printer_.IdToString(id);
    if (pos_string.empty()) {
      return absl::StrCat(id);
    }
    return absl::StrFormat("%s (%d)", pos_string, id);
  }

  internal::PosIdPrinter pos_id_printer_;

  friend class Singleton<PosIdPrintUtil>;
};

std::string SegmentTypeToString(Segment::SegmentType type) {
#define RETURN_STR(val) \
  case Segment::val:    \
    return #val
  switch (type) {
    RETURN_STR(FREE);
    RETURN_STR(FIXED_BOUNDARY);
    RETURN_STR(FIXED_VALUE);
    RETURN_STR(SUBMITTED);
    RETURN_STR(HISTORY);
    default:
      return "UNKNOWN";
  }
#undef RETURN_STR
}

std::string CandidateAttributesToString(uint32_t attrs) {
  std::vector<std::string> v;
#define ADD_STR(fieldname)                                              \
  do {                                                                  \
    if (attrs & Segment::Candidate::fieldname) v.push_back(#fieldname); \
  } while (false)

  ADD_STR(BEST_CANDIDATE);
  ADD_STR(RERANKED);
  ADD_STR(NO_HISTORY_LEARNING);
  ADD_STR(NO_SUGGEST_LEARNING);
  ADD_STR(CONTEXT_SENSITIVE);
  ADD_STR(SPELLING_CORRECTION);
  ADD_STR(NO_VARIANTS_EXPANSION);
  ADD_STR(NO_EXTRA_DESCRIPTION);
  ADD_STR(REALTIME_CONVERSION);
  ADD_STR(USER_DICTIONARY);
  ADD_STR(COMMAND_CANDIDATE);
  ADD_STR(PARTIALLY_KEY_CONSUMED);
  ADD_STR(TYPING_CORRECTION);
  ADD_STR(AUTO_PARTIAL_SUGGESTION);
  ADD_STR(USER_HISTORY_PREDICTION);
  ADD_STR(NO_MODIFICATION);
  ADD_STR(USER_SEGMENT_HISTORY_REWRITER);

#undef ADD_STR
  return absl::StrJoin(v, " | ");
}

std::string NumberStyleToString(NumberUtil::NumberString::Style style) {
#define RETURN_STR(val)               \
  case NumberUtil::NumberString::val: \
    return #val
  switch (style) {
    RETURN_STR(DEFAULT_STYLE);
    RETURN_STR(NUMBER_SEPARATED_ARABIC_HALFWIDTH);
    RETURN_STR(NUMBER_SEPARATED_ARABIC_FULLWIDTH);
    RETURN_STR(NUMBER_ARABIC_AND_KANJI_HALFWIDTH);
    RETURN_STR(NUMBER_ARABIC_AND_KANJI_FULLWIDTH);
    RETURN_STR(NUMBER_KANJI);
    RETURN_STR(NUMBER_OLD_KANJI);
    RETURN_STR(NUMBER_ROMAN_CAPITAL);
    RETURN_STR(NUMBER_ROMAN_SMALL);
    RETURN_STR(NUMBER_CIRCLED);
    RETURN_STR(NUMBER_KANJI_ARABIC);
    RETURN_STR(NUMBER_HEX);
    RETURN_STR(NUMBER_OCT);
    RETURN_STR(NUMBER_BIN);
    RETURN_STR(NUMBER_SUPERSCRIPT);
    RETURN_STR(NUMBER_SUBSCRIPT);
    default:
      return "UNKNOWN";
  }
#undef RETURN_STR
}

std::string InnerSegmentBoundaryToString(const Segment::Candidate &cand) {
  if (cand.inner_segment_boundary.empty()) {
    return "";
  }
  std::vector<std::string> pieces;
  for (Segment::Candidate::InnerSegmentIterator iter(&cand); !iter.Done();
       iter.Next()) {
    pieces.push_back(absl::StrCat("<", iter.GetKey(), ", ", iter.GetValue(),
                                  ", ", iter.GetContentKey(), ", ",
                                  iter.GetContentValue(), ">"));
  }
  return absl::StrJoin(pieces, " | ");
}

void PrintCandidate(const Segment &parent, size_t candidates_size, int num,
                    const Segment::Candidate &cand, std::ostream *os) {
  std::vector<std::string> lines;
  if (parent.key() != cand.key) {
    lines.push_back("key: " + cand.key);
  }
  lines.push_back("content_vk: " + cand.content_value + "  " +
                  cand.content_key);
  lines.push_back(absl::StrFormat("cost: %d  scost: %d  wcost: %d", cand.cost,
                                  cand.structure_cost, cand.wcost));
  lines.push_back("lid: " + PosIdPrintUtil::IdToString(cand.lid));
  lines.push_back("rid: " + PosIdPrintUtil::IdToString(cand.rid));
  lines.push_back("attr: " + CandidateAttributesToString(cand.attributes));
  lines.push_back("num_style: " + NumberStyleToString(cand.style));
  const std::string &segbdd_str = InnerSegmentBoundaryToString(cand);
  if (!segbdd_str.empty()) {
    lines.push_back("segbdd: " + segbdd_str);
  }
#ifdef MOZC_DEBUG
  if (!cand.log.empty()) {
    lines.push_back("log:");
    for (absl::string_view line : absl::StrSplit(cand.log, '\n')) {
      if (line.empty()) continue;
      lines.push_back(absl::StrCat("    ", line));
    }
  }
#endif  // MOZC_DEBUG

  (*os) << "  " << num << '/' << candidates_size << " " << cand.value
        << std::endl;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (!lines[i].empty()) {
      (*os) << "       " << lines[i] << std::endl;
    }
  }
}

void PrintSegment(size_t num, size_t segments_size, const Segment &segment,
                  std::ostream *os) {
  (*os) << "---------- Segment " << num << "/" << segments_size << " ["
        << SegmentTypeToString(segment.segment_type()) << "] ----------"
        << std::endl
        << segment.key() << std::endl;
  if (absl::GetFlag(FLAGS_show_meta_candidates)) {
    for (int i = 0; i < segment.meta_candidates_size(); ++i) {
      PrintCandidate(segment, segment.meta_candidates_size(), -i - 1,
                     segment.meta_candidate(i), os);
    }
  }
  const size_t n = std::min(segment.candidates_size(),
                            absl::GetFlag(FLAGS_max_candidates_to_show));
  for (size_t i = 0; i < n; ++i) {
    PrintCandidate(segment, segment.candidates_size(), i, segment.candidate(i),
                   os);
  }
}

void PrintSegments(const Segments &segments, std::ostream *os) {
  for (size_t i = 0; i < segments.segments_size(); ++i) {
    PrintSegment(i, segments.segments_size(), segments.segment(i), os);
  }
}

bool ExecCommand(const ConverterInterface &converter, const std::string &line,
                 const commands::Request &request, config::Config *config,
                 Segments *segments) {
  std::vector<std::string> fields =
      absl::StrSplit(line, absl::ByAnyChar("\t "), absl::SkipEmpty());

#define CHECK_FIELDS_LENGTH(length) \
  if (fields.size() < (length)) {   \
    return false;                   \
  }

  CHECK_FIELDS_LENGTH(1);

  composer::Composer composer(&composer::Table::GetDefaultTable(), &request,
                              config);
  commands::Context context;
  ConversionRequest::Options options = {
      .max_conversion_candidates_size =
          absl::GetFlag(FLAGS_max_conversion_candidates_size),
      .create_partial_candidates = request.auto_partial_suggestion(),
  };

  const std::string &func = fields[0];
  if (func == "startconversion" || func == "start" || func == "s") {
    options.request_type = ConversionRequest::CONVERSION;
    CHECK_FIELDS_LENGTH(2);
    composer.SetPreeditTextForTestOnly(fields[1]);
    const ConversionRequest conversion_request = ConversionRequest(
        composer, request, context, *config, std::move(options));
    return converter.StartConversion(conversion_request, segments);
  } else if (func == "convertwithnodeinfo" || func == "cn") {
    CHECK_FIELDS_LENGTH(5);
    Lattice::SetDebugDisplayNode(
        NumberUtil::SimpleAtoi(fields[2]),  // begin pos
        NumberUtil::SimpleAtoi(fields[3]),  // end pos
        fields[4]);
    composer::Composer composer;
    composer.SetPreeditTextForTestOnly(fields[1]);
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetComposer(composer).Build();
    const bool result = converter.StartConversion(convreq, segments);
    Lattice::ResetDebugDisplayNode();
    return result;
  } else if (func == "reverseconversion" || func == "reverse" || func == "r") {
    CHECK_FIELDS_LENGTH(2);
    return converter.StartReverseConversion(segments, fields[1]);
  } else if (func == "startprediction" || func == "predict" || func == "p") {
    options.request_type = ConversionRequest::PREDICTION;
    if (fields.size() >= 2) {
      composer.SetPreeditTextForTestOnly(fields[1]);
    }
    const ConversionRequest conversion_request = ConversionRequest(
        composer, request, context, *config, std::move(options));
    return converter.StartPrediction(conversion_request, segments);
  } else if (func == "startsuggestion" || func == "suggest") {
    options.request_type = ConversionRequest::SUGGESTION;
    if (fields.size() >= 2) {
      composer.SetPreeditTextForTestOnly(fields[1]);
    }
    const ConversionRequest conversion_request = ConversionRequest(
        composer, request, context, *config, std::move(options));
    return converter.StartPrediction(conversion_request, segments);
  } else if (func == "finishconversion" || func == "finish") {
    options.request_type = ConversionRequest::CONVERSION;
    const ConversionRequest conversion_request = ConversionRequest(
        composer, request, context, *config, std::move(options));
    converter.FinishConversion(conversion_request, segments);
    return true;
  } else if (func == "resetconversion" || func == "reset") {
    converter.ResetConversion(segments);
    return true;
  } else if (func == "cancelconversion" || func == "cancel") {
    converter.CancelConversion(segments);
    return true;
  } else if (func == "commitsegmentvalue" || func == "commit" || func == "c") {
    CHECK_FIELDS_LENGTH(3);
    return converter.CommitSegmentValue(segments,
                                        NumberUtil::SimpleAtoi(fields[1]),
                                        NumberUtil::SimpleAtoi(fields[2]));
  } else if (func == "commitallandfinish") {
    for (int i = 0; i < segments->conversion_segments_size(); ++i) {
      if (segments->conversion_segment(i).segment_type() !=
          Segment::FIXED_VALUE) {
        if (!(converter.CommitSegmentValue(segments, i, 0))) return false;
      }
    }
    options.request_type = ConversionRequest::CONVERSION;
    const ConversionRequest conversion_request = ConversionRequest(
        composer, request, context, *config, std::move(options));
    converter.FinishConversion(conversion_request, segments);
    return true;
  } else if (func == "focussegmentvalue" || func == "focus") {
    CHECK_FIELDS_LENGTH(3);
    return converter.FocusSegmentValue(segments,
                                       NumberUtil::SimpleAtoi(fields[1]),
                                       NumberUtil::SimpleAtoi(fields[2]));
  } else if (func == "commitfirstsegment") {
    CHECK_FIELDS_LENGTH(2);
    std::vector<size_t> singleton_vector;
    singleton_vector.push_back(NumberUtil::SimpleAtoi(fields[1]));
    return converter.CommitSegments(segments, singleton_vector);
  } else if (func == "resizesegment" || func == "resize") {
    options.request_type = ConversionRequest::CONVERSION;
    const ConversionRequest conversion_request = ConversionRequest(
        composer, request, context, *config, std::move(options));
    if (fields.size() == 3) {
      return converter.ResizeSegment(segments, conversion_request,
                                     NumberUtil::SimpleAtoi(fields[1]),
                                     NumberUtil::SimpleAtoi(fields[2]));
    } else if (fields.size() > 3) {
      std::vector<uint8_t> new_arrays;
      for (size_t i = 3; i < fields.size(); ++i) {
        new_arrays.push_back(
            static_cast<uint8_t>(NumberUtil::SimpleAtoi(fields[i])));
      }
      return converter.ResizeSegment(
          segments, conversion_request, NumberUtil::SimpleAtoi(fields[1]),
          NumberUtil::SimpleAtoi(fields[2]), new_arrays);
    }
  } else if (func == "disableuserhistory") {
    config->set_history_learning_level(config::Config::NO_HISTORY);
  } else if (func == "enableuserhistory") {
    config->set_history_learning_level(config::Config::DEFAULT_HISTORY);
  } else {
    LOG(WARNING) << "Unknown command: " << func;
    return false;
  }

#undef CHECK_FIELDS_LENGTH
  return true;
}

std::pair<std::string, std::string> SelectDataFileFromName(
    const std::string &mozc_runfiles_dir, const std::string &engine_name) {
  struct {
    const char *engine_name;
    const char *path;
    const char *magic;
  } kNameAndPath[] = {
      {"default", "data_manager/oss/mozc.data", "\xEFMOZC\r\n"},
      {"oss", "data_manager/oss/mozc.data", "\xEFMOZC\r\n"},
      {"mock", "data_manager/testing/mock_mozc.data", "MOCK"},
  };
  for (const auto &entry : kNameAndPath) {
    if (engine_name == entry.engine_name) {
      return std::pair<std::string, std::string>(
          FileUtil::JoinPath(mozc_runfiles_dir, entry.path), entry.magic);
    }
  }
  return std::pair<std::string, std::string>("", "");
}

std::string SelectIdDefFromName(const std::string &mozc_runfiles_dir,
                                const std::string &engine_name) {
  struct {
    const char *engine_name;
    const char *path;
  } kNameAndPath[] = {
      {"default", "data/dictionary_oss/id.def"},
      {"oss", "data/dictionary_oss/id.def"},
      {"mock", "data/test/dictionary/id.def"},
  };
  for (const auto &entry : kNameAndPath) {
    if (engine_name == entry.engine_name) {
      return FileUtil::JoinPath(mozc_runfiles_dir, entry.path);
    }
  }
  return "";
}

bool IsConsistentEngineNameAndType(const std::string &engine_name,
                                   const std::string &engine_type) {
  using NameAndTypeSet = std::set<std::pair<std::string, std::string>>;
  static const NameAndTypeSet *kConsistentPairs =
      new NameAndTypeSet({
                          {"oss", "desktop"},
                          {"mock", "desktop"},
                          {"mock", "mobile"},
                          {"default", "desktop"},
                          {"", "desktop"},
                          {"", "mobile"}});
  return kConsistentPairs->find(std::make_pair(engine_name, engine_type)) !=
         kConsistentPairs->end();
}

void RunLoop(std::unique_ptr<Engine> engine, commands::Request &&request,
             config::Config &&config) {
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);

  Segments segments;
  std::string line;
  while (!std::getline(std::cin, line).fail()) {
    if (ExecCommand(*converter, line, request, &config, &segments)) {
      if (absl::GetFlag(FLAGS_output_debug_string)) {
        PrintSegments(segments, &std::cout);
      }
    } else {
      std::cout << "ExecCommand() return false" << std::endl;
    }
  }
}

}  // namespace
}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if (!absl::GetFlag(FLAGS_user_profile_dir).empty()) {
    mozc::SystemUtil::SetUserProfileDirectory(
        absl::GetFlag(FLAGS_user_profile_dir));
  }

  std::string mozc_runfiles_dir = ".";
  if (absl::GetFlag(FLAGS_engine_data_path).empty()) {
    const auto path_and_magic = mozc::SelectDataFileFromName(
        mozc_runfiles_dir, absl::GetFlag(FLAGS_engine_name));
    absl::SetFlag(&FLAGS_engine_data_path, path_and_magic.first);
    absl::SetFlag(&FLAGS_magic, path_and_magic.second);
  }
  CHECK(!absl::GetFlag(FLAGS_engine_data_path).empty())
      << "--engine_data_path or --engine is invalid: " << "--engine_data_path="
      << absl::GetFlag(FLAGS_engine_data_path) << " "
      << "--engine_name=" << absl::GetFlag(FLAGS_engine_name);

  if (absl::GetFlag(FLAGS_id_def).empty()) {
    std::string mozc_idfile_dir = ".";
    absl::SetFlag(&FLAGS_id_def,
                  mozc::SelectIdDefFromName(mozc_idfile_dir,
                                            absl::GetFlag(FLAGS_engine_name)));
  }

  std::cout << "Engine type: " << absl::GetFlag(FLAGS_engine_type)
            << "\nData file: " << absl::GetFlag(FLAGS_engine_data_path)
            << "\nid.def: " << absl::GetFlag(FLAGS_id_def) << std::endl;

  absl::StatusOr<std::unique_ptr<mozc::DataManager>> data_manager =
      absl::GetFlag(FLAGS_magic).empty()
          ? mozc::DataManager::CreateFromFile(
                absl::GetFlag(FLAGS_engine_data_path))
          : mozc::DataManager::CreateFromFile(
                absl::GetFlag(FLAGS_engine_data_path),
                absl::GetFlag(FLAGS_magic));
  CHECK_OK(data_manager);

  mozc::config::Config config = mozc::config::ConfigHandler::DefaultConfig();
  mozc::commands::Request request;
  std::unique_ptr<mozc::Engine> engine;
  if (absl::GetFlag(FLAGS_engine_type) == "desktop") {
    engine =
        mozc::Engine::CreateDesktopEngine(*std::move(data_manager)).value();
  } else if (absl::GetFlag(FLAGS_engine_type) == "mobile") {
    engine = mozc::Engine::CreateMobileEngine(*std::move(data_manager)).value();
    mozc::request_test_util::FillMobileRequest(&request);
    config.set_use_kana_modifier_insensitive_conversion(true);
  } else {
    LOG(FATAL) << "Invalid type: --engine_type="
               << absl::GetFlag(FLAGS_engine_type);
    return 0;
  }
  if (const std::string &textproto =
          absl::GetFlag(FLAGS_decoder_experiment_params);
      !textproto.empty()) {
    mozc::commands::DecoderExperimentParams params;
    CHECK(mozc::protobuf::TextFormat::ParseFromString(textproto, &params))
        << "Invalid DecoderExperimentParams: " << textproto;
    request.mutable_decoder_experiment_params()->MergeFrom(params);
    LOG(INFO) << "DecoderExperimentParams was set:\n"
              << request.decoder_experiment_params();
  }

  if (!mozc::IsConsistentEngineNameAndType(absl::GetFlag(FLAGS_engine_name),
                                           absl::GetFlag(FLAGS_engine_type))) {
    LOG(WARNING) << "Engine name and type do not match.";
  }

  mozc::RunLoop(std::move(engine), std::move(request), std::move(config));
  return 0;
}

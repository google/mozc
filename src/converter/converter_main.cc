// Copyright 2010-2014, Google Inc.
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

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "converter/conversion_request.h"
#include "converter/converter_interface.h"
#include "converter/lattice.h"
#include "converter/pos_id_printer.h"
#include "converter/segments.h"
#include "engine/engine_factory.h"
#include "engine/engine_interface.h"
#include "engine/mock_data_engine_factory.h"
#include "session/commands.pb.h"

DEFINE_int32(max_conversion_candidates_size, 200, "maximum candidates size");
DEFINE_string(user_profile_dir, "", "path to user profile directory");
DEFINE_string(engine, "default", "engine: (default, chromeos)");
DEFINE_bool(output_debug_string, true, "output debug string for each input");
DEFINE_bool(show_meta_candidates, false, "if true, show meta candidates");
DEFINE_string(
    id_def,
    "",
    "id.def file for POS IDs. If provided, show human readable "
    "POS instead of ID number");

using mozc::composer::Composer;
using mozc::composer::Table;
using mozc::config::Config;

namespace mozc {
namespace {

// Wrapper class for pos id printing
class PosIdPrintUtil {
 public:
  static string IdToString(int id) {
    return Singleton<PosIdPrintUtil>::get()->IdToStringInternal(id);
  }

 private:
  PosIdPrintUtil() :
      pos_id_(new InputFileStream(FLAGS_id_def.c_str())),
      pos_id_printer_(new internal::PosIdPrinter(pos_id_.get())) {}

  string IdToStringInternal(int id) const {
    const string &pos_string = pos_id_printer_->IdToString(id);
    if (pos_string.empty()) {
      return NumberUtil::SimpleItoa(id);
    }
    return Util::StringPrintf("%s (%d)", pos_string.c_str(), id);
  }

  scoped_ptr<InputFileStream> pos_id_;
  scoped_ptr<internal::PosIdPrinter> pos_id_printer_;

  friend class Singleton<PosIdPrintUtil>;
  DISALLOW_COPY_AND_ASSIGN(PosIdPrintUtil);
};

string SegmentTypeToString(Segment::SegmentType type) {
#define RETURN_STR(val) case Segment::val: return #val
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

string CandidateAttributesToString(uint32 attrs) {
  vector<string> v;
#define ADD_STR(fieldname)                       \
  do {                                           \
    if (attrs & Segment::Candidate::fieldname)   \
      v.push_back(#fieldname);                   \
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

#undef ADD_STR
  string s;
  Util::JoinStrings(v, " | ", &s);
  return s;
}

string NumberStyleToString(NumberUtil::NumberString::Style style) {
#define RETURN_STR(val) case NumberUtil::NumberString::val: return #val
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
    default:
      return "UNKNOWN";
  }
#undef RETURN_STR
}

string InnerSegmentBoundaryToString(const Segment::Candidate &cand) {
  if (cand.inner_segment_boundary.empty()) {
    return "";
  }
  vector<StringPiece> pieces;
  const char *boundary_begin = cand.value.data();
  const char *boundary_end = boundary_begin;
  for (size_t i = 0; i < cand.inner_segment_boundary.size(); ++i) {
    for (int j = 0; j < cand.inner_segment_boundary[i].second; ++j) {
      boundary_end += Util::OneCharLen(boundary_end);
    }
    pieces.push_back(StringPiece(boundary_begin,
                                 boundary_end - boundary_begin));
    boundary_begin = boundary_end;
  }
  CHECK_EQ(cand.value.data() + cand.value.size(), boundary_begin);
  string s;
  Util::JoinStringPieces(pieces, " | ", &s);
  return s;
}

void PrintCandidate(const Segment &parent, int num,
                    const Segment::Candidate &cand, ostream *os) {
  vector<string> lines;
  if (parent.key() != cand.key) {
    lines.push_back("key: " + cand.key);
  }
  lines.push_back("content_vk: " + cand.content_value +
                  "  " + cand.content_key);
  lines.push_back(Util::StringPrintf(
      "cost: %d  scost: %d  wcost: %d",
      cand.cost, cand.structure_cost, cand.wcost));
  lines.push_back("lid: " + PosIdPrintUtil::IdToString(cand.lid));
  lines.push_back("rid: " + PosIdPrintUtil::IdToString(cand.rid));
  lines.push_back("attr: " + CandidateAttributesToString(cand.attributes));
  lines.push_back("num_style: " + NumberStyleToString(cand.style));
  const string &segbdd_str = InnerSegmentBoundaryToString(cand);
  if (!segbdd_str.empty()) {
    lines.push_back("segbdd: " + segbdd_str);
  }

  (*os) << "  " << num << " " << cand.value << endl;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (!lines[i].empty()) {
      (*os) << "       " << lines[i] << endl;
    }
  }
}

void PrintSegment(size_t num, size_t segments_size,
                  const Segment &segment, ostream *os) {
  (*os) << "---------- Segment " << num << "/" << segments_size << " ["
        << SegmentTypeToString(segment.segment_type())
        << "] ----------" << endl
        << segment.key() << endl;
  if (FLAGS_show_meta_candidates) {
    for (int i = 0; i < segment.meta_candidates_size(); ++i) {
      PrintCandidate(segment, -i - 1, segment.meta_candidate(i), os);
    }
  }
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    PrintCandidate(segment, i, segment.candidate(i), os);
  }
}

void PrintSegments(const Segments &segments, ostream *os) {
  for (size_t i = 0; i < segments.segments_size(); ++i) {
    PrintSegment(i, segments.segments_size(), segments.segment(i), os);
  }
}

bool ExecCommand(const ConverterInterface &converter,
                 Segments *segments,
                 const string &line,
                 const commands::Request &request) {
  vector<string> fields;
  Util::SplitStringUsing(line, "\t ", &fields);

#define CHECK_FIELDS_LENGTH(length) \
  if (fields.size() < (length)) { \
     return false; \
  }

  CHECK_FIELDS_LENGTH(1);

  const string &func = fields[0];

  const Config config;

  segments->set_max_conversion_candidates_size(
      FLAGS_max_conversion_candidates_size);

  if (func == "startconversion" || func == "start" || func == "s") {
    CHECK_FIELDS_LENGTH(2);
    Table table;
    Composer composer(&table, &request);
    composer.InsertCharacterPreedit(fields[1]);
    ConversionRequest conversion_request(&composer, &request);
    return converter.StartConversionForRequest(conversion_request, segments);
  } else if (func == "convertwithnodeinfo" || func == "cn") {
    CHECK_FIELDS_LENGTH(5);
    Lattice::SetDebugDisplayNode(atoi32(fields[2].c_str()),  // begin pos
                                 atoi32(fields[3].c_str()),  // end pos
                                 fields[4]);
    const bool result = converter.StartConversion(segments, fields[1]);
    Lattice::ResetDebugDisplayNode();
    return result;
  } else if (func == "reverseconversion" || func == "reverse" || func == "r") {
    CHECK_FIELDS_LENGTH(2);
    return converter.StartReverseConversion(segments, fields[1]);
  } else if (func == "startprediction" || func == "predict" || func == "p") {
    Table table;
    Composer composer(&table, &request);
    if (fields.size() >= 2) {
      composer.InsertCharacterPreedit(fields[1]);
      ConversionRequest conversion_request(&composer, &request);
      return converter.StartPredictionForRequest(conversion_request, segments);
    } else {
      ConversionRequest conversion_request(&composer, &request);
      return converter.StartPredictionForRequest(conversion_request, segments);
    }
  } else if (func == "startsuggestion" || func == "suggest") {
    Table table;
    Composer composer(&table, &request);
    if (fields.size() >= 2) {
      composer.InsertCharacterPreedit(fields[1]);
      ConversionRequest conversion_request(&composer, &request);
      return converter.StartSuggestionForRequest(conversion_request, segments);
    } else {
      ConversionRequest conversion_request(&composer, &request);
      return converter.StartSuggestionForRequest(conversion_request, segments);
    }
  } else if (func == "finishconversion" || func == "finish") {
    Table table;
    Composer composer(&table, &request);
    ConversionRequest conversion_request(&composer, &request);
    return converter.FinishConversion(conversion_request, segments);
  } else if (func == "resetconversion" || func == "reset") {
    return converter.ResetConversion(segments);
  } else if (func == "cancelconversion" || func == "cancel") {
    return converter.CancelConversion(segments);
  } else if (func == "commitsegmentvalue" || func == "commit" || func == "c") {
    CHECK_FIELDS_LENGTH(3);
    return converter.CommitSegmentValue(segments,
                                        atoi32(fields[1].c_str()),
                                        atoi32(fields[2].c_str()));
  } else if (func == "commitallandfinish") {
    for (int i = 0; i < segments->conversion_segments_size(); ++i) {
      if (segments->conversion_segment(i).segment_type() !=
            Segment::FIXED_VALUE) {
        if (!(converter.CommitSegmentValue(segments, i, 0))) return false;
      }
    }
    Table table;
    Composer composer(&table, &request);
    ConversionRequest conversion_request(&composer, &request);
    return converter.FinishConversion(conversion_request, segments);
  } else if (func == "focussegmentvalue" || func == "focus") {
    CHECK_FIELDS_LENGTH(3);
    return converter.FocusSegmentValue(segments,
                                       atoi32(fields[1].c_str()),
                                       atoi32(fields[2].c_str()));
  } else if (func == "commitfirstsegment") {
    CHECK_FIELDS_LENGTH(2);
    return converter.CommitFirstSegment(segments,
                                        atoi32(fields[1].c_str()));

  } else if (func == "freesegmentvalue" || func == "free") {
    CHECK_FIELDS_LENGTH(2);
    return converter.FreeSegmentValue(segments,
                                      atoi32(fields[1].c_str()));
  } else if (func == "resizesegment" || func == "resize") {
    const ConversionRequest request;
    if (fields.size() == 3) {
      return converter.ResizeSegment(segments,
                                     request,
                                     atoi32(fields[1].c_str()),
                                     atoi32(fields[2].c_str()));
    } else if (fields.size() > 3) {
      vector<uint8> new_arrays;
      for (size_t i = 3; i < fields.size(); ++i) {
        new_arrays.push_back(static_cast<uint8>(atoi32(fields[i].c_str())));
      }
      return converter.ResizeSegment(segments,
                                     request,
                                     atoi32(fields[1].c_str()),  // start
                                     atoi32(fields[2].c_str()),
                                     &new_arrays[0],
                                     new_arrays.size());
    }
  } else if (func == "disableuserhistory") {
    segments->set_user_history_enabled(false);
  } else if (func == "enableuserhistory") {
    segments->set_user_history_enabled(true);
  } else {
    LOG(WARNING) << "Unknown command: " <<  func;
    return false;
  }

#undef CHECK_FIELDS_LENGTH
  return true;
}

}  // namespace
}  // namespace mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  if (!FLAGS_user_profile_dir.empty()) {
    mozc::SystemUtil::SetUserProfileDirectory(FLAGS_user_profile_dir);
  }

  scoped_ptr<mozc::EngineInterface> engine;
  mozc::commands::Request request;
  if (FLAGS_engine == "default") {
    LOG(INFO) << "Using default preference and engine";
    engine.reset(mozc::EngineFactory::Create());
  }
  if (FLAGS_engine == "test") {
    LOG(INFO) << "Using test preference and engine";
    engine.reset(mozc::MockDataEngineFactory::Create());
  }

  CHECK(engine.get()) << "Invalid engine: " << FLAGS_engine;
  mozc::ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);

  mozc::Segments segments;
  string line;

  while (!getline(cin, line).fail()) {
    if (mozc::ExecCommand(*converter, &segments, line, request)) {
      if (FLAGS_output_debug_string) {
        mozc::PrintSegments(segments, &cout);
      }
    } else {
      cout << "ExecCommand() return false" << endl;
    }
  }
  return 0;
}

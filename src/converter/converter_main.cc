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

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "composer/composer.h"
#include "converter/conversion_request.h"
#include "converter/converter.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter.h"
#include "converter/lattice.h"
#include "converter/segments.h"
#include "session/commands.pb.h"
#include "session/request_handler.h"

DEFINE_int32(max_conversion_candidates_size, 200,
             "maximum candidates size");
DEFINE_string(user_profile_dir, "", "path to user profile directory");

DEFINE_bool(mobile, false, "use mobile preference");
DEFINE_bool(output_debug_string, true, "output debug string for each input");

namespace {
bool ExecCommand(const mozc::ConverterInterface &converter,
                 mozc::Segments *segments,
                 const string &line) {
  vector<string> fields;
  mozc::Util::SplitStringUsing(line, "\t ", &fields);

#define CHECK_FIELDS_LENGTH(length) \
  if (fields.size() < (length)) { \
     return false; \
  }

  CHECK_FIELDS_LENGTH(1);

  const string &func = fields[0];

  segments->set_max_conversion_candidates_size(
      FLAGS_max_conversion_candidates_size);

  if (func == "startconversion" || func == "start" || func == "s") {
    CHECK_FIELDS_LENGTH(2);
    return converter.StartConversion(segments, fields[1]);
  } else if (func == "convertwithnodeinfo" || func == "cn") {
    CHECK_FIELDS_LENGTH(5);
    mozc::Lattice::SetDebugDisplayNode(atoi32(fields[2].c_str()),  // begin pos
                                       atoi32(fields[3].c_str()),  // end pos
                                       fields[4]);
    const bool result = converter.StartConversion(segments, fields[1]);
    mozc::Lattice::ResetDebugDisplayNode();
    return result;
  } else if (func == "reverseconversion" || func == "reverse" || func == "r") {
    CHECK_FIELDS_LENGTH(2);
    return converter.StartReverseConversion(segments, fields[1]);
  } else if (func == "startprediction" || func == "predict" || func == "p") {
    if (fields.size() >= 2) {
      return converter.StartPrediction(segments, fields[1]);
    } else {
      return converter.StartPrediction(segments, "");
    }
  } else if (func == "startsuggestion" || func == "suggest") {
    if (fields.size() >= 2) {
      return converter.StartSuggestion(segments, fields[1]);
    } else {
      return converter.StartSuggestion(segments, "");
    }
  } else if (func == "finishconversion" || func == "finish") {
    return converter.FinishConversion(segments);
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
            mozc::Segment::FIXED_VALUE) {
        if (!(converter.CommitSegmentValue(segments, i, 0))) return false;
      }
    }
    return converter.FinishConversion(segments);
  } else if (func == "focussegmentvalue" || func == "focus") {
    CHECK_FIELDS_LENGTH(3);
    return converter.FocusSegmentValue(segments,
                                       atoi32(fields[1].c_str()),
                                       atoi32(fields[2].c_str()));
  } else if (func == "submitfirstsegment") {
    CHECK_FIELDS_LENGTH(2);
    return converter.SubmitFirstSegment(segments,
                                        atoi32(fields[1].c_str()));

  } else if (func == "freesegmentvalue" || func == "free") {
    CHECK_FIELDS_LENGTH(2);
    return converter.FreeSegmentValue(segments,
                                      atoi32(fields[1].c_str()));
  } else if (func == "resizesegment" || func == "resize") {
    const mozc::ConversionRequest request;
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
  } else if (func == "convertwithprecedingtext" || func == "cwpt") {
    CHECK_FIELDS_LENGTH(3);
    mozc::composer::Composer composer;
    composer.InsertCharacter(fields[2]);
    mozc::ConversionRequest request(&composer);
    request.set_preceding_text(fields[1]);
    converter.StartConversionForRequest(request, segments);
  } else if (func == "predictwithprecedingtext" || func == "pwpt") {
    CHECK_FIELDS_LENGTH(3);
    mozc::composer::Composer composer;
    composer.InsertCharacter(fields[2]);
    mozc::ConversionRequest request(&composer);
    request.set_preceding_text(fields[1]);
    converter.StartPredictionForRequest(request, segments);
  } else {
    LOG(WARNING) << "Unknown command: " <<  func;
    return false;
  }

#undef CHECK_FIELDS_LENGTH
  return true;
}
}   // namespace

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  if (!FLAGS_user_profile_dir.empty()) {
    mozc::Util::SetUserProfileDirectory(FLAGS_user_profile_dir);
  }

  if (FLAGS_mobile) {
    LOG(INFO) << "Using mobile preference and dictionary";
    mozc::commands::Request request;
    request.set_zero_query_suggestion(true);
    request.set_mixed_conversion(true);
    mozc::commands::RequestHandler::SetRequest(request);

  }

  scoped_ptr<mozc::ImmutableConverterImpl> immutable_converter(
      new mozc::ImmutableConverterImpl);
  mozc::ImmutableConverterFactory::SetImmutableConverter(
      immutable_converter.get());
  scoped_ptr<mozc::ConverterImpl> converter_impl(new mozc::ConverterImpl);
  mozc::ConverterInterface *converter = converter_impl.get();
  CHECK(converter);

  mozc::Segments segments;
  string line;

  while (getline(cin, line)) {
    if (ExecCommand(*converter, &segments, line)) {
      if (FLAGS_output_debug_string) {
        cout << segments.DebugString();
      }
    } else {
      cout << "ExecCommand() return false" << endl;
    }
  }
  return 0;
}

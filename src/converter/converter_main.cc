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

#include <iostream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "converter/conversion_request.h"
#include "converter/converter_interface.h"
#include "converter/lattice.h"
#include "converter/segments.h"
#include "engine/chromeos_engine_factory.h"
#include "engine/engine_factory.h"
#include "engine/engine_interface.h"
#include "engine/mock_data_engine_factory.h"
#include "session/commands.pb.h"

DEFINE_int32(max_conversion_candidates_size, 200,
             "maximum candidates size");
DEFINE_string(user_profile_dir, "", "path to user profile directory");

DEFINE_string(engine, "default", "engine: (default, chromeos)");
DEFINE_bool(output_debug_string, true, "output debug string for each input");

namespace {
bool ExecCommand(const mozc::ConverterInterface &converter,
                 mozc::Segments *segments,
                 const string &line,
                 const mozc::commands::Request &request) {
  vector<string> fields;
  mozc::Util::SplitStringUsing(line, "\t ", &fields);

#define CHECK_FIELDS_LENGTH(length) \
  if (fields.size() < (length)) { \
     return false; \
  }

  CHECK_FIELDS_LENGTH(1);

  const string &func = fields[0];

  const mozc::config::Config config;

  segments->set_max_conversion_candidates_size(
      FLAGS_max_conversion_candidates_size);

  if (func == "startconversion" || func == "start" || func == "s") {
    CHECK_FIELDS_LENGTH(2);
    mozc::composer::Table table;
    mozc::composer::Composer composer(&table, &request);
    composer.InsertCharacterPreedit(fields[1]);
    mozc::ConversionRequest conversion_request(&composer, &request);
    return converter.StartConversionForRequest(conversion_request, segments);
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
    mozc::composer::Table table;
    mozc::composer::Composer composer(&table, &request);
    if (fields.size() >= 2) {
      composer.InsertCharacterPreedit(fields[1]);
      mozc::ConversionRequest conversion_request(&composer, &request);
      return converter.StartPredictionForRequest(conversion_request, segments);
    } else {
      mozc::ConversionRequest conversion_request(&composer, &request);
      return converter.StartPredictionForRequest(conversion_request, segments);
    }
  } else if (func == "startsuggestion" || func == "suggest") {
    mozc::composer::Table table;
    mozc::composer::Composer composer(&table, &request);
    if (fields.size() >= 2) {
      composer.InsertCharacterPreedit(fields[1]);
      mozc::ConversionRequest conversion_request(&composer, &request);
      return converter.StartSuggestionForRequest(conversion_request, segments);
    } else {
      mozc::ConversionRequest conversion_request(&composer, &request);
      return converter.StartSuggestionForRequest(conversion_request, segments);
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
  } else if (func == "commitfirstsegment") {
    CHECK_FIELDS_LENGTH(2);
    return converter.CommitFirstSegment(segments,
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
    mozc::composer::Table table;
    table.InitializeWithRequestAndConfig(request, config);
    mozc::composer::Composer composer(&table, &request);
    composer.InsertCharacterPreedit(fields[2]);
    mozc::ConversionRequest conversion_request(&composer, &request);
    conversion_request.set_preceding_text(fields[1]);
    converter.StartConversionForRequest(conversion_request, segments);
  } else if (func == "predictwithprecedingtext" || func == "pwpt") {
    CHECK_FIELDS_LENGTH(3);
    mozc::composer::Table table;
    table.InitializeWithRequestAndConfig(request, config);
    mozc::composer::Composer composer(&table, &request);
    composer.InsertCharacterPreedit(fields[2]);
    mozc::ConversionRequest conversion_request(&composer, &request);
    conversion_request.set_preceding_text(fields[1]);
    converter.StartPredictionForRequest(conversion_request, segments);
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
    mozc::SystemUtil::SetUserProfileDirectory(FLAGS_user_profile_dir);
  }

  scoped_ptr<mozc::EngineInterface> engine;
  mozc::commands::Request request;
  if (FLAGS_engine == "default") {
    LOG(INFO) << "Using default preference and engine";
    engine.reset(mozc::EngineFactory::Create());
  }
  if (FLAGS_engine == "chromeos") {
    LOG(INFO) << "Using chromeos preference and engine";
    engine.reset(mozc::ChromeOsEngineFactory::Create());
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
    if (ExecCommand(*converter, &segments, line, request)) {
      if (FLAGS_output_debug_string) {
        cout << segments.DebugString();
      }
    } else {
      cout << "ExecCommand() return false" << endl;
    }
  }
  return 0;
}

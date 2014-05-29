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

#include "prediction/suggestion_filter.h"

#include <set>
#include <vector>
#include <string>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/flags.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

// TODO(noriyukit): Write test using tiner data set.
DEFINE_string(dictionary_file,
              "data/dictionary_oss/dictionary00.txt,"
              "data/dictionary_oss/dictionary01.txt,"
              "data/dictionary_oss/dictionary02.txt,"
              "data/dictionary_oss/dictionary03.txt,"
              "data/dictionary_oss/dictionary04.txt,"
              "data/dictionary_oss/dictionary05.txt,"
              "data/dictionary_oss/dictionary06.txt,"
              "data/dictionary_oss/dictionary07.txt,"
              "data/dictionary_oss/dictionary08.txt,"
              "data/dictionary_oss/dictionary09.txt",
              "default mozc dictionary");
DEFINE_string(suggestion_filter_files,
              "data/dictionary_oss/suggestion_filter.txt",
              "comma separated mozc suggestion_filter filter files");
DECLARE_string(test_srcdir);

namespace mozc {

namespace {
const int kThreshold = 30;
const double kErrorRatio = 0.0001;
}  // namespace

TEST(SuggestionFilter, IsBadSuggestionTest) {
  // Load suggestion_filter
  hash_set<string> suggestion_filter_set;

  vector<string> files;
  Util::SplitStringUsing(FLAGS_suggestion_filter_files, ",", &files);
  for (size_t i = 0; i < files.size(); ++i) {
    string line;
    const string filter_file = FileUtil::JoinPath(FLAGS_test_srcdir, files[i]);
    InputFileStream input(filter_file.c_str());
    CHECK(input) << "cannot open: " << filter_file;
    while (getline(input, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      Util::LowerString(&line);
      suggestion_filter_set.insert(line);
    }
  }

  LOG(INFO) << "Filter word size:\t" << suggestion_filter_set.size();

  vector<string> dic_files;
  Util::SplitStringUsing(FLAGS_dictionary_file, ",", &dic_files);
  size_t false_positives = 0;
  size_t num_words = 0;
  for (size_t i = 0; i < dic_files.size(); ++i) {
    LOG(INFO) << dic_files[i];
    const string dic_file = FileUtil::JoinPath(FLAGS_test_srcdir,
                                               dic_files[i]);
    InputFileStream input(dic_file.c_str());
    CHECK(input) << "cannot open: " << dic_file;
    vector<string> fields;
    string line;
    while (getline(input, line)) {
      fields.clear();
      Util::SplitStringUsing(line, "\t", &fields);
      CHECK_GE(fields.size(), 5);
      string value = fields[4];
      Util::LowerString(&value);

      const bool true_result =
          (suggestion_filter_set.find(value) != suggestion_filter_set.end());
      const bool bloom_filter_result
          = SuggestionFilter::IsBadSuggestion(value);

      // never emits false negative
      if (true_result) {
        EXPECT_TRUE(bloom_filter_result) << value;
      } else {
        if (bloom_filter_result) {
          ++false_positives;
          LOG(INFO) << value << " is false positive";
        }
      }
      ++num_words;
    }
  }

  const float error_ratio = 1.0 * false_positives / num_words;

  LOG(INFO) << "False positive ratio is " << error_ratio;

  EXPECT_LT(error_ratio, kErrorRatio);
}
}  // namespace mozc

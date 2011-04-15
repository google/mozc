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
#include <iostream>
#include "base/base.h"
#include "base/util.h"
#include "base/file_stream.h"
#include "converter/quality_regression_util.h"

DEFINE_string(test_file, "", "regression test file");

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  mozc::quality_regression::QualityRegressionUtil util;
  mozc::InputFileStream is(FLAGS_test_file.c_str());

  CHECK(is) << "Cannot open: " << FLAGS_test_file;

  string line;
  while (getline(is, line)) {
    vector<string> tokens;
    mozc::Util::SplitStringUsing(line, "\t", &tokens);
    CHECK_GE(tokens.size(), 6);
    const string &key            = tokens[1];
    const string &expected_value = tokens[2];
    const string &command        = tokens[3];
    const uint32  expected_rank  = atoi(tokens[4].c_str());

    string actual_value;
    const  bool result = util.ConvertAndTest(key,
                                             expected_value,
                                             command,
                                             expected_rank,
                                             &actual_value);
    if (result) {
      cout << "OK:\t" << line << endl;
    } else {
      cout << "FAILED:\t" << line << "\t" << actual_value << endl;
    }
  }

  return 0;
}

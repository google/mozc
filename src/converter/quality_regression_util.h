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
#include "base/base.h"

namespace mozc {
class Segments;
class ConverterInterface;

namespace quality_regression {

class QualityRegressionUtil {
 public:
  // bit fields
  enum Platform {
    DESKTOP = 1,
    OSS = 2,
  };

  struct TestItem {
    string label;
    string key;
    string expected_value;
    string command;
    int    expected_rank;
    double accuracy;
    // Target platform. Can set multiple platform defined in enum |Platform|.
    uint32 platform;
    string OutputAsTSV() const;
    bool ParseFromTSV(const string &tsv_line);
  };

  QualityRegressionUtil();
  explicit QualityRegressionUtil(ConverterInterface *converter);
  virtual ~QualityRegressionUtil();

  // Pase |filename| and save the all test items into |outputs|.
  static bool ParseFile(const string &filename,
                        vector<TestItem> *outputs);

  bool ConvertAndTest(const TestItem &item,
                      string *actual_value);

 private:
  ConverterInterface *converter_;
  scoped_ptr<Segments> segments_;
};
}  // quality_regression
}  // mozc

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

#include <iostream>  // NOLINT
#include <memory>
#include <string>
#include <vector>

#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/util.h"
#include "converter/quality_regression_util.h"
#include "engine/engine_factory.h"
#include "engine/engine_interface.h"

MOZC_FLAG(string, test_file, "", "regression test file");

using mozc::EngineFactory;
using mozc::EngineInterface;
using mozc::quality_regression::QualityRegressionUtil;

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  std::unique_ptr<EngineInterface> engine(EngineFactory::Create());
  QualityRegressionUtil util(engine->GetConverter());

  std::vector<QualityRegressionUtil::TestItem> items;
  QualityRegressionUtil::ParseFile(mozc::GetFlag(FLAGS_test_file), &items);

  for (size_t i = 0; i < items.size(); ++i) {
    std::string actual_value;
    const bool result = util.ConvertAndTest(items[i], &actual_value);
    if (result) {
      std::cout << "OK:\t" << items[i].OutputAsTSV() << std::endl;
    } else {
      std::cout << "FAILED:\t" << items[i].OutputAsTSV() << "\t" << actual_value
                << std::endl;
    }
  }

  return 0;
}

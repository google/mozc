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
#include <cstdint>
#include <iostream>
#include <string>

#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/util.h"
#include "ios/ios_engine.h"
#include "protocol/candidates.pb.h"
#include "absl/flags/flag.h"

// mozc/data_manager/testing:mozc_dataset_for_testing is one of datafile.
ABSL_FLAG(std::string, datafile, "", "Path to a data file to be used");
ABSL_FLAG(int32_t, candsize, 3, "Maximum number of candidates");
ABSL_FLAG(bool, show_full, false, "Display the debug string of output command");

namespace {

void Convert(const std::string &query, mozc::ios::IosEngine *engine,
             mozc::commands::Command *command) {
  std::string character;
  const char *begin = query.data();
  const char *end = query.data() + query.size();
  while (begin < end) {
    const size_t mblen = mozc::Util::OneCharLen(begin);
    character.assign(begin, mblen);
    if (character == ">") {
      engine->SendSpecialKey(mozc::commands::KeyEvent::VIRTUAL_RIGHT, command);
    } else if (character == "<") {
      engine->SendSpecialKey(mozc::commands::KeyEvent::VIRTUAL_LEFT, command);
    } else {
      engine->SendKey(character, command);
    }
    begin += mblen;
  }
}

}  // namespace

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  mozc::ios::IosEngine ios_engine(absl::GetFlag(FLAGS_datafile));

  mozc::commands::Command command;
  mozc::config::Config config;
  mozc::ios::IosEngine::FillMobileConfig(&config);
  ios_engine.SetConfig(config, &command);
  ios_engine.SetMobileRequest("12KEYS", &command);

  while (ios_engine.CreateSession(&command)) {
    std::string query;
    std::cout << "query: ";
    std::getline(std::cin, query);
    if (query.empty()) {
      break;
    }
    if (query == "\t12KEYS") {
      ios_engine.SetMobileRequest("12KEYS", &command);
      std::cout << "Selected 12 key table" << std::endl;
      continue;
    }
    if (query == "\tQWERTY_JA") {
      ios_engine.SetMobileRequest("QWERTY_JA", &command);
      std::cout << "Selected qwerty Hiragana table" << std::endl;
      continue;
    }

    Convert(query, &ios_engine, &command);

    if (absl::GetFlag(FLAGS_show_full)) {
      std::cout << command.Utf8DebugString() << std::endl;
    } else {
      std::cout << "----- preedit -----\n"
                << command.output().preedit().Utf8DebugString() << std::endl;
      const auto &cands = command.output().candidates();
      const int size = std::min(absl::GetFlag(FLAGS_candsize),
                                cands.candidate_size());
      for (int i = 0; i < size; ++i) {
        std::cout << "----- candidate " << i << " -----\n"
                  << cands.candidate(i).Utf8DebugString() << std::endl;
      }
    }
  }
  return 0;
}

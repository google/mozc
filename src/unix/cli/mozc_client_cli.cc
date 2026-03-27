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

#include <iostream>
#include <sstream>
#include <string>

#include "absl/flags/flag.h"
#include "base/init_mozc.h"
#include "client/client.h"
#include "google/protobuf/json/json.h"
#include "protocol/commands.pb.h"

ABSL_FLAG(std::string, format, "jsonl", "Format for input/output");

namespace mozc::cli {
namespace {
// Main loop, which takes JSON Lines as "mozc.commands.Input"s and
// print corresponding results returned by Mozc server in JSON lines.
bool ProcessLoopJSONL() {
  client::Client client;
  std::string line;
  while (std::getline(std::cin, line)) {
    mozc::commands::Input input;
    auto status = google::protobuf::json::JsonStringToMessage(line, &input);
    if (!status.ok()) {
      std::cerr << "Failed to parse an input JSON as mozc.commands.Input: <"
                << line << ">: " << status.ToString() << std::endl;
      return false;
    }
    mozc::commands::Output output;
    if (!client.Call(input, &output)) {
      return false;
    }
    std::string outputJSON;
    status = google::protobuf::json::MessageToJsonString(output, &outputJSON);
    if (!status.ok()) {
      std::cerr << "Failed to generate an output JSON for mozc.commands.Output: "
                << status.ToString() << std::endl;
      return false;
    }
    std::cout << outputJSON << std::endl;
  }
  return true;
}

}  // namespace
}  // namespace mozc::cli

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  bool success = false;
  const auto& format = absl::GetFlag(FLAGS_format);
  if (format == "jsonl") {
    success = mozc::cli::ProcessLoopJSONL();
  } else if (format == "text") {
    // TODO
  } else if (format == "binary") {
    // TODO
  } else if (format == "s-expressionl") {
    // TODO
  } else {
    std::cerr << "Unsupported format: <" << format << ">" << std::endl;
  }

  return success ? 0 : 1;
}

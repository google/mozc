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
#include <istream>
#include <memory>
#include <ostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/init_mozc.h"
#include "base/protobuf/text_format.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/key_parser.h"
#include "engine/engine_factory.h"
#include "engine/engine_interface.h"
#include "protocol/commands.pb.h"
#include "session/session.h"

ABSL_FLAG(std::string, input, "", "Input file");
ABSL_FLAG(std::string, output, "", "Output file");
ABSL_FLAG(std::string, profile_dir, "", "Profile dir");

namespace mozc {
namespace {

void Loop(std::istream *input, std::ostream *output) {
  std::unique_ptr<EngineInterface> engine = EngineFactory::Create().value();
  auto session = std::make_unique<session::Session>(*engine);

  commands::Command command;
  std::string line;
  while (std::getline(*input, line)) {
    Util::ChopReturns(&line);
    if (line.size() > 1 && line[0] == '#' && line[1] == '#') {
      continue;
    }
    if (line.empty()) {
      session = std::make_unique<session::Session>(*engine);
      *output << std::endl << "## New session" << std::endl << std::endl;
      continue;
    }

    command.Clear();
    command.mutable_input()->set_type(commands::Input::SEND_KEY);
    if (!KeyParser::ParseKey(line, command.mutable_input()->mutable_key())) {
      LOG(ERROR) << "cannot parse: " << line;
      continue;
    }

    if (!session->SendKey(&command)) {
      LOG(ERROR) << "Command failure";
    }

    std::string textpb;
    protobuf::TextFormat::PrintToString(command, &textpb);
    *output << textpb;
    LOG(INFO) << command;
  }
}

}  // namespace
}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  std::unique_ptr<mozc::InputFileStream> input_file;
  std::unique_ptr<mozc::OutputFileStream> output_file;
  std::istream *input = nullptr;
  std::ostream *output = nullptr;

  const std::string flags_profile_dir = absl::GetFlag(FLAGS_profile_dir);
  const std::string flags_input = absl::GetFlag(FLAGS_input);
  const std::string flags_output = absl::GetFlag(FLAGS_output);

  if (!flags_profile_dir.empty()) {
    // TODO(komatsu): Make a tmp dir and use it.
    if (absl::Status s = mozc::FileUtil::CreateDirectory(flags_profile_dir);
        !s.ok() && absl::IsAlreadyExists(s)) {
      LOG(ERROR) << s;
      return -1;
    }
    mozc::SystemUtil::SetUserProfileDirectory(flags_profile_dir);
  }

  if (!flags_input.empty()) {
    // Batch mode loading the input file.
    input_file = std::make_unique<mozc::InputFileStream>(flags_input);
    if (input_file->fail()) {
      LOG(ERROR) << "File not opend: " << flags_input;
      std::cerr << "File not opend: " << flags_input << std::endl;
      return 1;
    }
    input = input_file.get();
  } else {
    // Interaction mode.
    input = &std::cin;
  }

  if (!flags_output.empty()) {
    output_file = std::make_unique<mozc::OutputFileStream>(flags_output);
    if (output_file->fail()) {
      LOG(ERROR) << "File not opend: " << flags_output;
      std::cerr << "File not opend: " << flags_output << std::endl;
      return 1;
    }
    output = output_file.get();
  } else {
    output = &std::cout;
  }

  mozc::Loop(input, output);
  return 0;
}

// Copyright 2010-2020, Google Inc.
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
#include <memory>
#include <string>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/key_parser.h"
#include "engine/engine_factory.h"
#include "engine/engine_interface.h"
#include "protocol/commands.pb.h"
#include "session/session.h"

DEFINE_string(input, "", "Input file");
DEFINE_string(output, "", "Output file");
DEFINE_string(profile_dir, "", "Profile dir");

namespace mozc {

void Loop(std::istream *input, std::ostream *output) {
  std::unique_ptr<EngineInterface> engine(EngineFactory::Create());
  std::unique_ptr<session::Session> session(new session::Session(engine.get()));

  commands::Command command;
  std::string line;
  while (std::getline(*input, line)) {
    Util::ChopReturns(&line);
    if (line.size() > 1 && line[0] == '#' && line[1] == '#') {
      continue;
    }
    if (line.empty()) {
      session.reset(new session::Session(engine.get()));
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

    *output << command.Utf8DebugString();
    LOG(INFO) << command.Utf8DebugString();
  }
}

}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  std::unique_ptr<mozc::InputFileStream> input_file;
  std::unique_ptr<mozc::OutputFileStream> output_file;
  std::istream *input = nullptr;
  std::ostream *output = nullptr;

  if (!FLAGS_profile_dir.empty()) {
    // TODO(komatsu): Make a tmp dir and use it.
    mozc::FileUtil::CreateDirectory(FLAGS_profile_dir);
    mozc::SystemUtil::SetUserProfileDirectory(FLAGS_profile_dir);
  }

  if (!FLAGS_input.empty()) {
    // Batch mode loading the input file.
    input_file.reset(new mozc::InputFileStream(FLAGS_input.c_str()));
    if (input_file->fail()) {
      LOG(ERROR) << "File not opend: " << FLAGS_input;
      std::cerr << "File not opend: " << FLAGS_input << std::endl;
      return 1;
    }
    input = input_file.get();
  } else {
    // Interaction mode.
    input = &std::cin;
  }

  if (!FLAGS_output.empty()) {
    output_file.reset(new mozc::OutputFileStream(FLAGS_output.c_str()));
    if (output_file->fail()) {
      LOG(ERROR) << "File not opend: " << FLAGS_output;
      std::cerr << "File not opend: " << FLAGS_output << std::endl;
      return 1;
    }
    output = output_file.get();
  } else {
    output = &std::cout;
  }

  mozc::Loop(input, output);
  return 0;
}

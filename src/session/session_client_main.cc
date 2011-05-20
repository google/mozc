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

#include <iostream>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "session/commands.pb.h"
#include "session/key_parser.h"
#include "session/session.h"

DEFINE_string(input, "", "Input file");
DEFINE_string(output, "", "Output file");
DEFINE_string(profile_dir, "", "Profile dir");

namespace mozc {
void Loop(istream *input, ostream *output) {
  scoped_ptr<session::Session> session(new session::Session);

  commands::Command command;
  string line;
  while (getline(*input, line)) {
    Util::ChopReturns(&line);
    if (line.size() > 1 && line[0] == '#' && line[1] == '#') {
      continue;
    }
    if (line.empty()) {
      session.reset(new session::Session);
      *output << endl << "## New session" << endl << endl;
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

    *output << command.DebugString();
    LOG(INFO) << command.DebugString();
  }
}

}  // namespace mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);
  scoped_ptr<mozc::InputFileStream> input_file(NULL);
  scoped_ptr<mozc::OutputFileStream> output_file(NULL);
  istream *input = NULL;
  ostream *output = NULL;

  if (!FLAGS_profile_dir.empty()) {
    // TODO(komatsu): Make a tmp dir and use it.
    mozc::Util::CreateDirectory(FLAGS_profile_dir);
    mozc::Util::SetUserProfileDirectory(FLAGS_profile_dir);
  }

  if (!FLAGS_input.empty()) {
    // Batch mode loading the input file.
    input_file.reset(new mozc::InputFileStream(FLAGS_input.c_str()));
    if (input_file->fail()) {
      LOG(ERROR) << "File not opend: " << FLAGS_input;
      cerr << "File not opend: " << FLAGS_input << endl;
      return 1;
    }
    input = input_file.get();
  } else {
    // Interaction mode.
    input = &cin;
  }

  if (!FLAGS_output.empty()) {
    output_file.reset(new mozc::OutputFileStream(FLAGS_output.c_str()));
    if (output_file->fail()) {
      LOG(ERROR) << "File not opend: " << FLAGS_output;
      cerr << "File not opend: " << FLAGS_output << endl;
      return 1;
    }
    output = output_file.get();
  } else {
    output = &cout;
  }

  mozc::Loop(input, output);
  return 0;
}

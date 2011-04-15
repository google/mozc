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

#ifndef MOZC_UNIX_SCIM_MOZC_RESPONSE_PARSER_H_
#define MOZC_UNIX_SCIM_MOZC_RESPONSE_PARSER_H_

#include "base/base.h"  // for DISALLOW_COPY_AND_ASSIGN.

namespace mozc {
namespace commands {

class Candidates;
class Input;
class Output;
class Preedit;
class Result;

}  // namespace commands
}  // namespace mozc

namespace mozc_unix_scim {

class ScimMozc;

// This class parses IPC response from mozc_server (mozc::commands::Output) and
// updates the SCIM UI.
class MozcResponseParser {
 public:
  MozcResponseParser();
  ~MozcResponseParser();

  // Parses a response from Mozc server and sets persed information on scim_mozc
  // object. Returns true if response.consumed() is true. scim_mozc must be non
  // NULL. This function does not take ownership of scim_mozc.
  bool ParseResponse(const mozc::commands::Output &response,
                     ScimMozc *scim_mozc) const;

  // Setter for use_annotation_. If use_annotation_ is true, ParseCandidates()
  // uses annotation infomation.
  void set_use_annotation(bool use_annotation);

 private:
  void ParseResult(const mozc::commands::Result &result,
                   ScimMozc *scim_mozc) const;
  void ParseCandidates(const mozc::commands::Candidates &candidates,
                       ScimMozc *scim_mozc) const;
  void ParsePreedit(const mozc::commands::Preedit &preedit,
                    uint32 position,
                    ScimMozc *scim_mozc) const;

  bool use_annotation_;

  DISALLOW_COPY_AND_ASSIGN(MozcResponseParser);
};

}  // namespace mozc_unix_scim

#endif  // MOZC_UNIX_SCIM_MOZC_RESPONSE_PARSER_H_

// Copyright 2010-2013, Google Inc.
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

// Handwriting module connecting to the cloud server.

#include "handwriting/cloud_handwriting.h"

#include "base/logging.h"



namespace mozc {
namespace handwriting {
namespace {

bool MakeHandwritingRequest(const string &request, string *output) {
  bool result = true;
  // The details of sending is internal only just in case.
  return result;
}

bool SendHandwritingFeedback(const string &request) {
  bool result = true;
  // The details of sending is internal only just in case.
  return result;
}

}  // namespace

// static
bool CloudHandwriting::ParseResponse(const string &json,
                                     vector<string> *candidates) {
  // The response format is internal only just in case.

  return true;
}

// static
string CloudHandwriting::CreateRequest(const Strokes &strokes) {
  string result;
  // The request format is internal only just in case.
  return result;
}

// static
string CloudHandwriting::CreateFeedback(
    const Strokes &strokes, const string &result) {
  if (strokes.empty() || result.empty()) {
    return "";
  }

  string feedback_data;
  // The feedback format is internal only just in case.
  return feedback_data;
}

HandwritingStatus CloudHandwriting::Recognize(
    const Strokes &strokes, vector<string> *candidates) const {
  string output;
  if (!MakeHandwritingRequest(CreateRequest(strokes), &output)) {
    return HANDWRITING_NETWORK_ERROR;
  }
  if (!ParseResponse(output, candidates)) {
    return HANDWRITING_ERROR;
  }
  return HANDWRITING_NO_ERROR;
}

HandwritingStatus CloudHandwriting::Commit(const Strokes &strokes,
                                           const string &result) {
  if (strokes.empty()) {
    DLOG(ERROR) << "Empty strokes: nothing should be committed";
    return HANDWRITING_ERROR;
  }

  if (result.empty()) {
    DLOG(ERROR) << "Result is empty";
    return HANDWRITING_ERROR;
  }
  if (!SendHandwritingFeedback(CreateFeedback(strokes, result))) {
    return HANDWRITING_NETWORK_ERROR;
  }
  return HANDWRITING_NO_ERROR;
}

}  // namespace handwriting
}  // namespace mozc

// Copyright 2010-2012, Google Inc.
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

// Test utility for Request.

#include "session/request_handler.h"
#include "session/request_test_util.h"

namespace mozc {
namespace commands {
ScopedRequestForUnittest::ScopedRequestForUnittest(const Request &request)
    : prev_request_(RequestHandler::GetRequest()) {
  RequestHandler::SetRequest(request);
}

ScopedRequestForUnittest::~ScopedRequestForUnittest() {
  RequestHandler::SetRequest(prev_request_);
}

ScopedMobileRequestForUnittest::ScopedMobileRequestForUnittest() {
  Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_update_input_mode_from_surrounding_text(false);
  request.set_special_romanji_table(Request::TWELVE_KEYS_TO_HIRAGANA);

  scoped_request_.reset(new ScopedRequestForUnittest(request));
}

ScopedMobileRequestForUnittest::~ScopedMobileRequestForUnittest() {}
}  // namespace commands
}  // namespace mozc

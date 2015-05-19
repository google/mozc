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

// Metadata of OAuth2 server (used by OAuth2Util)

#ifndef MOZC_SYNC_OAUTH2_SERVER_H_
#define MOZC_SYNC_OAUTH2_SERVER_H_

#include <string>

namespace mozc {
namespace sync {

struct OAuth2Server {
  string authenticate_uri_;
  string redirect_uri_;
  string request_token_uri_;
  string scope_;

  OAuth2Server(const string &authenticate_uri,
               const string &redirect_uri,
               const string &request_token_uri,
               const string &scope)
      : authenticate_uri_(authenticate_uri),
        redirect_uri_(redirect_uri),
        request_token_uri_(request_token_uri),
        scope_(scope) {}

  static OAuth2Server GetDefaultInstance();
};

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_OAUTH2_SERVER_H_

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

#include "sync/oauth2_server.h"

namespace mozc {
namespace sync {
namespace {

const char kAuthenticateUri[] = "https://accounts.google.com/o/oauth2/auth";
const char kRedirectUri[] = "urn:ietf:wg:oauth:2.0:oob";
const char kRequestTokenUri[] = "https://accounts.google.com/o/oauth2/token";
// TODO(team): Add https://www.google.com/m8/feeds to support ContactSyncer.
const char kScope[] = "https://www.googleapis.com/auth/imesync";

}  // namespace

// static
OAuth2Server OAuth2Server::GetDefaultInstance() {
  return OAuth2Server(kAuthenticateUri,
                      kRedirectUri,
                      kRequestTokenUri,
                      kScope);
}

}  // namespace sync
}  // namespace mozc

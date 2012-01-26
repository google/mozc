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

#ifndef MOZC_MAC_USER_HISTORY_TRANSITION_DEPRECATED_USER_STORAGE_H_
#define MOZC_MAC_USER_HISTORY_TRANSITION_DEPRECATED_USER_STORAGE_H_

#include "prediction/user_history_predictor.pb.h"

namespace mozc {
namespace mac {
// This is quite similar to UserHistoryStorage in
// prediction/user_history_predictor.(h|cc) but uses the deprecated
// mac's password manager instead of the current one.
class DeprecatedUserHistoryStorage :
    public mozc::user_history_predictor::UserHistory {
 public:
  explicit DeprecatedUserHistoryStorage(const string &filename);
  ~DeprecatedUserHistoryStorage();

  // Load from encrypted file.
  bool Load();

  // This class intentionally does not have Save() method because we
  // don't need to store in the deprecated format.
 private:
  string filename_;
};
}  // namespace mozc::mac
}  // namespace mozc

#endif  // MOZC_MAC_USER_HISTORY_TRANSITION_DEPRECATED_USER_STORAGE_H_

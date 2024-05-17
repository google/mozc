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

#include "dictionary/suppression_dictionary.h"

#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"

namespace mozc {
namespace dictionary {

bool SuppressionDictionary::AddEntry(std::string key, std::string value)
    ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
  if (key.empty() && value.empty()) {
    LOG(WARNING) << "Both key and value are empty";
    return false;
  }

  if (key.empty()) {
    values_only_.insert(std::move(value));
  } else if (value.empty()) {
    keys_only_.insert(std::move(key));
  } else {
    keys_values_.emplace(std::move(key), std::move(value));
  }

  return true;
}

void SuppressionDictionary::Clear() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
  keys_values_.clear();
  keys_only_.clear();
  values_only_.clear();
}

void SuppressionDictionary::Lock() ABSL_EXCLUSIVE_LOCK_FUNCTION(mutex_) {
  mutex_.Lock();
}

void SuppressionDictionary::UnLock() ABSL_UNLOCK_FUNCTION(mutex_) {
  mutex_.Unlock();
}

bool SuppressionDictionary::IsEmpty() const {
  if (mutex_.TryLock()) {
    bool is_empty =
        keys_only_.empty() && values_only_.empty() && keys_values_.empty();
    mutex_.Unlock();
    return is_empty;
  }

  return true;
}

bool SuppressionDictionary::SuppressEntry(const absl::string_view key,
                                          const absl::string_view value) const {
  if (mutex_.TryLock()) {
    if (keys_only_.empty() && values_only_.empty() && keys_values_.empty()) {
      // Almost all users don't use word suppression function.
      // We can return false as early as possible.
      mutex_.Unlock();
      return false;
    }

    bool suppress = keys_values_.contains(std::make_pair(key, value)) ||
                    keys_only_.contains(key) || values_only_.contains(value);
    mutex_.Unlock();
    return suppress;
  }

  return false;
}

}  // namespace dictionary
}  // namespace mozc

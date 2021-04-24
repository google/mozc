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

#include <atomic>
#include <thread>  // NOLINT for portability

#include "base/logging.h"
#include "base/mutex.h"

namespace mozc {
namespace dictionary {
namespace {

constexpr char kDelimiter = '\t';

class Unlocker final {
 public:
  explicit Unlocker(std::atomic<bool> *locked) : locked_{locked} {}
  ~Unlocker() { locked_->store(false, std::memory_order_release); }

 private:
  std::atomic<bool> *locked_;
};

}  // namespace

SuppressionDictionary::SuppressionDictionary()
    : has_key_empty_(false), has_value_empty_(false), locked_(false) {}

SuppressionDictionary::~SuppressionDictionary() = default;

bool SuppressionDictionary::AddEntry(const std::string &key,
                                     const std::string &value) {
  if (!locked_.load(std::memory_order_relaxed)) {
    LOG(ERROR) << "Dictionary is not locked";
    return false;
  }

  if (key.empty() && value.empty()) {
    LOG(WARNING) << "Both key and value are empty";
    return false;
  }

  if (key.empty()) {
    has_key_empty_ = true;
  }

  if (value.empty()) {
    has_value_empty_ = true;
  }

  dic_.insert(key + kDelimiter + value);

  return true;
}

void SuppressionDictionary::Clear() {
  if (!locked_.load(std::memory_order_relaxed)) {
    LOG(ERROR) << "Dictionary is not locked";
    return;
  }
  has_key_empty_ = false;
  has_value_empty_ = false;
  dic_.clear();
}

void SuppressionDictionary::Lock() {
  scoped_lock l(&mutex_);  // TODO(noriyukit): Check if we need this lock.
  for (;;) {
    bool expected = false;
    if (locked_.compare_exchange_weak(expected, true, std::memory_order_acquire,
                                      std::memory_order_relaxed)) {
      return;
    }
    std::this_thread::yield();
  }
}

void SuppressionDictionary::UnLock() {
  scoped_lock l(&mutex_);  // TODO(noriyukit): Check if we need this lock.
  bool expected = true;
  if (!locked_.compare_exchange_weak(expected, false, std::memory_order_release,
                                     std::memory_order_relaxed)) {
    LOG(DFATAL) << "The dictionary was not locked";
  }
}

bool SuppressionDictionary::IsEmpty() const {
  bool expected = false;
  if (!locked_.compare_exchange_weak(expected, true, std::memory_order_acquire,
                                     std::memory_order_relaxed)) {
    VLOG(2) << "Dictionary is locked now";
    return true;
  }
  Unlocker u(&locked_);
  return dic_.empty();
}

bool SuppressionDictionary::SuppressEntry(const std::string &key,
                                          const std::string &value) const {
  bool expected = false;
  if (!locked_.compare_exchange_weak(expected, true, std::memory_order_acquire,
                                     std::memory_order_relaxed)) {
    VLOG(2) << "Dictionary is locked now";
    return false;
  }
  Unlocker u(&locked_);

  if (dic_.empty()) {
    // Almost all users don't use word suppression function.
    // We can return false as early as possible.
    return false;
  }

  std::string lookup_key = key;
  lookup_key.append(1, kDelimiter).append(value);
  if (dic_.find(lookup_key) != dic_.end()) {
    return true;
  }

  if (has_key_empty_) {
    lookup_key.assign(1, kDelimiter).append(value);
    if (dic_.find(lookup_key) != dic_.end()) {
      return true;
    }
  }

  if (has_value_empty_) {
    lookup_key.assign(key).append(1, kDelimiter);
    if (dic_.find(lookup_key) != dic_.end()) {
      return true;
    }
  }

  return false;
}

}  // namespace dictionary
}  // namespace mozc

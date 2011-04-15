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

#include "dictionary/suppression_dictionary.h"

#include "base/base.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "converter/node.h"

namespace mozc {
namespace {
const char kDelimiter = '\t';
}

bool SuppressionDictionary::AddEntry(
    const string &key, const string &value) {
  if (!locked_) {
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
  if (!locked_) {
    LOG(ERROR) << "Dictionary is not locked";
    return;
  }
  has_key_empty_ = false;
  has_value_empty_ = false;
  dic_.clear();
}

void SuppressionDictionary::Lock() {
  scoped_lock l(&mutex_);
  locked_ = true;
}

void SuppressionDictionary::UnLock() {
  scoped_lock l(&mutex_);
  locked_ = false;
}

bool SuppressionDictionary::IsEmpty() const {
  if (locked_) {
    return true;
  }
  return dic_.empty();
}

bool SuppressionDictionary::SuppressEntry(
    const string &key, const string &value) const {
  if (dic_.empty()) {
    // Almost all users don't use word supresssion function.
    // We can return false as early as possible
    return false;
  }

  if (locked_) {
    VLOG(2) << "Dictionary is locked now";
    return false;
  }

  if (dic_.find(key + kDelimiter + value) != dic_.end()) {
    return true;
  }

  if (has_key_empty_ &&
      dic_.find(kDelimiter + value) != dic_.end()) {
    return true;
  }

  if (has_value_empty_ &&
      dic_.find(key + kDelimiter) != dic_.end()) {
    return true;
  }

  return false;
}

Node *SuppressionDictionary::SuppressNodes(Node *node) const {
  // In almost all cases, user don't use suppression feature.
  if (IsEmpty()) {
    return node;
  }

  Node *head = NULL;
  Node *prev = NULL;

  for (Node *n = node; n != NULL; n = n->bnext) {
    if (SuppressEntry(n->key, n->value)) {
      continue;
    }
    if (head == NULL) {
      head = n;
    }
    if (prev != NULL) {
      prev->bnext = n;
    }
    prev = n;
  }

  if (prev != NULL) {
    prev->bnext = NULL;
  }

  return head;
}

// Return Singleton object
SuppressionDictionary *SuppressionDictionary::GetSuppressionDictionary() {
  return Singleton<SuppressionDictionary>::get();
}

SuppressionDictionary::SuppressionDictionary()
    : locked_(false), has_key_empty_(false), has_value_empty_(false) {}

SuppressionDictionary::~SuppressionDictionary() {}
}  // mozc

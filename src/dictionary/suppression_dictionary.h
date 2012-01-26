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

#ifndef MOZC_DICTIONARY_SUPPRESSION_DICTIONARY_H_
#define MOZC_DICTIONARY_SUPPRESSION_DICTIONARY_H_

#include <set>
#include <string>
#include "base/base.h"
#include "base/mutex.h"

namespace mozc {

struct Node;

class SuppressionDictionary {
 public:
  // Lock dictioanry.
  // call Lock() before calling AddWord() or Clear();
  // When the dictionary is locked, Supress() return false.
  //
  // NOTE:
  // Lock() and SupressWord() must be called synchronously.
  void Lock();

  // Unlock dictionary
  void UnLock();

  // return true if the dictionary is locked.
  bool IsLocked() const {
    return locked_;
  }

  // Note that AddWord is not thread safe.
  bool AddEntry(const string &key, const string &value);

  // Note that AddWord is not thread safe.
  void Clear();

  // return true if SuppressionDictionary doesn't have any entries.
  bool IsEmpty() const;

  // Return true if |word| should be suppressed.
  // if the current dictionay is "locked" via Lock() method,
  // this function always return false.
  // Lock() and SuppressWord() must be called synchronously.
  bool SuppressEntry(const string &key, const string &value) const;

  // Suppress node from a linked list of nodes.
  // Return new (modified) linked list.
  Node *SuppressNodes(Node *node) const;

  // Return Singleton object
  static SuppressionDictionary *GetSuppressionDictionary();

  SuppressionDictionary();
  virtual ~SuppressionDictionary();

 private:
  set<string> dic_;
  bool locked_;
  bool has_key_empty_;
  bool has_value_empty_;
  Mutex mutex_;
};
}

#endif  // MOZC_DICTIONARY_SUPPRESSION_DICTIONARY_H_

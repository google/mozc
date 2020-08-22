// Copyright 2010-2020, Google Inc.
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

#include "base/mutex.h"
#include "base/port.h"

namespace mozc {
namespace dictionary {

class SuppressionDictionary {
 public:
  SuppressionDictionary();
  virtual ~SuppressionDictionary();

  // Locks dictionary.
  // Need to lock before calling AddEntry() or Clear().
  // When the dictionary is locked, Supress() return false.
  //
  // NOTE:
  // Lock() and SupressWord() must be called synchronously.
  void Lock();

  // Unlocks dictionary.
  void UnLock();

  // Returns true if the dictionary is locked.
  bool IsLocked() const { return locked_; }

  // Note: this method is thread unsafe.
  bool AddEntry(const std::string &key, const std::string &value);

  // Note: this method is thread unsafe.
  void Clear();

  // Returns true if SuppressionDictionary doesn't have any entries.
  bool IsEmpty() const;

  // Returns true if |word| should be suppressed.  If the current dictionay is
  // "locked" via Lock() method, this function always return false.  Lock() and
  // SuppressWord() must be called synchronously.
  bool SuppressEntry(const std::string &key, const std::string &value) const;

 private:
  std::set<std::string> dic_;
  bool locked_;
  bool has_key_empty_;
  bool has_value_empty_;
  Mutex mutex_;

  DISALLOW_COPY_AND_ASSIGN(SuppressionDictionary);
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SUPPRESSION_DICTIONARY_H_

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

#ifndef MOZC_COMPOSER_INTERNAL_TYPING_CORRECTOR_H_
#define MOZC_COMPOSER_INTERNAL_TYPING_CORRECTOR_H_

#include <string>
#include <utility>
#include <vector>

#include "base/port.h"
#include "base/protobuf/repeated_field.h"
#include "protocol/config.pb.h"
#include "absl/strings/string_view.h"

namespace mozc {

namespace commands {
class KeyEvent;
class KeyEvent_ProbableKeyEvent;
}  // namespace commands

namespace composer {

class Table;
struct TypeCorrectedQuery;

typedef commands::KeyEvent_ProbableKeyEvent ProbableKeyEvent;
typedef mozc::protobuf::RepeatedPtrField<ProbableKeyEvent> ProbableKeyEvents;

class TypingCorrector {
 public:
  // Keeps up to |max_correction_query_candidates| corrections at each
  // insertion.
  // Returns up to |max_correction_query_results| results from
  // GetQueriesForPrediction.
  TypingCorrector(const Table *table, size_t max_correction_query_candidates,
                  size_t max_correction_query_results);
  ~TypingCorrector();

  // Sets a romaji table.
  void SetTable(const Table *table);

  void SetConfig(const config::Config *config);

  // Resets this instance as a copy of |src|.
  void CopyFrom(const TypingCorrector &src);

  // Returns true if there is typing correction available.
  bool IsAvailable() const;

  // Invalidates the all corrections.
  void Invalidate();

  // Resets to the default state.
  void Reset();

  // Inserts a character, represented by a key or a ProbableKeyEvents,
  // and performs online typing correction.
  // If |probable_key_events| is non-empty, |key| is ignored.
  // If |probable_key_events| is empty, |key| is used instead assuming that
  // the probability is 1.0.
  void InsertCharacter(const absl::string_view key,
                       const ProbableKeyEvents &probable_key_events);

  // Extracts type-corrected queries for prediction.
  void GetQueriesForPrediction(std::vector<TypeCorrectedQuery> *queries) const;

 private:
  friend class TypingCorrectorTest;

  // Represents one type-correction: key sequence and its penalty (cost).
  typedef std::pair<std::string, int> KeyAndPenalty;

  // Less-than comparator for KeyAndPenalty. Since this functor accesses
  // KeyAndPenalty, we need to define it in private member.
  struct KeyAndPenaltyLess;

  bool available_;
  const Table *table_;
  size_t max_correction_query_candidates_;
  size_t max_correction_query_results_;
  const config::Config *config_;
  std::string raw_key_;
  std::vector<KeyAndPenalty> top_n_;

  DISALLOW_COPY_AND_ASSIGN(TypingCorrector);
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_TYPING_CORRECTOR_H_

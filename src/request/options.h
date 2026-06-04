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

#ifndef MOZC_REQUEST_OPTIONS_H_
#define MOZC_REQUEST_OPTIONS_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace mozc {

inline constexpr size_t kMaxConversionCandidatesSize = 200;

enum RequestType {
  CONVERSION,          // normal conversion
  REVERSE_CONVERSION,  // reverse conversion
  PREDICTION,          // show prediction with user tab key
  SUGGESTION,          // show prediction automatically
  PARTIAL_PREDICTION,  // show prediction using the text before cursor
  PARTIAL_SUGGESTION,  // show suggestion using the text before cursor
};

enum ComposerKeySelection {
  // Use Composer::GetQueryForConversion() to generate conversion key. This
  // option uses the exact composition which user sees, e.g., "とうk".
  CONVERSION_KEY,

  // Use Composer::GetQueryForPrediction() to generate conversion key. This
  // option trims the trailing unresolved romaji. For example, if composition
  // is "とうk", the conversion key becomes "とう". This option is mainly used
  // in dictionary_predictor.cc for realtime conversion.
  PREDICTION_KEY,

  // TODO(team): We may want to implement other options. For instance,
  // Composer::GetQueriesForPrediction() expands the trailing romaji to a set
  // of possible hiragana.
};

// Options must be trivially copyable to get hash value directly.
// Since it is small (~40 bytes), passing and returning by value is preferred
// to avoid reference lifetime issues.
struct ConversionOptions {
  RequestType request_type = CONVERSION;

  // Which composer's method to use for conversion key; see the comment around
  // the definition of ComposerKeySelection above.
  ComposerKeySelection composer_key_selection = CONVERSION_KEY;

  int max_conversion_candidates_size = kMaxConversionCandidatesSize;
  int max_user_history_prediction_candidates_size = 3;
  int max_user_history_prediction_candidates_size_for_zero_query = 4;
  int max_dictionary_prediction_candidates_size = 20;

  // If true, insert a top candidate from the actual (non-immutable) converter
  // to realtime conversion results. Note that setting this true causes a big
  // performance loss (3 times slower).
  bool use_actual_converter_for_realtime_conversion = false;

  // Don't use this flag directly. This flag is used by DictionaryPredictor to
  // skip some heavy rewriters, such as UserBoundaryHistoryRewriter and
  // TransliterationRewriter.
  // TODO(noriyukit): Fix such a hacky handling for realtime conversion.
  bool skip_slow_rewriters = false;

  // If true, partial candidates are created on prediction/suggestion.
  // For example, "私の" is created from composition "わたしのなまえ".
  bool create_partial_candidates = false;

  // If false, stop using user history for conversion.
  // This is used for supporting CONVERT_WITHOUT_HISTORY command.
  // Please refer to session/internal/keymap.h
  bool enable_user_history_for_conversion = true;

  // If true, enable kana modifier insensitive conversion.
  bool kana_modifier_insensitive_conversion = true;

  // If true, use conversion_segment(0).key() instead of ComposerData.
  // TODO(b/365909808): Create a new string field to store the key.
  bool use_already_typing_corrected_key = false;

  // Enables incognito mode even when Config.incognito_mode() or
  // Request.is_incognito_mode() is false. Use this flag to dynamically change
  // the incognito_mode per client request.
  bool incognito_mode = false;

  // Overrides the bos_id to a specific POS id to specify the context
  // information. Note that the bos_id must be in the valid range.
  // POS id 0 is reserved for the default BOS/EOS.
  uint16_t bos_id = 0;

  // Disables to add prefix penalty.
  // Used to calculate the cost of a suffix of a word.
  bool disable_prefix_penalty = false;

  // This conversion request is called by predictor for realtime conversion.
  bool used_in_predictor_realtime_conversion = false;
};

static_assert(std::is_trivially_copyable<ConversionOptions>::value,
              "ConversionOptions must be trivially copyable");

}  // namespace mozc

#endif  // MOZC_REQUEST_OPTIONS_H_

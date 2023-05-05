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

#ifndef MOZC_REQUEST_CONVERSION_REQUEST_H_
#define MOZC_REQUEST_CONVERSION_REQUEST_H_

#include <string>

#include "base/port.h"

namespace mozc {
inline constexpr size_t kMaxConversionCandidatesSize = 200;

// Protocol buffers, commands::Request and config::Config should be forward
// declaration instead of include header files.  Otherwise, we need to specify
// 'hard_dependency' to all affected rules in the GYP files.
namespace commands {
class Request;
}  // namespace commands

namespace composer {
class Composer;
}  // namespace composer

namespace config {
class Config;
}  // namespace config

// Contains utilizable information for conversion, suggestion and prediction,
// including composition, preceding text, etc.
// This class doesn't take ownerships of any Composer* argument.
// TODO(team, yukawa): Refactor this class so it can represents that more
// detailed context information such as commands::Context.
class ConversionRequest {
 public:
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

  ConversionRequest();
  ConversionRequest(const composer::Composer *c,
                    const commands::Request *request,
                    const config::Config *config);

  // This class is copyable.
  ConversionRequest(const ConversionRequest &x);
  ConversionRequest &operator=(const ConversionRequest &x);

  ~ConversionRequest();

  RequestType request_type() const;
  void set_request_type(RequestType request_type);

  bool has_composer() const;
  const composer::Composer &composer() const;
  void set_composer(const composer::Composer *c);

  bool use_actual_converter_for_realtime_conversion() const;
  void set_use_actual_converter_for_realtime_conversion(bool value);

  bool create_partial_candidates() const;
  void set_create_partial_candidates(bool value);

  bool enable_user_history_for_conversion() const;
  void set_enable_user_history_for_conversion(bool value);

  ComposerKeySelection composer_key_selection() const;
  void set_composer_key_selection(ComposerKeySelection selection);

  const commands::Request &request() const;
  void set_request(const commands::Request *request);

  const config::Config &config() const;
  void set_config(const config::Config *config);

  // TODO(noriyukit): Remove these methods after removing skip_slow_rewriters_
  // flag.
  bool skip_slow_rewriters() const;
  void set_skip_slow_rewriters(bool value);

  bool IsKanaModifierInsensitiveConversion() const;

  size_t max_conversion_candidates_size() const;
  void set_max_conversion_candidates_size(size_t value);

  size_t max_user_history_prediction_candidates_size() const;
  void set_max_user_history_prediction_candidates_size(size_t value);

  size_t max_user_history_prediction_candidates_size_for_zero_query() const;
  void set_max_user_history_prediction_candidates_size_for_zero_query(
      size_t value);

  size_t max_dictionary_prediction_candidates_size() const;
  void set_max_dictionary_prediction_candidates_size(size_t value);

  bool should_call_set_key_in_prediction() const;
  void set_should_call_set_key_in_prediction(bool value);

 private:
  RequestType request_type_ = CONVERSION;

  // Required fields
  // Input composer to generate a key for conversion, suggestion, etc.
  const composer::Composer *composer_;

  // Input request.
  const commands::Request *request_;

  // Input config.
  const config::Config *config_;

  // Which composer's method to use for conversion key; see the comment around
  // the definition of ComposerKeySelection above.
  ComposerKeySelection composer_key_selection_ = CONVERSION_KEY;

  int max_conversion_candidates_size_ = kMaxConversionCandidatesSize;
  int max_user_history_prediction_candidates_size_ = 3;
  int max_user_history_prediction_candidates_size_for_zero_query_ = 4;
  int max_dictionary_prediction_candidates_size_ = 20;

  // If true, insert a top candidate from the actual (non-immutable) converter
  // to realtime conversion results. Note that setting this true causes a big
  // performance loss (3 times slower).
  bool use_actual_converter_for_realtime_conversion_ = false;

  // Don't use this flag directly. This flag is used by DictionaryPredictor to
  // skip some heavy rewriters, such as UserBoundaryHistoryRewriter and
  // TransliterationRewriter.
  // TODO(noriyukit): Fix such a hacky handling for realtime conversion.
  bool skip_slow_rewriters_ = false;

  // If true, partial candidates are created on prediction/suggestion.
  // For example, "私の" is created from composition "わたしのなまえ".
  bool create_partial_candidates_ = false;

  // If false, stop using user history for conversion.
  // This is used for supporting CONVERT_WITHOUT_HISTORY command.
  // Please refer to session/internal/keymap.h
  bool enable_user_history_for_conversion_ = true;

  // If true, set conversion key to output segments in prediction.
  bool should_call_set_key_in_prediction_ = false;

  // TODO(noriyukit): Moves all the members of Segments that are irrelevant to
  // this structure, e.g., Segments::request_type_.
  // Also, a key for conversion is eligible to live in this class.
};

}  // namespace mozc

#endif  // MOZC_REQUEST_CONVERSION_REQUEST_H_

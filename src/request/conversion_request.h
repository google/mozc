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

#include <cstddef>

#include "absl/log/check.h"
#include "composer/composer.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
inline constexpr size_t kMaxConversionCandidatesSize = 200;

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

  ConversionRequest()
      : request_(&commands::Request::default_instance()),
        context_(&commands::Context::default_instance()),
        config_(&config::ConfigHandler::DefaultConfig()) {}

  ConversionRequest(const composer::Composer &composer,
                    const commands::Request *request,
                    const commands::Context *context,
                    const config::Config *config)
      : has_composer_(true),
        composer_(composer.CreateComposerData()),
        request_(request),
        context_(context),
        config_(config) {}

  // Copyable and movable.
  ConversionRequest(const ConversionRequest &) = default;
  ConversionRequest &operator=(const ConversionRequest &) = default;
  ConversionRequest(ConversionRequest &&) = default;
  ConversionRequest &operator=(ConversionRequest &&) = default;

  RequestType request_type() const { return request_type_; }
  void set_request_type(RequestType request_type) {
    request_type_ = request_type;
  }

  bool has_composer() const { return has_composer_; }
  const composer::ComposerData &composer() const {
    DCHECK(has_composer_);
    return composer_;
  }

  void reset_composer() { has_composer_ = false; }

  bool use_actual_converter_for_realtime_conversion() const {
    return use_actual_converter_for_realtime_conversion_;
  }
  void set_use_actual_converter_for_realtime_conversion(bool value) {
    use_actual_converter_for_realtime_conversion_ = value;
  }

  bool create_partial_candidates() const { return create_partial_candidates_; }
  void set_create_partial_candidates(bool value) {
    create_partial_candidates_ = value;
  }

  bool enable_user_history_for_conversion() const {
    return enable_user_history_for_conversion_;
  }
  void set_enable_user_history_for_conversion(bool value) {
    enable_user_history_for_conversion_ = value;
  }

  ComposerKeySelection composer_key_selection() const {
    return composer_key_selection_;
  }
  void set_composer_key_selection(ComposerKeySelection selection) {
    composer_key_selection_ = selection;
  }

  const commands::Request &request() const {
    DCHECK(request_);
    return *request_;
  }
  void set_request(const commands::Request *request) { request_ = request; }

  const commands::Context &context() const {
    DCHECK(context_);
    return *context_;
  }
  void set_context(const commands::Context *context) { context_ = context; }

  const config::Config &config() const {
    DCHECK(config_);
    return *config_;
  }
  void set_config(const config::Config *config) { config_ = config; }

  // TODO(noriyukit): Remove these methods after removing skip_slow_rewriters_
  // flag.
  bool skip_slow_rewriters() const { return skip_slow_rewriters_; }
  void set_skip_slow_rewriters(bool value) { skip_slow_rewriters_ = value; }

  bool IsKanaModifierInsensitiveConversion() const {
    return request_->kana_modifier_insensitive_conversion() &&
           config_->use_kana_modifier_insensitive_conversion() &&
           kana_modifier_insensitive_conversion_;
  }

  size_t max_conversion_candidates_size() const {
    return max_conversion_candidates_size_;
  }
  void set_max_conversion_candidates_size(size_t value) {
    max_conversion_candidates_size_ = value;
  }

  size_t max_user_history_prediction_candidates_size() const {
    return max_user_history_prediction_candidates_size_;
  }
  void set_max_user_history_prediction_candidates_size(size_t value) {
    max_user_history_prediction_candidates_size_ = value;
  }

  size_t max_user_history_prediction_candidates_size_for_zero_query() const {
    return max_user_history_prediction_candidates_size_for_zero_query_;
  }
  void set_max_user_history_prediction_candidates_size_for_zero_query(
      size_t value) {
    max_user_history_prediction_candidates_size_for_zero_query_ = value;
  }

  size_t max_dictionary_prediction_candidates_size() const {
    return max_dictionary_prediction_candidates_size_;
  }
  void set_max_dictionary_prediction_candidates_size(size_t value) {
    max_dictionary_prediction_candidates_size_ = value;
  }

  bool should_call_set_key_in_prediction() const {
    return should_call_set_key_in_prediction_;
  }
  void set_should_call_set_key_in_prediction(bool value) {
    should_call_set_key_in_prediction_ = value;
  }

  void set_kana_modifier_insensitive_conversion(bool value) {
    kana_modifier_insensitive_conversion_ = value;
  }

 private:
  RequestType request_type_ = CONVERSION;

  // Required fields
  // Input composer to generate a key for conversion, suggestion, etc.
  bool has_composer_ = false;
  composer::ComposerData composer_;

  // Input request.
  const commands::Request *request_;

  // Input context.
  const commands::Context *context_;

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

  // If true, enable kana modifier insensitive conversion.
  bool kana_modifier_insensitive_conversion_ = true;

  // TODO(noriyukit): Moves all the members of Segments that are irrelevant to
  // this structure, e.g., Segments::request_type_.
  // Also, a key for conversion is eligible to live in this class.
};
}  // namespace mozc

#endif  // MOZC_REQUEST_CONVERSION_REQUEST_H_

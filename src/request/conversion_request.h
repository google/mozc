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
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/strings/assign.h"
#include "base/util.h"
#include "composer/composer.h"
#include "config/config_handler.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
inline constexpr size_t kMaxConversionCandidatesSize = 200;

namespace internal {

// Helper class that holds either the view or copy of T.
template <typename T>
class copy_or_view_ptr {
 public:
  copy_or_view_ptr() = default;
  ~copy_or_view_ptr() = default;
  // default constructor stores the view.
  copy_or_view_ptr(T &view ABSL_ATTRIBUTE_LIFETIME_BOUND) : view_(&view) {};
  copy_or_view_ptr(copy_or_view_ptr<T> &other) { CopyFrom(other); }
  copy_or_view_ptr(const copy_or_view_ptr<T> &other) { CopyFrom(other); }
  copy_or_view_ptr(copy_or_view_ptr<T> &&other) { MoveFrom(std::move(other)); }

  constexpr T &operator*() const { return *view_; }
  constexpr T *operator->() { return view_; }
  constexpr const T *operator->() const { return view_; }
  explicit operator bool() const { return view_ != nullptr; }

  constexpr void set_view(T &view) {
    view_ = &view;
    copy_.reset();
  }

  constexpr void copy_from(T &copy) {
    copy_ = std::make_unique<T>(copy);
    view_ = copy_.get();
  }

  constexpr void move_from(T &&other) {
    copy_ = std::make_unique<T>(std::move(other));
    view_ = copy_.get();
  }

  constexpr copy_or_view_ptr<T> &operator=(const copy_or_view_ptr<T> &other) {
    CopyFrom(other);
    return *this;
  }

  constexpr copy_or_view_ptr<T> &operator=(copy_or_view_ptr<T> &&other) {
    MoveFrom(std::move(other));
    return *this;
  }

 private:
  void CopyFrom(const copy_or_view_ptr<T> &other) {
    if (other.copy_) {
      copy_ = std::make_unique<T>(*other.copy_);
      view_ = copy_.get();
    } else {
      view_ = other.view_;
      copy_.reset();
    }
  }

  void MoveFrom(copy_or_view_ptr<T> &&other) {
    if (other.copy_) {
      copy_ = std::move(other.copy_);
      view_ = copy_.get();
    } else {
      view_ = other.view_;
      copy_.reset();
    }
  }

  T *view_ = nullptr;
  std::unique_ptr<T> copy_;
};
}  // namespace internal

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

  // Options must be trivially copyable to get hash value directly.
  struct Options {
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
  };

  static_assert(std::is_trivially_copyable<Options>::value,
                "Options must be trivially copyable");

  // Default constructor stores the view.
  // All default variable returns global reference.
  ConversionRequest()
      : composer_data_(composer::Composer::EmptyComposerData()),
        request_(commands::Request::default_instance()),
        context_(commands::Context::default_instance()),
        config_(config::ConfigHandler::DefaultConfig()),
        options_(Options()) {}

  ConversionRequest(const ConversionRequest &) = default;
  ConversionRequest(ConversionRequest &&) = default;

  // operator= are not available since this class has a const member.
  ConversionRequest &operator=(const ConversionRequest &) = delete;
  ConversionRequest &operator=(ConversionRequest &&) = delete;

  RequestType request_type() const { return options_.request_type; }

  const composer::ComposerData &composer() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *composer_data_;
  }

  bool use_actual_converter_for_realtime_conversion() const {
    return options_.use_actual_converter_for_realtime_conversion;
  }

  bool create_partial_candidates() const {
    return options_.create_partial_candidates;
  }

  bool enable_user_history_for_conversion() const {
    return options_.enable_user_history_for_conversion;
  }

  ComposerKeySelection composer_key_selection() const {
    return options_.composer_key_selection;
  }

  const commands::Request &request() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *request_;
  }
  const commands::Context &context() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *context_;
  }
  const config::Config &config() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *config_;
  }
  const Options &options() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return options_;
  }

  // TODO(noriyukit): Remove these methods after removing skip_slow_rewriters_
  // flag.
  bool skip_slow_rewriters() const { return options_.skip_slow_rewriters; }

  bool IsKanaModifierInsensitiveConversion() const {
    return request_->kana_modifier_insensitive_conversion() &&
           config_->use_kana_modifier_insensitive_conversion() &&
           options_.kana_modifier_insensitive_conversion;
  }

  bool IsZeroQuerySuggestion() const { return key().empty(); }

  size_t max_conversion_candidates_size() const {
    return options_.max_conversion_candidates_size;
  }

  size_t max_user_history_prediction_candidates_size() const {
    return options_.max_user_history_prediction_candidates_size;
  }

  size_t max_user_history_prediction_candidates_size_for_zero_query() const {
    return options_.max_user_history_prediction_candidates_size_for_zero_query;
  }

  size_t max_dictionary_prediction_candidates_size() const {
    return options_.max_dictionary_prediction_candidates_size;
  }

  bool use_already_typing_corrected_key() const {
    return options_.use_already_typing_corrected_key;
  }

  // Clients needs to check ConversionRequest::incognito_mode() instead
  // of Config::incognito_mode() or Request::is_incognito_mode(), as the
  // incognito mode can also set via Options.
  bool incognito_mode() const {
    return options_.incognito_mode || config_->incognito_mode() ||
           request_->is_incognito_mode();
  }

  absl::string_view key() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return key_; }

  // Takes the last `size` history key. return all key when size = -1.
  std::string converter_history_key(int size = -1) const {
    return segments_ ? segments_->history_key(size) : "";
  }

  // Takes the last `size` history value. return all value when size = -1.
  std::string converter_history_value(int size = -1) const {
    return segments_ ? segments_->history_value(size) : "";
  }

  size_t converter_history_size() const {
    return segments_ ? segments_->history_segments_size() : 0;
  }

  // Returns the right context id of the history.
  int converter_history_rid() const {
    if (const converter::Candidate *candidate = last_history_candidate();
        candidate) {
      return candidate->rid;
    }
    return 0;
  }

  // Returns the cost of the history if defined.
  std::optional<int> converter_history_cost() const {
    if (const converter::Candidate *candidate = last_history_candidate();
        candidate) {
      return candidate->cost;
    }
    return std::nullopt;
  }

  // Temporal API to make legacy Segments from request.
  // This API is used in the components (e.g.. immutable converter) that
  // use Segments as the decoder request.
  // TODO(b/409183257): remove this API after removing the dependency
  // from ConverterRequest to Segments.
  converter::Segments MakeRequestSegments() const {
    // Needs to call SetHistorySegmentsView to use this method.
    DCHECK(segments_);
    converter::Segments segments =
        segments_ ? *segments_ : converter::Segments();
    if (segments.conversion_segments_size() == 0) {
      segments.add_segment()->set_key(key());
    }
    segments.mutable_conversion_segment(0)->clear_candidates();
    return segments;
  }

  // Returns true if request has the legacy segments structure.
  // This method is only used in the segments to request migration.
  bool HasConverterHistorySegments() const {
    return static_cast<bool>(segments_);
  }

  // Temporal API to accesses raw history segments.
  // TODO(b/409183257): Better to return 'Result' instead.
  struct HistorySegment {
    absl::string_view key;
    absl::string_view value;
    absl::string_view content_key;
    absl::string_view content_value;
  };

  std::vector<HistorySegment> GetConverterHistorySegments() const {
    if (!segments_) return {};
    std::vector<HistorySegment> results;
    for (const converter::Segment &segment : segments_->history_segments()) {
      DCHECK_LE(1, segment.candidates_size());
      const auto &candidate = segment.candidate(0);
      results.push_back({candidate.key, candidate.value, candidate.content_key,
                         candidate.content_value});
    }
    return results;
  }

  // Builder can access the private member for construction.
  friend class ConversionRequestBuilder;

 private:
  const converter::Candidate *last_history_candidate() const {
    if (!segments_ || segments_->history_segments_size() == 0) {
      return nullptr;
    }
    const converter::Segment &history_segment =
        segments_->history_segment(segments_->history_segments_size() - 1);
    if (history_segment.candidates_size() == 0) {
      return nullptr;
    }
    return &history_segment.candidate(0);
  }

  // Required options
  // Input composer to generate a key for conversion, suggestion, etc.
  internal::copy_or_view_ptr<const composer::ComposerData> composer_data_;

  // Input request.
  internal::copy_or_view_ptr<const commands::Request> request_;

  // Input context.
  internal::copy_or_view_ptr<const commands::Context> context_;

  // Input config.
  internal::copy_or_view_ptr<const config::Config> config_;

  // Stores segments to access legacy context key/value stored in Segments.
  // Actual segments is NOT exposed to users to get rid of the dependency from
  // supplemental model to Segments. See converter_history_(key|value) methods.
  // TODO(taku): Migrate them to context proto to feed the context information
  // from the client to decoder.
  internal::copy_or_view_ptr<const converter::Segments> segments_;

  // Options for conversion request.
  Options options_;

  // Key used for conversion.
  // This is typically a Hiragana text to be converted to Kanji words.
  std::string key_;
};

class ConversionRequestBuilder {
 public:
  ConversionRequest Build() {
    // If the key is specified, use it. Otherwise, generate it.
    // NOTE: Specifying Composer is preferred over specifying key directly.
    DCHECK_LE(stage_, 3);
    stage_ = 100;
    if (request_.key_.empty()) {
      request_.key_ =
          GetKey(*request_.composer_data_, request_.options_.request_type,
                 request_.options_.composer_key_selection);
    }
    return request_;
  }

  ConversionRequestBuilder &SetConversionRequest(
      const ConversionRequest &base_convreq) {
    DCHECK_LE(stage_, 1);
    stage_ = 1;
    // Uses the default copy operator.
    // whether using view or copy depends on the storage type of
    // `base_convreq`.
    request_.composer_data_ = base_convreq.composer_data_;
    request_.request_ = base_convreq.request_;
    request_.context_ = base_convreq.context_;
    request_.config_ = base_convreq.config_;
    request_.segments_ = base_convreq.segments_;
    request_.options_ = base_convreq.options_;
    request_.key_ = base_convreq.key_;
    return *this;
  }
  ConversionRequestBuilder &SetConversionRequestView(
      const ConversionRequest &base_convreq ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    DCHECK_LE(stage_, 1);
    stage_ = 1;
    // Enforces to use the view.
    request_.composer_data_.set_view(*base_convreq.composer_data_);
    request_.request_.set_view(*base_convreq.request_);
    request_.context_.set_view(*base_convreq.context_);
    request_.config_.set_view(*base_convreq.config_);
    request_.options_ = base_convreq.options_;
    request_.key_ = base_convreq.key_;
    if (base_convreq.segments_) {
      request_.segments_.set_view(*base_convreq.segments_);
    }
    return *this;
  }
  ConversionRequestBuilder &SetComposerData(
      composer::ComposerData &&composer_data) {
    DCHECK_LE(stage_, 2);
    stage_ = 2;
    request_.composer_data_.move_from(std::move(composer_data));
    return *this;
  }
  ConversionRequestBuilder &SetComposer(const composer::Composer &composer) {
    DCHECK_LE(stage_, 2);
    stage_ = 2;
    request_.composer_data_.copy_from(composer.CreateComposerData());
    return *this;
  }
  ConversionRequestBuilder &SetRequest(const commands::Request &request) {
    DCHECK_LE(stage_, 2);
    stage_ = 2;
    request_.request_.copy_from(request);
    return *this;
  }
  ConversionRequestBuilder &SetRequestView(
      const commands::Request &request ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    DCHECK_LE(stage_, 2);
    stage_ = 2;
    request_.request_.set_view(request);
    return *this;
  }
  ConversionRequestBuilder &SetContext(const commands::Context &context) {
    DCHECK_LE(stage_, 2);
    stage_ = 2;
    request_.context_.copy_from(context);
    return *this;
  }
  ConversionRequestBuilder &SetContextView(
      const commands::Context &context ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    DCHECK_LE(stage_, 2);
    stage_ = 2;
    request_.context_.set_view(context);
    return *this;
  }
  ConversionRequestBuilder &SetConfig(const config::Config &config) {
    DCHECK_LE(stage_, 2);
    stage_ = 2;
    request_.config_.copy_from(TrimConfig(config));
    return *this;
  }
  ConversionRequestBuilder &SetConfigView(
      const config::Config &config ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    DCHECK_LE(stage_, 2);
    stage_ = 2;
    request_.config_.set_view(config);
    return *this;
  }
  // `segments` contain both conversion segments and history segments,
  // but we only populate the information in history segments.
  // Generally ConversionRequest only stores the request to the
  // converter, while segments both contain request and result.
  ConversionRequestBuilder &SetHistorySegments(
      const converter::Segments &segments ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    DCHECK_LE(stage_, 2);
    stage_ = 2;
    request_.segments_.copy_from(segments);
    return *this;
  }
  ConversionRequestBuilder &SetHistorySegmentsView(
      const converter::Segments &segments ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    DCHECK_LE(stage_, 2);
    stage_ = 2;
    request_.segments_.set_view(segments);
    return *this;
  }
  ConversionRequestBuilder &SetOptions(ConversionRequest::Options &&options) {
    DCHECK_LE(stage_, 2);
    stage_ = 2;
    request_.options_ = std::move(options);
    return *this;
  }
  ConversionRequestBuilder &SetRequestType(
      ConversionRequest::RequestType request_type) {
    DCHECK_LE(stage_, 3);
    stage_ = 3;
    request_.options_.request_type = request_type;
    return *this;
  }
  // We cannot set empty key (SetKey("")). When key is empty,
  // key is created from composer.
  ConversionRequestBuilder &SetKey(absl::string_view key) {
    DCHECK_LE(stage_, 3);
    stage_ = 3;
    strings::Assign(request_.key_, key);
    return *this;
  }

 private:
  // Remove unnecessary but potentially large options for ConversionRequest
  // from Config and return it.
  // TODO(b/365909808): Move this method to Session after updating the
  // ConversionRequest constructor.
  static config::Config TrimConfig(const config::Config &base_config) {
    config::Config config = base_config;
    config.clear_custom_keymap_table();
    config.clear_custom_roman_table();
    return config;
  }

  static std::string GetKey(
      const composer::ComposerData &composer_data,
      const ConversionRequest::RequestType type,
      const ConversionRequest::ComposerKeySelection selection) {
    if (type == ConversionRequest::CONVERSION &&
        selection == ConversionRequest::CONVERSION_KEY) {
      return composer_data.GetQueryForConversion();
    }

    if ((type == ConversionRequest::CONVERSION &&
         selection == ConversionRequest::PREDICTION_KEY) ||
        type == ConversionRequest::PREDICTION ||
        type == ConversionRequest::SUGGESTION) {
      return composer_data.GetQueryForPrediction();
    }

    if (type == ConversionRequest::PARTIAL_PREDICTION ||
        type == ConversionRequest::PARTIAL_SUGGESTION) {
      const std::string prediction_key = composer_data.GetQueryForConversion();
      return std::string(
          Util::Utf8SubString(prediction_key, 0, composer_data.GetCursor()));
    }
    return "";
  }

  // The stage of the builder.
  // 0: No data set
  // 1: ConversionRequest set.
  // 2: ComposerData, Request, Context, Config, Options set.
  // 3: RequestType, Key, as values of Options set.
  // 100: Build() called.
  int stage_ = 0;
  ConversionRequest request_;
};

}  // namespace mozc

#endif  // MOZC_REQUEST_CONVERSION_REQUEST_H_

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

#include "request/conversion_request.h"

#include "base/logging.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"

namespace mozc {

ConversionRequest::ConversionRequest()
    : ConversionRequest(nullptr, &commands::Request::default_instance(),
                        &config::ConfigHandler::DefaultConfig()) {}

ConversionRequest::ConversionRequest(const composer::Composer *c,
                                     const commands::Request *request,
                                     const config::Config *config)
    : request_type_(ConversionRequest::CONVERSION),
      composer_(c),
      request_(request),
      config_(config) {}

ConversionRequest::ConversionRequest(const ConversionRequest &x) = default;
ConversionRequest &ConversionRequest::operator=(const ConversionRequest &x) =
    default;

ConversionRequest::~ConversionRequest() = default;

ConversionRequest::RequestType ConversionRequest::request_type() const {
  return request_type_;
}

void ConversionRequest::set_request_type(
    ConversionRequest::RequestType request_type) {
  request_type_ = request_type;
}

bool ConversionRequest::has_composer() const { return composer_ != nullptr; }

const composer::Composer &ConversionRequest::composer() const {
  DCHECK(composer_);
  return *composer_;
}

void ConversionRequest::set_composer(const composer::Composer *c) {
  composer_ = c;
}

const commands::Request &ConversionRequest::request() const {
  DCHECK(request_);
  return *request_;
}

void ConversionRequest::set_request(const commands::Request *request) {
  request_ = request;
}

const config::Config &ConversionRequest::config() const {
  DCHECK(config_);
  return *config_;
}

void ConversionRequest::set_config(const config::Config *config) {
  config_ = config;
}

bool ConversionRequest::use_actual_converter_for_realtime_conversion() const {
  return use_actual_converter_for_realtime_conversion_;
}

void ConversionRequest::set_use_actual_converter_for_realtime_conversion(
    bool value) {
  use_actual_converter_for_realtime_conversion_ = value;
}

ConversionRequest::ComposerKeySelection
ConversionRequest::composer_key_selection() const {
  return composer_key_selection_;
}

void ConversionRequest::set_composer_key_selection(
    ComposerKeySelection selection) {
  composer_key_selection_ = selection;
}

bool ConversionRequest::skip_slow_rewriters() const {
  return skip_slow_rewriters_;
}

void ConversionRequest::set_skip_slow_rewriters(bool value) {
  skip_slow_rewriters_ = value;
}

bool ConversionRequest::create_partial_candidates() const {
  return create_partial_candidates_;
}

void ConversionRequest::set_create_partial_candidates(bool value) {
  create_partial_candidates_ = value;
}

bool ConversionRequest::enable_user_history_for_conversion() const {
  return enable_user_history_for_conversion_;
}

void ConversionRequest::set_enable_user_history_for_conversion(bool value) {
  enable_user_history_for_conversion_ = value;
}

bool ConversionRequest::IsKanaModifierInsensitiveConversion() const {
  return request_->kana_modifier_insensitive_conversion() &&
         config_->use_kana_modifier_insensitive_conversion() &&
         kana_modifier_insensitive_conversion_;
}

size_t ConversionRequest::max_conversion_candidates_size() const {
  return max_conversion_candidates_size_;
}

void ConversionRequest::set_max_conversion_candidates_size(size_t value) {
  max_conversion_candidates_size_ = value;
}

size_t ConversionRequest::max_user_history_prediction_candidates_size() const {
  return max_user_history_prediction_candidates_size_;
}

void ConversionRequest::set_max_user_history_prediction_candidates_size(
    size_t value) {
  max_user_history_prediction_candidates_size_ = value;
}

size_t
ConversionRequest::max_user_history_prediction_candidates_size_for_zero_query()
    const {
  return max_user_history_prediction_candidates_size_for_zero_query_;
}

void ConversionRequest::
    set_max_user_history_prediction_candidates_size_for_zero_query(
        size_t value) {
  max_user_history_prediction_candidates_size_for_zero_query_ = value;
}

size_t ConversionRequest::max_dictionary_prediction_candidates_size() const {
  return max_dictionary_prediction_candidates_size_;
}

void ConversionRequest::set_max_dictionary_prediction_candidates_size(
    size_t value) {
  max_dictionary_prediction_candidates_size_ = value;
}

bool ConversionRequest::should_call_set_key_in_prediction() const {
  return should_call_set_key_in_prediction_;
}

void ConversionRequest::set_should_call_set_key_in_prediction(bool value) {
  should_call_set_key_in_prediction_ = value;
}

void ConversionRequest::set_kana_modifier_insensitive_conversion(bool value) {
  kana_modifier_insensitive_conversion_ = value;
}

}  // namespace mozc

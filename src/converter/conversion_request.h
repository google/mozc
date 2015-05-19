// Copyright 2010-2013, Google Inc.
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

#ifndef MOZC_CONVERTER_CONVERSION_REQUEST_H_
#define MOZC_CONVERTER_CONVERSION_REQUEST_H_

#include <string>

#include "base/base.h"

namespace mozc {
namespace composer {
class Composer;
}  // namespace composer
namespace commands {
class Request;
}  // namespace commands

// Contains utilizable information for conversion, suggestion and prediction,
// including composition, preceding text, etc.
// This class doesn't take ownerships of any Composer* argument.
class ConversionRequest {
 public:
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
                    const commands::Request *request);
  ~ConversionRequest();

  bool has_composer() const;
  const composer::Composer &composer() const;
  void set_composer(const composer::Composer *c);

  const string &preceding_text() const;
  void set_preceding_text(const string &preceding_text);

  bool use_actual_converter_for_realtime_conversion() const;
  void set_use_actual_converter_for_realtime_conversion(bool value);

  bool create_partial_candidates() const;
  void set_create_partial_candidates(bool value);

  ComposerKeySelection composer_key_selection() const;
  void set_composer_key_selection(ComposerKeySelection selection);

  const commands::Request &request() const;

  void CopyFrom(const ConversionRequest &request);

  // TODO(noriyukit): Remove these methods after removing skip_slow_rewriters_
  // flag.
  bool skip_slow_rewriters() const;
  void set_skip_slow_rewriters(bool value);

  bool IsKanaModifierInsensitiveConversion() const;

 private:
  // Required fields
  // Input composer to generate a key for conversion, suggestion, etc.
  const composer::Composer *composer_;

  // Input request.
  const commands::Request *request_;

  // Optional fields
  // If nonempty, utilizes this preceding text for conversion.
  string preceding_text_;

  // If true, insert a top candidate from the actual (non-immutable) converter
  // to realtime conversion results. Note that setting this true causes a big
  // performance loss (3 times slower).
  bool use_actual_converter_for_realtime_conversion_;

  // Which composer's method to use for conversion key; see the comment around
  // the definition of ComposerKeySelection above.
  ComposerKeySelection composer_key_selection_;

  // Don't use this flag directly. This flag is used by DictionaryPredictor to
  // skip some heavy rewriters, such as UserBoundaryHistoryRewriter and
  // TransliterationRewriter.
  // TODO(noriyukit): Fix such a hacky handling for realtime conversion.
  bool skip_slow_rewriters_;

  // If true, partial candidates are created on prediction/suggestion.
  // For example, "私の" is created from composition "わたしのなまえ".
  bool create_partial_candidates_;

  // TODO(noriyukit): Moves all the members of Segments that are irrelevant to
  // this structure, e.g., Segments::user_history_enabled_ and
  // Segments::request_type_. Also, a key for conversion is eligible to live in
  // this class.

  DISALLOW_COPY_AND_ASSIGN(ConversionRequest);
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_CONVERSION_REQUEST_H_

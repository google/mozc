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

// A class handling the converter on the session layer.

#ifndef MOZC_ENGINE_SESSION_CONVERTER_INTERFACE_H_
#define MOZC_ENGINE_SESSION_CONVERTER_INTERFACE_H_

#include <cstddef>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "composer/composer.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace engine {

struct ConversionPreferences {
  bool use_history;

  // This is a flag to check if the converter should return the suggestion
  // or not. Indeed, the design is actually twisted, because clients should
  // be able to avoid the invocation of EngineConverter::Suggest, if they'd
  // like. However, The current EngineConverter's architecture is too
  // complicated and has too many limitations to ensure the full state
  // transition. In order to support "skipping suggestion" for the performance
  // without current client's breakage in short period, this flag is
  // introduced.
  // TODO(hidehiko,komatsu): Remove this flag, when the full EngineConverter
  //   refactoring is done and gets safer for future development/extensions.
  bool request_suggestion;
};

// Class handling ConverterInterface with a session state.  This class
// support stateful operations related with the converter.
class EngineConverterInterface {
 public:
  EngineConverterInterface() = default;
  EngineConverterInterface(const EngineConverterInterface &) = delete;
  EngineConverterInterface &operator=(const EngineConverterInterface &) =
      delete;
  virtual ~EngineConverterInterface() = default;

  typedef int States;
  enum State {
    NO_STATE = 0,
    COMPOSITION = 1,
    SUGGESTION = 2,
    PREDICTION = 4,
    CONVERSION = 8,
  };

  // Check if the current state is in the state bitmap.
  virtual bool CheckState(States) const = 0;

  // Indicate if the conversion session is active or not.  In general,
  // Convert functions make it active and Cancel, Reset and Commit
  // functions make it deactive.
  virtual bool IsActive() const = 0;

  // Return the default conversion preferences to be used for custom
  // conversion.
  virtual const ConversionPreferences &conversion_preferences() const = 0;

  // Send a conversion request to the converter.
  virtual bool Convert(const composer::Composer &composer) = 0;
  virtual bool ConvertWithPreferences(
      const composer::Composer &composer,
      const ConversionPreferences &preferences) = 0;

  // Get reading text (e.g. from "猫" to "ねこ").
  virtual bool GetReadingText(absl::string_view str, std::string *reading) = 0;

  // Send a transliteration request to the converter.
  virtual bool ConvertToTransliteration(
      const composer::Composer &composer,
      transliteration::TransliterationType type) = 0;

  // Convert the current composition to half-width characters.
  // NOTE(komatsu): This function might be merged to ConvertToTransliteration.
  virtual bool ConvertToHalfWidth(const composer::Composer &composer) = 0;

  // Switch the composition to Hiragana, full-width Katakana or
  // half-width Katakana by rotation.
  virtual bool SwitchKanaType(const composer::Composer &composer) = 0;

  // Send a suggestion request to the converter.
  virtual bool Suggest(const composer::Composer &composer,
                       const commands::Context &context) = 0;
  virtual bool SuggestWithPreferences(
      const composer::Composer &composer, const commands::Context &context,
      const ConversionPreferences &preferences) = 0;

  // Send a prediction request to the converter.
  virtual bool Predict(const composer::Composer &composer) = 0;
  virtual bool PredictWithPreferences(
      const composer::Composer &composer,
      const ConversionPreferences &preferences) = 0;

  // Clear conversion segments, but keep the context.
  virtual void Cancel() = 0;

  // Clear conversion segments and the context.
  virtual void Reset() = 0;

  // Fix the conversion with the current status.
  virtual void Commit(const composer::Composer &composer,
                      const commands::Context &context) = 0;

  // Fix the suggestion candidate.  True is returned if the selected
  // candidate is successfully committed.
  virtual bool CommitSuggestionByIndex(size_t index,
                                       const composer::Composer &composer,
                                       const commands::Context &context,
                                       size_t *committed_key_size) = 0;

  // Select a candidate and commit the selected candidate.  True is
  // returned if the selected candidate is successfully committed.
  virtual bool CommitSuggestionById(int id, const composer::Composer &composer,
                                    const commands::Context &context,
                                    size_t *committed_key_size) = 0;

  // Fix only the conversion of the first segment, and keep the rest.
  // The caller should delete characters from composer based on returned
  // |committed_key_size|.
  virtual void CommitFirstSegment(const composer::Composer &composer,
                                  const commands::Context &context,
                                  size_t *committed_key_size) = 0;

  // Fix only the [0, focused] conversion segments, and keep the rest.
  // The caller should delete characters from composer based on returned
  // |committed_key_size|.
  virtual void CommitHeadToFocusedSegments(const composer::Composer &composer,
                                           const commands::Context &context,
                                           size_t *committed_key_size) = 0;

  // Commit the preedit string represented by Composer.
  virtual void CommitPreedit(const composer::Composer &composer,
                             const commands::Context &context) = 0;

  // Commit prefix of the preedit string represented by Composer.
  // The caller should delete characters from composer based on returned
  // |committed_size|.
  virtual void CommitHead(size_t count, const composer::Composer &composer,
                          size_t *committed_size) = 0;

  // Revert the last "Commit" operation
  virtual void Revert() = 0;

  // Delete candidate from user input history.
  // Try to delete the current selected candidate if |id| is not specified.
  // Returns false if the candidate was not found or deletion failed.
  virtual bool DeleteCandidateFromHistory(std::optional<int> id) = 0;

  // Move the focus of segments.
  virtual void SegmentFocusRight() = 0;
  virtual void SegmentFocusLast() = 0;
  virtual void SegmentFocusLeft() = 0;
  virtual void SegmentFocusLeftEdge() = 0;

  // Resize the focused segment.
  virtual void SegmentWidthExpand(const composer::Composer &composer) = 0;
  virtual void SegmentWidthShrink(const composer::Composer &composer) = 0;

  // Move the focus of candidates.
  virtual void CandidateNext(const composer::Composer &composer) = 0;
  virtual void CandidateNextPage() = 0;
  virtual void CandidatePrev() = 0;
  virtual void CandidatePrevPage() = 0;
  // Move the focus to the candidate represented by the id.
  virtual void CandidateMoveToId(int id,
                                 const composer::Composer &composer) = 0;
  // Move the focus to the index from the beginning of the current page.
  virtual void CandidateMoveToPageIndex(size_t index) = 0;
  // Move the focus to the candidate represented by the shortcut.  If
  // the shortcut is not bound with any candidate, false is returned.
  virtual bool CandidateMoveToShortcut(char shortcut) = 0;

  // Operation for the candidate list.
  virtual void SetCandidateListVisible(bool visible) = 0;

  // Fill protocol buffers and update internal status.
  virtual void PopOutput(const composer::Composer &composer,
                         commands::Output *output) = 0;

  // Fill preedit
  virtual void FillPreedit(const composer::Composer &composer,
                           commands::Preedit *preedit) const = 0;

  // Fill protocol buffers
  virtual void FillOutput(const composer::Composer &composer,
                          commands::Output *output) const = 0;

  // Set setting by the request.
  // Currently this is especially for EngineConverter.
  virtual void SetRequest(const commands::Request &request) = 0;

  // Set setting by the config.
  // Currently this is especially for EngineConverter.
  virtual void SetConfig(const config::Config &config) = 0;

  // Update the internal state by the context.
  virtual void OnStartComposition(const commands::Context &context) = 0;

  // Clone instance.
  // Callee object doesn't have the ownership of the cloned instance.
  virtual EngineConverterInterface *Clone() const = 0;

  virtual void set_selection_shortcut(
      config::Config::SelectionShortcut selection_shortcut) = 0;

  virtual void set_use_cascading_window(bool use_cascading_window) = 0;
};

}  // namespace engine
}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_CONVERTER_INTERFACE_H_

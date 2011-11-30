// Copyright 2010-2011, Google Inc.
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

#ifndef MOZC_SESSION_SESSION_CONVERTER_INTERFACE_H_
#define MOZC_SESSION_SESSION_CONVERTER_INTERFACE_H_

#include <string>

#include "base/base.h"
#include "converter/segments.h"
#include "composer/composer.h"
#include "session/commands.pb.h"

namespace mozc {
class ConverterInterface;

namespace session {
class CandidateList;

struct ConversionPreferences {
  bool use_history;
  int max_history_size;
};

// Do not forget to update SessionConverter::SetOperationPreferences()
// when update this struct.
struct OperationPreferences {
  bool use_cascading_window;
  string candidate_shortcuts;
  OperationPreferences() {
#ifdef OS_LINUX
    // TODO(komatsu): Move this logic to the client code.
    use_cascading_window = false;
#else
    use_cascading_window = true;
#endif
  }
};

// Class handling ConverterInterface with a session state.  This class
// support stateful operations related with the converter.
class SessionConverterInterface {
 public:
  SessionConverterInterface() {}
  virtual ~SessionConverterInterface() {}

  // Update OperationPreferences.
  virtual void SetOperationPreferences(const OperationPreferences &preferences)
      ABSTRACT;

  typedef int States;
  enum State {
    NO_STATE = 0,
    COMPOSITION = 1,
    SUGGESTION = 2,
    PREDICTION = 4,
    CONVERSION = 8,
  };

  // Check if the current state is in the state bitmap.
  virtual bool CheckState(States) const ABSTRACT;

  // Indicate if the conversion session is active or not.  In general,
  // Convert functions make it active and Cancel, Reset and Commit
  // functions make it deactive.
  virtual bool IsActive() const ABSTRACT;

  // Return the default conversion preferences to be used for custom
  // conversion.
  virtual const ConversionPreferences &conversion_preferences() const ABSTRACT;

  // Send a conversion request to the converter.
  virtual bool Convert(const composer::Composer &composer) ABSTRACT;
  virtual bool ConvertWithPreferences(
      const composer::Composer &composer,
      const ConversionPreferences &preferences) ABSTRACT;

  // Get reading text (e.g. from "猫" to "ねこ").
  virtual bool GetReadingText(const string &str, string *reading) ABSTRACT;

  // Send a transliteration request to the converter.
  virtual bool ConvertToTransliteration(
      const composer::Composer &composer,
      transliteration::TransliterationType type) ABSTRACT;

  // Convert the current composition to half-width characters.
  // NOTE(komatsu): This function might be merged to ConvertToTransliteration.
  virtual bool ConvertToHalfWidth(const composer::Composer &composer) ABSTRACT;

  // Switch the composition to Hiragana, full-width Katakana or
  // half-width Katakana by rotation.
  virtual bool SwitchKanaType(const composer::Composer &composer) ABSTRACT;

  // Send a suggestion request to the converter.
  virtual bool Suggest(const composer::Composer &composer) ABSTRACT;
  virtual bool SuggestWithPreferences(
      const composer::Composer &composer,
      const ConversionPreferences &preferences) ABSTRACT;

  // Send a prediction request to the converter.
  virtual bool Predict(const composer::Composer &composer) ABSTRACT;
  virtual bool PredictWithPreferences(
      const composer::Composer &composer,
      const ConversionPreferences &preferences) ABSTRACT;

  // Send a prediction request to the converter.
  // The result is added at the tail of existing candidate list as "suggestion"
  // candidates.
  virtual bool ExpandSuggestion(const composer::Composer &composer) ABSTRACT;
  virtual bool ExpandSuggestionWithPreferences(
      const composer::Composer &composer,
      const ConversionPreferences &preferences) ABSTRACT;

  // Clear conversion segments, but keep the context.
  virtual void Cancel() ABSTRACT;

  // Clear conversion segments and the context.
  virtual void Reset() ABSTRACT;

  // Fix the conversion with the current status.
  virtual void Commit() ABSTRACT;

  // Fix the suggestion candidate.  True is returned if teh selected
  // candidate is successfully commited.
  virtual bool CommitSuggestionByIndex(
      size_t index,
      const composer::Composer &composer,
      size_t *committed_key_size) ABSTRACT;

  // Select a candidate and commit the selected candidate.  True is
  // returned if teh selected candidate is successfully commited.
  virtual bool CommitSuggestionById(
      int id,
      const composer::Composer &composer,
      size_t *committed_key_size) ABSTRACT;

  // Fix only the conversion of the first segment, and keep the rest.
  // The caller should delete characters from composer based on returned
  // |committed_key_size|.
  virtual void CommitFirstSegment(size_t *committed_key_size) ABSTRACT;

  // Commit the preedit string represented by Composer.
  virtual void CommitPreedit(const composer::Composer &composer) ABSTRACT;

  // Commit prefix of the preedit string represented by Composer.
  // The caller should delete characters from composer based on returned
  // |commited_size|.
  virtual void CommitHead(size_t count,
                          const composer::Composer &composer,
                          size_t *commited_size) ABSTRACT;

  // Revert the last "Commit" operation
  virtual void Revert() ABSTRACT;

  // Move the focus of segments.
  virtual void SegmentFocusRight() ABSTRACT;
  virtual void SegmentFocusLast() ABSTRACT;
  virtual void SegmentFocusLeft() ABSTRACT;
  virtual void SegmentFocusLeftEdge() ABSTRACT;

  // Resize the focused segment.
  virtual void SegmentWidthExpand() ABSTRACT;
  virtual void SegmentWidthShrink() ABSTRACT;

  // Move the focus of candidates.
  virtual void CandidateNext(const composer::Composer &composer) ABSTRACT;
  virtual void CandidateNextPage() ABSTRACT;
  virtual void CandidatePrev() ABSTRACT;
  virtual void CandidatePrevPage() ABSTRACT;
  // Move the focus to the candidate represented by the id.
  virtual void CandidateMoveToId(
      int id, const composer::Composer &composer) ABSTRACT;
  // Move the focus to the index from the beginning of the current page.
  virtual void CandidateMoveToPageIndex(size_t index) ABSTRACT;
  // Move the focus to the candidate represented by the shortcut.  If
  // the shortcut is not bound with any candidate, false is returned.
  virtual bool CandidateMoveToShortcut(char shortcut) ABSTRACT;

  // Operation for the candidate list.
  virtual bool IsCandidateListVisible() const ABSTRACT;
  virtual void SetCandidateListVisible(bool visible) ABSTRACT;

  // Fill protocol buffers and update internal status.
  virtual void PopOutput(
      const composer::Composer &composer, commands::Output *output) ABSTRACT;

  // Fill protocol buffers
  virtual void FillOutput(
      const composer::Composer &composer,
      commands::Output *output) const ABSTRACT;

  // Fill context information
  virtual void FillContext(commands::Context *context) const ABSTRACT;

  // Get/Set segments
  virtual void GetSegments(Segments *dest) const ABSTRACT;
  virtual void SetSegments(const Segments &src) ABSTRACT;

  // Remove tail part of history segments
  virtual void RemoveTailOfHistorySegments(size_t num_of_characters) ABSTRACT;

  // Accessor
  virtual const commands::Result &GetResult() const ABSTRACT;
  virtual const CandidateList &GetCandidateList() const ABSTRACT;
  virtual const OperationPreferences &GetOperationPreferences() const ABSTRACT;
  virtual State GetState() const ABSTRACT;
  virtual size_t GetSegmentIndex() const ABSTRACT;
  virtual const Segment &GetPreviousSuggestions()
      const ABSTRACT;

  virtual void CopyFrom(const SessionConverterInterface &src) ABSTRACT;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionConverterInterface);
};
}  // namespace session
}  // namespace mozc
#endif  // MOZC_SESSION_SESSION_CONVERTER_INTERFACE_H_

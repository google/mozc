// Copyright 2010, Google Inc.
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

#ifndef MOZC_SESSION_SESSION_CONVERTER_H_
#define MOZC_SESSION_SESSION_CONVERTER_H_

#include <string>

#include "session/session_converter_interface.h"

namespace mozc {
namespace session {
class CandidateList;

// Class handling ConverterInterface with a session state.  This class
// support stateful operations related with the converter.
class SessionConverter : public SessionConverterInterface {
 public:
  explicit SessionConverter(const ConverterInterface *converter);
  virtual ~SessionConverter();

  // Reload the global configuration variables.
  void ReloadConfig();
  // Update the configuration.
  void UpdateConfig(const config::Config &config);

  typedef int States;
  enum State {
    NO_STATE = 0,
    COMPOSITION = 1,
    SUGGESTION = 2,
    PREDICTION = 4,
    CONVERSION = 8,
  };

  // Check if the current state is in the state bitmap.
  bool CheckState(States) const;

  // Indicate if the conversion session is active or not.  In general,
  // Convert functions make it active and Cancel, Reset and Commit
  // functions make it deactive.
  bool IsActive() const;

  // Return the default conversion preferences to be used for custom
  // conversion.
  const ConversionPreferences &conversion_preferences() const;

  // Send a conversion request to the converter.
  bool Convert(const composer::Composer *composer);
  bool ConvertWithPreferences(const composer::Composer *composer,
                              const ConversionPreferences &preferences);

  // Send a transliteration request to the converter.
  bool ConvertToTransliteration(const composer::Composer *composer,
                                transliteration::TransliterationType type);

  // Convert the current composition to half-width characters.
  // NOTE(komatsu): This function might be merged to ConvertToTransliteration.
  bool ConvertToHalfWidth(const composer::Composer *composer);

  // Switch the composition to Hiragana, full-width Katakana or
  // half-width Katakana by rotation.
  bool SwitchKanaType(const composer::Composer *composer);

  // Send a suggestion request to the converter.
  bool Suggest(const composer::Composer *composer);
  bool SuggestWithPreferences(const composer::Composer *composer,
                              const ConversionPreferences &preferences);

  // Send a prediction request to the converter.
  bool Predict(const composer::Composer *composer);
  bool PredictWithPreferences(const composer::Composer *composer,
                              const ConversionPreferences &preferences);

  // Clear conversion segments, but keep the context.
  void Cancel();

  // Clear conversion segments and the context.
  void Reset();

  // Fix the conversion with the current status.
  void Commit();

  // Fix the suggestion candidate.
  void CommitSuggestion(size_t index);

  // Fix only the conversion of the first segment, and keep the rest.
  void CommitFirstSegment(composer::Composer *composer);

  // Commit the preedit string represented by Composer.
  void CommitPreedit(const composer::Composer &composer);

  // Revert the last "Commit" operation
  void Revert();

  // Move the focus of segments.
  void SegmentFocusRight();
  void SegmentFocusLast();
  void SegmentFocusLeft();
  void SegmentFocusLeftEdge();

  // Resize the focused segment.
  void SegmentWidthExpand();
  void SegmentWidthShrink();

  // Move the focus of candidates.
  void CandidateNext();
  void CandidateNextPage();
  void CandidatePrev();
  void CandidatePrevPage();
  // Move the focus to the candidate represented by the id.
  void CandidateMoveToId(int id);
  // Move the focus to the index from the beginning of the current page.
  void CandidateMoveToPageIndex(size_t index);
  // Move the focus to the candidate represented by the shortcut.  If
  // the shortcut is not bound with any candidate, false is returned.
  bool CandidateMoveToShortcut(char shortcut);

  // Operation for the candidate list.
  bool IsCandidateListVisible() const;
  void SetCandidateListVisible(bool visible);

  // Fill protocol buffers and update the internal status.
  void PopOutput(commands::Output *output);

  // Fill protocol buffers
  void FillOutput(commands::Output *output) const;

  // Fill protocol buffers with all flatten candidate words.
  void FillAllCandidateWords(commands::CandidateList *candidates) const;

  const string &GetDefaultResult() const;

  // Fill segments with the conversion preferences.
  static void SetConversionPreferences(
      const ConversionPreferences &preferences,
      Segments *segments);

 private:
  // Reset the result value stored at the previous command.
  void ResetResult();

  // Reset the session state variables.
  void ResetState();

  // Notify the converter that the current segment is focused.
  void SegmentFocus();

  // Notify the converter that the current segment is fixed.
  void SegmentFix();

  // Get preedit and conversion from segment(index) to segment(index + size).
  void GetPreeditAndConversion(size_t index, size_t size,
                               string *preedit, string *conversion) const;

  // Update internal states
  void UpdateResult(size_t index, size_t size);
  void UpdateCandidateList();
  void InitSegment(const size_t segment_index);

  // Return the candidate index to be used by the converter.
  int GetCandidateIndexForConverter(const size_t segment_index) const;

  // if focus_id is pointing to the last of suggestions,
  // call StartPrediction().
  void MaybeExpandPrediction();

  // Return the candidate to be used by the converter.
  const Segment::Candidate &GetSelectedCandidate(size_t segment_index) const;

  void FillConversion(commands::Preedit *preedit) const;
  void FillResult(commands::Result *result) const;
  void FillCandidates(commands::Candidates *candidates) const;

  State state_;

  bool active_;

  const composer::Composer *composer_;
  const ConverterInterface *converter_;
  scoped_ptr<Segments> segments_;
  size_t segment_index_;

  // Previous suggestions to be merged with the current predictions.
  vector<Segment::Candidate> previous_suggestions_;

  // Default conversion preferences.
  ConversionPreferences conversion_preferences_;

  // Preferences for user's operation.
  OperationPreferences operation_preferences_;

  string composition_;
  commands::Result result_;

  // Default result the converter generated without user's
  // modifactions.  Note, this result is considered with user's
  // histroy when ConversionPreferences::use_history is true.
  string default_result_;

  scoped_ptr<CandidateList> candidate_list_;
  bool candidate_list_visible_;

  DISALLOW_COPY_AND_ASSIGN(SessionConverter);
};
}  // namespace session
}  // namespace mozc
#endif  // MOZC_SESSION_SESSION_CONVERTER_H_

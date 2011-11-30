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

#ifndef MOZC_SESSION_SESSION_CONVERTER_H_
#define MOZC_SESSION_SESSION_CONVERTER_H_

#include <string>
#include <vector>

#include "session/commands.pb.h"
#include "session/session_converter_interface.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace session {
class CandidateList;

// Class handling ConverterInterface with a session state.  This class
// support stateful operations related with the converter.
class SessionConverter : public SessionConverterInterface {
 public:
  explicit SessionConverter(const ConverterInterface *converter);
  virtual ~SessionConverter();

  // Update OperationPreferences.
  void SetOperationPreferences(const OperationPreferences &preferences);

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
  bool Convert(const composer::Composer &composer);
  bool ConvertWithPreferences(const composer::Composer &composer,
                              const ConversionPreferences &preferences);

  // Get reading text (e.g. from "猫" to "ねこ").
  bool GetReadingText(const string &source_text, string *reading);

  // Send a transliteration request to the converter.
  bool ConvertToTransliteration(const composer::Composer &composer,
                                transliteration::TransliterationType type);

  // Convert the current composition to half-width characters.
  // NOTE(komatsu): This function might be merged to ConvertToTransliteration.
  bool ConvertToHalfWidth(const composer::Composer &composer);

  // Switch the composition to Hiragana, full-width Katakana or
  // half-width Katakana by rotation.
  bool SwitchKanaType(const composer::Composer &composer);

  // Send a suggestion request to the converter.
  bool Suggest(const composer::Composer &composer);
  bool SuggestWithPreferences(const composer::Composer &composer,
                              const ConversionPreferences &preferences);

  // Send a prediction request to the converter.
  bool Predict(const composer::Composer &composer);
  bool PredictWithPreferences(const composer::Composer &composer,
                              const ConversionPreferences &preferences);

  // Send a prediction request to the converter.
  // The result is added at the tail of existing candidate list as "suggestion"
  // candidates.
  bool ExpandSuggestion(const composer::Composer &composer);
  bool ExpandSuggestionWithPreferences(
      const composer::Composer &composer,
      const ConversionPreferences &preferences);

  // Clear conversion segments, but keep the context.
  void Cancel();

  // Clear conversion segments and the context.
  void Reset();

  // Fix the conversion with the current status.
  void Commit();

  // Fix the suggestion candidate. Stores the number of characters in the key
  // of the committed candidate to committed_key_size.
  // For example, assume that "日本語" was suggested as a candidate for "にほ".
  // The key of the candidate is "にほんご". Therefore, when the candidate is
  // committed by this function, committed_key_size will be set to 4.
  // True is returned if the suggestion is successfully committed.
  bool CommitSuggestionByIndex(size_t index,
                               const composer::Composer &composer,
                               size_t *committed_key_size);

  // Select a candidate and commit the selected candidate.  True is
  // returned if the suggestion is successfully committed.
  bool CommitSuggestionById(int id,
                            const composer::Composer &composer,
                            size_t *committed_key_size);

  // Fix only the conversion of the first segment, and keep the rest.
  // The caller should delete characters from composer based on returned
  // |committed_key_size|.
  // If |committed_key_size| is 0, this means that there is only a segment
  // so Commit() method is called instead. In this case, the caller
  // should not delete any characters.
  void CommitFirstSegment(size_t *committed_key_size);

  // Commit the preedit string represented by Composer.
  void CommitPreedit(const composer::Composer &composer);

  // Commit the specified number of characters at the head of the preedit
  // string represented by Composer.
  // The caller should delete characters from composer based on returned
  // |committed_size|.
  // TODO(yamaguchi): Enhance to support the conversion mode.
  void CommitHead(size_t count,
                  const composer::Composer &composer,
                  size_t *committed_size);

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
  void CandidateNext(const composer::Composer &composer);
  void CandidateNextPage();
  void CandidatePrev();
  void CandidatePrevPage();
  // Move the focus to the candidate represented by the id.
  void CandidateMoveToId(int id, const composer::Composer &composer);
  // Move the focus to the index from the beginning of the current page.
  void CandidateMoveToPageIndex(size_t index);
  // Move the focus to the candidate represented by the shortcut.  If
  // the shortcut is not bound with any candidate, false is returned.
  bool CandidateMoveToShortcut(char shortcut);

  // Operation for the candidate list.
  bool IsCandidateListVisible() const;
  void SetCandidateListVisible(bool visible);

  // Fill protocol buffers and update the internal status.
  void PopOutput(const composer::Composer &composer, commands::Output *output);

  // Fill protocol buffers
  void FillOutput(
      const composer::Composer &composer, commands::Output *output) const;

  // Fill context information
  void FillContext(commands::Context *context) const;

  // Get/Set segments
  void GetSegments(Segments *dest) const;
  void SetSegments(const Segments &src);

  // Remove tail part of history segments
  void RemoveTailOfHistorySegments(size_t num_of_characters);

  // Fill protocol buffers with all flatten candidate words.
  void FillAllCandidateWords(commands::CandidateList *candidates) const;

  // Accessor
  const commands::Result &GetResult() const;
  const CandidateList &GetCandidateList() const;
  const OperationPreferences &GetOperationPreferences() const;
  SessionConverterInterface::State GetState() const;
  size_t GetSegmentIndex() const;
  const Segment &GetPreviousSuggestions() const;

  // Fill segments with the conversion preferences.
  static void SetConversionPreferences(
      const ConversionPreferences &preferences,
      Segments *segments);

  // Copy SessionConverter
  // TODO(hsumita): Copy all member variables.
  // Currently, converter_ is not copied.
  void CopyFrom(const SessionConverterInterface &src);

 private:
  FRIEND_TEST(SessionConverterTest, AppendCandidateList);
  FRIEND_TEST(SessionConverterTest, GetPreeditAndGetConversion);
  FRIEND_TEST(SessionConverterTest, PartialSuggestion);

  // Reset the result value stored at the previous command.
  void ResetResult();

  // Reset the session state variables.
  void ResetState();

  // Notify the converter that the current segment is focused.
  void SegmentFocus();

  // Notify the converter that the current segment is fixed.
  void SegmentFix();

  // Get preedit from segment(index) to segment(index + size).
  void GetPreedit(size_t index, size_t size, string *preedit) const;
  // Get conversion from segment(index) to segment(index + size).
  void GetConversion(size_t index, size_t size, string *conversion) const;

  // Perform the command if the command candidate is selected.  True
  // is returned if a command is performed.
  bool MaybePerformCommandCandidate(size_t index, size_t size) const;

  // Update internal states
  bool UpdateResult(size_t index, size_t size);

  // Fill the candidate list with the focused segment's candidates.
  // This method does not clear the candidate list before processing.
  // Only the candidates of which id is not existent in the candidate list
  // are appended. Other candidates are ignored.
  void AppendCandidateList();
  // Clear the candidate list and fill it with the focused segment's candidates.
  void UpdateCandidateList();

  // Return the candidate index to be used by the converter.
  int GetCandidateIndexForConverter(const size_t segment_index) const;

  // if focus_id is pointing to the last of suggestions,
  // call StartPrediction().
  void MaybeExpandPrediction(const composer::Composer &composer);

  // Return the value of candidate to be used by the converter.
  string GetSelectedCandidateValue(size_t segment_index) const;

  // Return the candidate to be used by the converter.
  const Segment::Candidate &GetSelectedCandidate(size_t segment_index) const;

  // Returns the length of committed candidate's key in characters.
  // True is returned if the selected candidate is successfully committed.
  bool CommitSuggestionInternal(const composer::Composer &composer,
                                size_t *committed_length);

  void FillConversion(commands::Preedit *preedit) const;
  void FillResult(commands::Result *result) const;
  void FillCandidates(commands::Candidates *candidates) const;

  bool IsEmptySegment(const Segment &segment) const;

  SessionConverterInterface::State state_;

  const ConverterInterface *converter_;
  scoped_ptr<Segments> segments_;
  size_t segment_index_;

  // Previous suggestions to be merged with the current predictions.
  Segment previous_suggestions_;

  // Default conversion preferences.
  ConversionPreferences conversion_preferences_;

  // Preferences for user's operation.
  OperationPreferences operation_preferences_;

  commands::Result result_;

  scoped_ptr<CandidateList> candidate_list_;
  bool candidate_list_visible_;

  DISALLOW_COPY_AND_ASSIGN(SessionConverter);
};

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_CONVERTER_H_
